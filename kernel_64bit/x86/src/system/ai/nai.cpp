#include "nai.hpp"

#include "system/filesystem/iso9660/iso9660.hpp"
#include "system/scheduler/scheduler.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "drivers/timer/driver.hpp"

#include <string.h>

#if defined(__x86_64__)
// Pull in only the intrinsic headers needed for the AVX2 kernels. The
// umbrella immintrin.h also includes AVX-512 headers that do not compile in
// this freestanding environment, and mm_malloc.h (reached via xmmintrin.h)
// requires stdlib.h, which does not exist here; _mm_malloc is unused.
#define _MM_MALLOC_H_INCLUDED
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#define _IMMINTRIN_H_INCLUDED
#include <avxintrin.h>
#include <avx2intrin.h>
#include <fmaintrin.h>
#endif

namespace NasuaAI
{
    namespace
    {
        constexpr uint32_t EMBED = 576;
        constexpr uint32_t FF = 1536;
        constexpr uint32_t KV_EMBED = 192;
        constexpr uint32_t HEAD_COUNT = 9;
        constexpr uint32_t KV_HEAD_COUNT = 3;
        constexpr uint32_t HEAD_DIM = 64;
        constexpr uint32_t LAYER_COUNT = 30;
        constexpr uint32_t VOCAB_SIZE = 49152;
        constexpr uint32_t CONTEXT_SIZE = 512;
        constexpr uint32_t MAX_METADATA_SIZE = 2 * 1024 * 1024;
        constexpr uint32_t MERGE_COUNT_MAX = 50000;
        constexpr uint32_t VOCAB_HASH_SIZE = 131072;
        constexpr uint32_t MERGE_HASH_SIZE = 131072;

        constexpr char MODEL_PATH[] = "/system/NAI/SMOLLM2.GG";
        constexpr uint32_t GGML_TYPE_F32 = 0;
        constexpr uint32_t GGML_TYPE_Q8_0 = 8;
        constexpr uint32_t GGML_TYPE_Q3_K = 11;
        constexpr uint32_t GGML_TYPE_IQ4_NL = 20;
        constexpr uint32_t GGML_TYPE_IQ4_XS = 23;

        struct tensor_info
        {
            char name[64];
            uint32_t dims[2];
            uint32_t type;
            uint64_t relative_offset;
            uint64_t f32_offset;
        };

        struct vocab_entry
        {
            const char* text;
            uint32_t length;
        };

        struct merge_text_entry
        {
            const char* text;
            uint32_t length;
        };

        struct merge_entry
        {
            uint32_t left;
            uint32_t right;
            uint32_t merged;
            uint32_t rank;
        };

        static bool initialized = false;
        static bool init_failed = false;
        static uint8_t* metadata = nullptr;
        static uint32_t metadata_size = 0;
        static uint8_t* model_data = nullptr;
        static uint32_t model_data_size = 0;
        static float* weights_f32 = nullptr;
        static uint32_t tensor_count = 0;
        static uint32_t vocab_count = 0;
        static uint32_t merge_count = 0;
        static tensor_info tensors[272];
        static vocab_entry vocab[VOCAB_SIZE];
        static merge_text_entry merge_texts[MERGE_COUNT_MAX];
        static merge_entry merges[MERGE_COUNT_MAX];
        static uint32_t vocab_hash_table[VOCAB_HASH_SIZE];
        static uint32_t merge_hash_table[MERGE_HASH_SIZE];
        static uint32_t bos_token = 1;
        static uint32_t eos_token = 2;
        static uint32_t pad_token = 2;
        static float* key_cache = nullptr;
        static float* value_cache = nullptr;

        struct layer_tensor_set
        {
            tensor_info* attn_norm;
            tensor_info* attn_q;
            tensor_info* attn_k;
            tensor_info* attn_v;
            tensor_info* attn_output;
            tensor_info* ffn_norm;
            tensor_info* ffn_gate;
            tensor_info* ffn_up;
            tensor_info* ffn_down;
        };

        struct inference_buffers
        {
            float* hidden = nullptr;
            float* norm = nullptr;
            float* q = nullptr;
            float* k = nullptr;
            float* v = nullptr;
            float* attn = nullptr;
            float* gate = nullptr;
            float* up = nullptr;
            float* logits = nullptr;

            ~inference_buffers()
            {
                if (hidden != nullptr) Memory::heap::kfree(hidden);
                if (norm != nullptr) Memory::heap::kfree(norm);
                if (q != nullptr) Memory::heap::kfree(q);
                if (k != nullptr) Memory::heap::kfree(k);
                if (v != nullptr) Memory::heap::kfree(v);
                if (attn != nullptr) Memory::heap::kfree(attn);
                if (gate != nullptr) Memory::heap::kfree(gate);
                if (up != nullptr) Memory::heap::kfree(up);
                if (logits != nullptr) Memory::heap::kfree(logits);
            }
        };

        static layer_tensor_set layer_tensors[LAYER_COUNT];
        static tensor_info* token_embd_tensor = nullptr;
        static tensor_info* output_norm_tensor = nullptr;
        static bool use_avx2_fma = false;
        static bool use_sse_simd = false;
        static uint32_t last_generated_tokens = 0;
        static uint64_t last_elapsed_ms = 0;
        static bool last_stats_valid = false;

        constexpr uint32_t JOB_RESULT_SIZE = 1024;
        constexpr uint32_t JOB_PROMPT_SIZE = 512;

        enum job_state : uint32_t
        {
            JOB_IDLE = 0,
            JOB_RUNNING = 1,
            JOB_DONE = 2,
            JOB_FAILED = 3
        };

        struct generation_job
        {
            char prompt[JOB_PROMPT_SIZE];
            char result[JOB_RESULT_SIZE];
            uint32_t max_tokens;
            uint32_t id;
            volatile uint32_t state;
        };

        static generation_job current_job = {};
        static volatile bool job_active = false;
        static uint32_t next_job_id = 1;

        static bool fail(const char* stage)
        {
            Uart::puts("[NAI] Failed: ");
            Uart::puts(stage);
            Uart::puts("\n");
            init_failed = true;
            return false;
        }

        static uint16_t read_u16(const uint8_t* data)
        {
            return static_cast<uint16_t>(data[0]) |
                   (static_cast<uint16_t>(data[1]) << 8);
        }

        static uint32_t read_u32(const uint8_t* data)
        {
            return static_cast<uint32_t>(data[0]) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   (static_cast<uint32_t>(data[2]) << 16) |
                   (static_cast<uint32_t>(data[3]) << 24);
        }

        static uint64_t read_u64(const uint8_t* data)
        {
            uint64_t low = read_u32(data);
            uint64_t high = read_u32(data + 4);
            return low | (high << 32);
        }

        static float half_to_float(uint16_t value)
        {
            uint32_t sign = (value & 0x8000) << 16;
            uint32_t exponent = (value >> 10) & 0x1F;
            uint32_t fraction = value & 0x03FF;
            uint32_t bits;
            if (exponent == 0)
            {
                if (fraction == 0)
                {
                    bits = sign;
                }
                else
                {
                    int e = -14;
                    uint32_t mantissa = fraction;
                    while ((mantissa & 0x400) == 0)
                    {
                        mantissa <<= 1;
                        --e;
                    }
                    mantissa &= 0x3FF;
                    bits = sign | static_cast<uint32_t>(e + 127) << 23 |
                           mantissa << 13;
                }
            }
            else if (exponent == 31)
            {
                bits = sign | 0x7F800000 | (fraction << 13);
            }
            else
            {
                bits = sign | static_cast<uint32_t>(exponent + 112) << 23 |
                       fraction << 13;
            }
            float result;
            memcpy(&result, &bits, sizeof(result));
            return result;
        }

        struct gguf_reader
        {
            const uint8_t* data;
            uint32_t size;
            uint32_t cursor;
        };

        static bool read_bytes(gguf_reader* reader, void* output,
                              uint32_t length)
        {
            if (reader == nullptr || reader->cursor > reader->size ||
                length > reader->size - reader->cursor)
            {
                return false;
            }
            if (length != 0)
            {
                memcpy(output, reader->data + reader->cursor, length);
            }
            reader->cursor += length;
            return true;
        }

        static bool read_u32_reader(gguf_reader* reader, uint32_t* value)
        {
            uint8_t bytes[4];
            if (!read_bytes(reader, bytes, sizeof(bytes)))
            {
                return false;
            }
            *value = read_u32(bytes);
            return true;
        }

        static bool read_u64_reader(gguf_reader* reader, uint64_t* value)
        {
            uint8_t bytes[8];
            if (!read_bytes(reader, bytes, sizeof(bytes)))
            {
                return false;
            }
            *value = read_u64(bytes);
            return true;
        }

        static bool read_string(gguf_reader* reader, char* output,
                                uint32_t capacity)
        {
            uint64_t length;
            if (!read_u64_reader(reader, &length) || length >= capacity)
            {
                return false;
            }
            if (!read_bytes(reader, output, static_cast<uint32_t>(length)))
            {
                return false;
            }
            output[length] = '\0';
            return true;
        }

        static bool skip_array(gguf_reader* reader)
        {
            uint32_t element_type;
            uint64_t count;
            if (!read_u32_reader(reader, &element_type) ||
                !read_u64_reader(reader, &count))
            {
                return false;
            }
            uint32_t element_size = 0;
            switch (element_type)
            {
                case 0: case 1: case 7: element_size = 1; break;
                case 2: case 3: element_size = 2; break;
                case 4: case 5: case 6: element_size = 4; break;
                case 10: case 11: case 12: element_size = 8; break;
                default: break;
            }
            if (element_type == 8)
            {
                for (uint64_t index = 0; index < count; ++index)
                {
                    uint64_t length;
                    if (!read_u64_reader(reader, &length) ||
                        length > reader->size - reader->cursor)
                    {
                        return false;
                    }
                    reader->cursor += static_cast<uint32_t>(length);
                }
                return true;
            }
            if (element_size == 0 ||
                count > (reader->size - reader->cursor) / element_size)
            {
                return false;
            }
            reader->cursor += static_cast<uint32_t>(count * element_size);
            return true;
        }

        static bool skip_metadata_value(gguf_reader* reader, uint32_t type)
        {
            uint8_t bytes[8];
            switch (type)
            {
                case 0: case 1: case 7:
                    return read_bytes(reader, bytes, 1);
                case 2: case 3:
                    return read_bytes(reader, bytes, 2);
                case 4: case 5: case 6:
                    return read_bytes(reader, bytes, 4);
                case 8:
                {
                    uint64_t length;
                    if (!read_u64_reader(reader, &length) ||
                        length > reader->size - reader->cursor)
                    {
                        return false;
                    }
                    reader->cursor += static_cast<uint32_t>(length);
                    return true;
                }
                case 9:
                    return skip_array(reader);
                case 10: case 11: case 12:
                    return read_bytes(reader, bytes, 8);
                default:
                    return false;
            }
        }

        static bool text_equal(const char* left, const char* right)
        {
            uint32_t index = 0;
            while (left[index] != '\0' && right[index] != '\0')
            {
                if (left[index] != right[index])
                {
                    return false;
                }
                ++index;
            }
            return left[index] == right[index];
        }

        static tensor_info* find_tensor(const char* name)
        {
            for (uint32_t index = 0; index < tensor_count; ++index)
            {
                if (text_equal(tensors[index].name, name))
                {
                    return &tensors[index];
                }
            }
            return nullptr;
        }

        static uint32_t row_bytes(const tensor_info* tensor)
        {
            const uint32_t elements = tensor->dims[0];
            switch (tensor->type)
            {
                case GGML_TYPE_F32:
                    return elements * 4;
                case GGML_TYPE_Q8_0:
                    return elements / 32 * 34;
                case GGML_TYPE_Q3_K:
                    return elements / 256 * 110;
                case GGML_TYPE_IQ4_NL:
                    return elements / 32 * 18;
                case GGML_TYPE_IQ4_XS:
                    return elements / 256 * 136;
                default:
                    return 0;
            }
        }

        static float sqrt_positive(float value)
        {
            if (value <= 0.0f)
            {
                return 0.0f;
            }
            return __builtin_sqrtf(value);
        }

        static float exp_small(float value)
        {
            if (value < -20.0f)
            {
                return 0.0f;
            }
            if (value > 20.0f)
            {
                return 485165195.0f;
            }
            const float reduced = value * 0.0625f;
            float term = 1.0f;
            float sum = 1.0f;
            for (uint32_t index = 1; index < 15; ++index)
            {
                term *= reduced / static_cast<float>(index);
                sum += term;
            }
            for (uint32_t square = 0; square < 4; ++square)
            {
                sum *= sum;
            }
            return sum;
        }

        static void rms_norm(const float* input, const float* weight,
                             float* output, uint32_t elements)
        {
            float sum = 0.0f;
            for (uint32_t index = 0; index < elements; ++index)
            {
                sum += input[index] * input[index];
            }
            const float scale = 1.0f /
                sqrt_positive(sum / static_cast<float>(elements) + 0.00001f);
            for (uint32_t index = 0; index < elements; ++index)
            {
                output[index] = input[index] * scale * weight[index];
            }
        }

#if defined(__x86_64__)
#define NAI_AVX2_TARGET __attribute__((target("avx2,fma")))
#define NAI_SSE_TARGET __attribute__((target("ssse3,sse4.1")))
#define nai_sse_mul_add_ps(a, b, c) _mm_add_ps(_mm_mul_ps((a), (b)), (c))

        NAI_SSE_TARGET static inline float hsum128_ps(__m128 value)
        {
            __m128 sum = _mm_add_ps(value, _mm_movehl_ps(value, value));
            sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 0x55));
            return _mm_cvtss_f32(sum);
        }

        NAI_SSE_TARGET static inline __m128 dot_i8_4_ps(
            const int8_t* quant, const float* input, __m128 acc)
        {
            int raw;
            memcpy(&raw, quant, sizeof(raw));
            const __m128i weights_i32 =
                _mm_cvtepi8_epi32(_mm_cvtsi32_si128(raw));
            return nai_sse_mul_add_ps(_mm_cvtepi32_ps(weights_i32),
                                _mm_loadu_ps(input), acc);
        }

        NAI_SSE_TARGET static inline __m128 dot_i8_16_ps_sse(
            const __m128i weights, const float* input, __m128 acc)
        {
            const __m128i w0 = weights;
            const __m128i w1 = _mm_srli_si128(weights, 4);
            const __m128i w2 = _mm_srli_si128(weights, 8);
            const __m128i w3 = _mm_srli_si128(weights, 12);
            acc = nai_sse_mul_add_ps(_mm_cvtepi32_ps(_mm_cvtepi8_epi32(w0)),
                               _mm_loadu_ps(input), acc);
            acc = nai_sse_mul_add_ps(_mm_cvtepi32_ps(_mm_cvtepi8_epi32(w1)),
                               _mm_loadu_ps(input + 4), acc);
            acc = nai_sse_mul_add_ps(_mm_cvtepi32_ps(_mm_cvtepi8_epi32(w2)),
                               _mm_loadu_ps(input + 8), acc);
            return nai_sse_mul_add_ps(_mm_cvtepi32_ps(_mm_cvtepi8_epi32(w3)),
                                _mm_loadu_ps(input + 12), acc);
        }

        NAI_SSE_TARGET static inline __m128 iq4nl_group_dot_sse(
            const uint8_t* qs, const float* input)
        {
            const __m128i lut = _mm_setr_epi8(
                -127, -104, -83, -65, -49, -35, -22, -10,
                1, 13, 25, 38, 53, 69, 89, 113);
            const __m128i packed =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(qs));
            const __m128i low_nibbles =
                _mm_and_si128(packed, _mm_set1_epi8(0x0F));
            const __m128i high_nibbles = _mm_and_si128(
                _mm_srli_epi16(packed, 4), _mm_set1_epi8(0x0F));
            const __m128i low_weights =
                _mm_shuffle_epi8(lut, low_nibbles);
            const __m128i high_weights =
                _mm_shuffle_epi8(lut, high_nibbles);
            __m128 acc = _mm_setzero_ps();
            acc = dot_i8_16_ps_sse(low_weights, input, acc);
            return dot_i8_16_ps_sse(high_weights, input + 16, acc);
        }

        NAI_SSE_TARGET static float dequant_f32_dot_sse(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            const float* weights = reinterpret_cast<const float*>(row);
            __m128 acc = _mm_setzero_ps();
            uint32_t index = 0;
            for (; index + 4 <= elements; index += 4)
            {
                acc = nai_sse_mul_add_ps(_mm_loadu_ps(weights + index),
                                   _mm_loadu_ps(input + index), acc);
            }
            float sum = hsum128_ps(acc);
            for (; index < elements; ++index)
            {
                sum += input[index] * weights[index];
            }
            return sum;
        }

        NAI_SSE_TARGET static float dequant_q8_0_dot_sse(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m128 total = _mm_setzero_ps();
            for (uint32_t block = 0; block < elements / 32; ++block)
            {
                const uint8_t* bytes = row + block * 34;
                const __m128 scale = _mm_set1_ps(
                    half_to_float(read_u16(bytes)));
                const int8_t* quant =
                    reinterpret_cast<const int8_t*>(bytes + 2);
                const float* values = input + block * 32;
                __m128 acc = _mm_setzero_ps();
                for (uint32_t index = 0; index < 32; index += 4)
                {
                    acc = dot_i8_4_ps(quant + index, values + index, acc);
                }
                total = nai_sse_mul_add_ps(acc, scale, total);
            }
            return hsum128_ps(total);
        }

        NAI_SSE_TARGET static float dequant_iq4nl_dot_sse(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m128 total = _mm_setzero_ps();
            for (uint32_t block = 0; block < elements / 32; ++block)
            {
                const uint8_t* bytes = row + block * 18;
                const __m128 scale = _mm_set1_ps(
                    half_to_float(read_u16(bytes)));
                total = nai_sse_mul_add_ps(
                    iq4nl_group_dot_sse(bytes + 2,
                                        input + block * 32),
                    scale, total);
            }
            return hsum128_ps(total);
        }

        NAI_SSE_TARGET static float dequant_iq4xs_dot_sse(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m128 total = _mm_setzero_ps();
            for (uint32_t block = 0; block < elements / 256; ++block)
            {
                const uint8_t* bytes = row + block * 136;
                const float d = half_to_float(read_u16(bytes));
                const uint16_t scales_h = read_u16(bytes + 2);
                const uint8_t* scales_l = bytes + 4;
                const uint8_t* qs = bytes + 8;
                const float* input_values = input + block * 256;
                for (uint32_t sub = 0; sub < 8; ++sub)
                {
                    const uint8_t sl = scales_l[sub / 2];
                    const int32_t ls =
                        ((sl >> (4 * (sub % 2))) & 0x0F) |
                        (((scales_h >> (2 * sub)) & 3) << 4);
                    const __m128 scale = _mm_set1_ps(
                        d * static_cast<float>(ls - 32));
                    total = nai_sse_mul_add_ps(
                        iq4nl_group_dot_sse(
                            qs + sub * 16,
                            input_values + sub * 32),
                        scale, total);
                }
            }
            return hsum128_ps(total);
        }

        NAI_AVX2_TARGET static inline float hsum256_ps(__m256 value)
        {
            const __m128 low = _mm256_castps256_ps128(value);
            const __m128 high = _mm256_extractf128_ps(value, 1);
            __m128 sum = _mm_add_ps(low, high);
            sum = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
            sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 0x55));
            return _mm_cvtss_f32(sum);
        }

        NAI_AVX2_TARGET static inline __m256 dot_i8_8_ps(
            const int8_t* quant, const float* input, __m256 acc)
        {
            const __m128i raw = _mm_loadl_epi64(
                reinterpret_cast<const __m128i*>(quant));
            const __m256 weights =
                _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(raw));
            return _mm256_fmadd_ps(weights, _mm256_loadu_ps(input), acc);
        }

        NAI_AVX2_TARGET static inline __m256 dot_i8_16_ps(
            const __m128i weights, const float* input, __m256 acc)
        {
            const __m256i low = _mm256_cvtepi8_epi32(weights);
            const __m256i high =
                _mm256_cvtepi8_epi32(_mm_srli_si128(weights, 8));
            acc = _mm256_fmadd_ps(_mm256_cvtepi32_ps(low),
                                  _mm256_loadu_ps(input), acc);
            return _mm256_fmadd_ps(_mm256_cvtepi32_ps(high),
                                   _mm256_loadu_ps(input + 8), acc);
        }

        NAI_AVX2_TARGET static inline __m256 iq4nl_group_dot_avx2(
            const uint8_t* qs, const float* input)
        {
            const __m128i lut = _mm_setr_epi8(
                -127, -104, -83, -65, -49, -35, -22, -10,
                1, 13, 25, 38, 53, 69, 89, 113);
            const __m128i packed =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(qs));
            const __m128i low_nibbles =
                _mm_and_si128(packed, _mm_set1_epi8(0x0F));
            const __m128i high_nibbles = _mm_and_si128(
                _mm_srli_epi16(packed, 4), _mm_set1_epi8(0x0F));
            const __m128i low_weights =
                _mm_shuffle_epi8(lut, low_nibbles);
            const __m128i high_weights =
                _mm_shuffle_epi8(lut, high_nibbles);
            __m256 acc = _mm256_setzero_ps();
            acc = dot_i8_16_ps(low_weights, input, acc);
            return dot_i8_16_ps(high_weights, input + 16, acc);
        }

        NAI_AVX2_TARGET static float dequant_f32_dot_avx2(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            const float* weights = reinterpret_cast<const float*>(row);
            __m256 acc = _mm256_setzero_ps();
            uint32_t index = 0;
            for (; index + 8 <= elements; index += 8)
            {
                acc = _mm256_fmadd_ps(_mm256_loadu_ps(weights + index),
                                      _mm256_loadu_ps(input + index), acc);
            }
            float sum = hsum256_ps(acc);
            for (; index < elements; ++index)
            {
                sum += input[index] * weights[index];
            }
            return sum;
        }

        NAI_AVX2_TARGET static float dequant_q8_0_dot_avx2(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m256 total = _mm256_setzero_ps();
            for (uint32_t block = 0; block < elements / 32; ++block)
            {
                const uint8_t* bytes = row + block * 34;
                const __m256 scale = _mm256_set1_ps(
                    half_to_float(read_u16(bytes)));
                const int8_t* quant =
                    reinterpret_cast<const int8_t*>(bytes + 2);
                const float* values = input + block * 32;
                __m256 acc = _mm256_setzero_ps();
                for (uint32_t index = 0; index < 32; index += 8)
                {
                    acc = dot_i8_8_ps(quant + index, values + index, acc);
                }
                total = _mm256_fmadd_ps(acc, scale, total);
            }
            return hsum256_ps(total);
        }

        NAI_AVX2_TARGET static float dequant_iq4nl_dot_avx2(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m256 total = _mm256_setzero_ps();
            for (uint32_t block = 0; block < elements / 32; ++block)
            {
                const uint8_t* bytes = row + block * 18;
                const __m256 scale = _mm256_set1_ps(
                    half_to_float(read_u16(bytes)));
                total = _mm256_fmadd_ps(
                    iq4nl_group_dot_avx2(bytes + 2,
                                         input + block * 32),
                    scale, total);
            }
            return hsum256_ps(total);
        }

        NAI_AVX2_TARGET static float dequant_iq4xs_dot_avx2(
            const uint8_t* row, const float* input, uint32_t elements)
        {
            __m256 total = _mm256_setzero_ps();
            for (uint32_t block = 0; block < elements / 256; ++block)
            {
                const uint8_t* bytes = row + block * 136;
                const float d = half_to_float(read_u16(bytes));
                const uint16_t scales_h = read_u16(bytes + 2);
                const uint8_t* scales_l = bytes + 4;
                const uint8_t* qs = bytes + 8;
                const float* input_values = input + block * 256;
                for (uint32_t sub = 0; sub < 8; ++sub)
                {
                    const uint8_t sl = scales_l[sub / 2];
                    const int32_t ls =
                        ((sl >> (4 * (sub % 2))) & 0x0F) |
                        (((scales_h >> (2 * sub)) & 3) << 4);
                    const __m256 scale = _mm256_set1_ps(
                        d * static_cast<float>(ls - 32));
                    total = _mm256_fmadd_ps(
                        iq4nl_group_dot_avx2(
                            qs + sub * 16,
                            input_values + sub * 32),
                        scale, total);
                }
            }
            return hsum256_ps(total);
        }
#endif

        static float dequant_row_dot(const uint8_t* row,
                                     const tensor_info* tensor,
                                     const float* input)
        {
            const uint32_t elements = tensor->dims[0];
#if defined(__x86_64__)
            if (use_avx2_fma)
            {
                switch (tensor->type)
                {
                    case GGML_TYPE_F32:
                        return dequant_f32_dot_avx2(row, input, elements);
                    case GGML_TYPE_Q8_0:
                        return dequant_q8_0_dot_avx2(row, input, elements);
                    case GGML_TYPE_IQ4_NL:
                        return dequant_iq4nl_dot_avx2(row, input, elements);
                    case GGML_TYPE_IQ4_XS:
                        return dequant_iq4xs_dot_avx2(row, input, elements);
                    default:
                        break;
                }
            }
            else if (use_sse_simd)
            {
                switch (tensor->type)
                {
                    case GGML_TYPE_F32:
                        return dequant_f32_dot_sse(row, input, elements);
                    case GGML_TYPE_Q8_0:
                        return dequant_q8_0_dot_sse(row, input, elements);
                    case GGML_TYPE_IQ4_NL:
                        return dequant_iq4nl_dot_sse(row, input, elements);
                    case GGML_TYPE_IQ4_XS:
                        return dequant_iq4xs_dot_sse(row, input, elements);
                    default:
                        break;
                }
            }
#endif
            float sum = 0.0f;
            if (tensor->type == GGML_TYPE_F32)
            {
                const float* weights = reinterpret_cast<const float*>(row);
                for (uint32_t index = 0; index < elements; ++index)
                {
                    sum += input[index] * weights[index];
                }
            }
            else if (tensor->type == GGML_TYPE_Q8_0)
            {
                for (uint32_t block = 0; block < elements / 32; ++block)
                {
                    const uint8_t* bytes = row + block * 34;
                    const float scale = half_to_float(read_u16(bytes));
                    const int8_t* quant = reinterpret_cast<const int8_t*>(bytes + 2);
                    const float* values = input + block * 32;
                    for (uint32_t index = 0; index < 32; ++index)
                    {
                        sum += values[index] * static_cast<float>(quant[index]) * scale;
                    }
                }
            }
            else if (tensor->type == GGML_TYPE_IQ4_NL)
            {
                static const int8_t values[16] =
                {
                    -127, -104, -83, -65, -49, -35, -22, -10,
                    1, 13, 25, 38, 53, 69, 89, 113
                };
                for (uint32_t block = 0; block < elements / 32; ++block)
                {
                    const uint8_t* bytes = row + block * 18;
                    const float scale = half_to_float(read_u16(bytes));
                    const uint8_t* quant = bytes + 2;
                    const float* input_values = input + block * 32;
                    for (uint32_t index = 0; index < 16; ++index)
                    {
                        sum += input_values[index] *
                               static_cast<float>(values[quant[index] & 0x0F]) * scale;
                        sum += input_values[index + 16] *
                               static_cast<float>(values[quant[index] >> 4]) * scale;
                    }
                }
            }
            else if (tensor->type == GGML_TYPE_IQ4_XS)
            {
                static const int8_t values[16] =
                {
                    -127, -104, -83, -65, -49, -35, -22, -10,
                    1, 13, 25, 38, 53, 69, 89, 113
                };
                for (uint32_t block = 0; block < elements / 256; ++block)
                {
                    const uint8_t* bytes = row + block * 136;
                    const float d = half_to_float(read_u16(bytes));
                    const uint16_t scales_h = read_u16(bytes + 2);
                    const uint8_t* scales_l = bytes + 4;
                    const uint8_t* qs = bytes + 8;
                    const float* input_values = input + block * 256;

                    for (uint32_t ib = 0; ib < 8; ++ib)
                    {
                        const uint8_t sl = scales_l[ib / 2];
                        const int32_t ls = ((sl >> (4 * (ib % 2))) & 0x0F) |
                                           (((scales_h >> (2 * ib)) & 3) << 4);
                        const float dl = d * static_cast<float>(ls - 32);
                        const uint8_t* sub_qs = qs + ib * 16;
                        const float* sub_in = input_values + ib * 32;
                        for (uint32_t j = 0; j < 16; ++j)
                        {
                            sum += sub_in[j] * dl *
                                   static_cast<float>(values[sub_qs[j] & 0x0F]);
                            sum += sub_in[j + 16] * dl *
                                   static_cast<float>(values[sub_qs[j] >> 4]);
                        }
                    }
                }
            }
            else if (tensor->type == GGML_TYPE_Q3_K)
            {
                for (uint32_t block = 0; block < elements / 256; ++block)
                {
                    const uint8_t* bytes = row + block * 110;
                    const uint8_t* hmask = bytes;
                    const uint8_t* quant = bytes + 32;
                    const uint8_t* encoded_scales = bytes + 96;
                    const float scale = half_to_float(read_u16(bytes + 108));
                    const float* input_values = input + block * 256;

                    uint32_t aux[4];
                    aux[0] = encoded_scales[0] |
                             static_cast<uint32_t>(encoded_scales[1]) << 8 |
                             static_cast<uint32_t>(encoded_scales[2]) << 16 |
                             static_cast<uint32_t>(encoded_scales[3]) << 24;
                    aux[1] = encoded_scales[4] |
                             static_cast<uint32_t>(encoded_scales[5]) << 8 |
                             static_cast<uint32_t>(encoded_scales[6]) << 16 |
                             static_cast<uint32_t>(encoded_scales[7]) << 24;
                    aux[2] = encoded_scales[8] |
                             static_cast<uint32_t>(encoded_scales[9]) << 8 |
                             static_cast<uint32_t>(encoded_scales[10]) << 16 |
                             static_cast<uint32_t>(encoded_scales[11]) << 24;
                    aux[3] = 0;
                    uint32_t tmp = aux[2];
                    aux[2] = (aux[0] >> 4 & 0x0F0F0F0F) |
                             ((tmp >> 4 & 0x03030303) << 4);
                    aux[3] = (aux[1] >> 4 & 0x0F0F0F0F) |
                             ((tmp >> 6 & 0x03030303) << 4);
                    aux[0] = (aux[0] & 0x0F0F0F0F) |
                             ((tmp >> 0 & 0x03030303) << 4);
                    aux[1] = (aux[1] & 0x0F0F0F0F) |
                             ((tmp >> 2 & 0x03030303) << 4);
                    const int8_t* block_scales = reinterpret_cast<const int8_t*>(aux);

                    uint32_t is = 0;
                    uint8_t mask = 1;
                    for (uint32_t segment = 0; segment < 256; segment += 128)
                    {
                        uint32_t shift = 0;
                        for (uint32_t group = 0; group < 4; ++group)
                        {
                            float dl = scale *
                                       static_cast<float>(block_scales[is++] - 32);
                            for (uint32_t l = 0; l < 16; ++l)
                            {
                                uint32_t position = segment + group * 32 + l;
                                uint32_t low =
                                    (quant[l] >> shift) & 3;
                                const int32_t q_value =
                                    static_cast<int32_t>(low) -
                                    ((hmask[l] & mask) != 0 ? 0 : 4);
                                sum += input_values[position] * dl *
                                       static_cast<float>(q_value);
                            }
                            dl = scale *
                                 static_cast<float>(block_scales[is++] - 32);
                            for (uint32_t l = 0; l < 16; ++l)
                            {
                                uint32_t position = segment + group * 32 + 16 + l;
                                uint32_t low =
                                    (quant[l + 16] >> shift) & 3;
                                const int32_t q_value =
                                    static_cast<int32_t>(low) -
                                    ((hmask[l + 16] & mask) != 0 ? 0 : 4);
                                sum += input_values[position] * dl *
                                       static_cast<float>(q_value);
                            }
                            shift += 2;
                            mask <<= 1;
                        }
                        quant += 32;
                    }
                }
            }
            return sum;
        }

        static float f32_row_dot(const float* weights,
                                 const float* input, uint32_t elements)
        {
#if defined(__x86_64__)
            if (use_avx2_fma)
            {
                return dequant_f32_dot_avx2(
                    reinterpret_cast<const uint8_t*>(weights), input,
                    elements);
            }
            if (use_sse_simd)
            {
                return dequant_f32_dot_sse(
                    reinterpret_cast<const uint8_t*>(weights), input,
                    elements);
            }
#endif
            float sum = 0.0f;
            for (uint32_t index = 0; index < elements; ++index)
            {
                sum += input[index] * weights[index];
            }
            return sum;
        }

        static bool dequant_row_to_f32(const tensor_info* tensor,
                                       const uint8_t* row, float* out)
        {
            const uint32_t elements = tensor->dims[0];
            if (tensor->type == GGML_TYPE_F32)
            {
                memcpy(out, row, static_cast<size_t>(elements) * 4);
                return true;
            }
            if (tensor->type == GGML_TYPE_Q8_0)
            {
                for (uint32_t block = 0; block < elements / 32; ++block)
                {
                    const uint8_t* bytes = row + block * 34;
                    const float scale = half_to_float(read_u16(bytes));
                    const int8_t* quant =
                        reinterpret_cast<const int8_t*>(bytes + 2);
                    float* dst = out + block * 32;
                    for (uint32_t index = 0; index < 32; ++index)
                    {
                        dst[index] = static_cast<float>(quant[index]) * scale;
                    }
                }
                return true;
            }
            if (tensor->type == GGML_TYPE_IQ4_NL ||
                tensor->type == GGML_TYPE_IQ4_XS)
            {
                static const int8_t values[16] =
                {
                    -127, -104, -83, -65, -49, -35, -22, -10,
                    1, 13, 25, 38, 53, 69, 89, 113
                };
                if (tensor->type == GGML_TYPE_IQ4_NL)
                {
                    for (uint32_t block = 0; block < elements / 32; ++block)
                    {
                        const uint8_t* bytes = row + block * 18;
                        const float scale = half_to_float(read_u16(bytes));
                        const uint8_t* quant = bytes + 2;
                        float* dst = out + block * 32;
                        for (uint32_t index = 0; index < 16; ++index)
                        {
                            dst[index] = static_cast<float>(
                                values[quant[index] & 0x0F]) * scale;
                            dst[index + 16] = static_cast<float>(
                                values[quant[index] >> 4]) * scale;
                        }
                    }
                    return true;
                }
                for (uint32_t block = 0; block < elements / 256; ++block)
                {
                    const uint8_t* bytes = row + block * 136;
                    const float d = half_to_float(read_u16(bytes));
                    const uint16_t scales_h = read_u16(bytes + 2);
                    const uint8_t* scales_l = bytes + 4;
                    const uint8_t* qs = bytes + 8;
                    float* dst = out + block * 256;
                    for (uint32_t ib = 0; ib < 8; ++ib)
                    {
                        const uint8_t sl = scales_l[ib / 2];
                        const int32_t ls =
                            ((sl >> (4 * (ib % 2))) & 0x0F) |
                            (((scales_h >> (2 * ib)) & 3) << 4);
                        const float dl = d * static_cast<float>(ls - 32);
                        const uint8_t* sub_qs = qs + ib * 16;
                        float* sub_dst = dst + ib * 32;
                        for (uint32_t j = 0; j < 16; ++j)
                        {
                            sub_dst[j] = static_cast<float>(
                                values[sub_qs[j] & 0x0F]) * dl;
                            sub_dst[j + 16] = static_cast<float>(
                                values[sub_qs[j] >> 4]) * dl;
                        }
                    }
                }
                return true;
            }
            return false;
        }

        static bool f32_shadow_supported(uint32_t type)
        {
            return type == GGML_TYPE_F32 || type == GGML_TYPE_Q8_0 ||
                   type == GGML_TYPE_IQ4_NL || type == GGML_TYPE_IQ4_XS;
        }

        static void build_f32_weights()
        {
            uint64_t total_floats = 0;
            for (uint32_t index = 0; index < tensor_count; ++index)
            {
                const tensor_info* tensor = &tensors[index];
                if (!f32_shadow_supported(tensor->type))
                {
                    continue;
                }
                const uint64_t count =
                    static_cast<uint64_t>(tensor->dims[0]) *
                    tensor->dims[1];
                if (count == 0 || total_floats + count > (1ULL << 31))
                {
                    Uart::puts("[NAI] F32 weight shadow skipped\n");
                    return;
                }
                total_floats += count;
            }
            if (total_floats == 0)
            {
                return;
            }
            weights_f32 = static_cast<float*>(
                Memory::heap::kmalloc(total_floats * sizeof(float)));
            if (weights_f32 == nullptr)
            {
                Uart::puts("[NAI] F32 weight shadow unavailable\n");
                return;
            }
            uint64_t offset = 0;
            for (uint32_t index = 0; index < tensor_count; ++index)
            {
                tensor_info* tensor = &tensors[index];
                if (!f32_shadow_supported(tensor->type))
                {
                    continue;
                }
                const uint32_t in = tensor->dims[0];
                const uint32_t rows = tensor->dims[1];
                const uint32_t bytes = row_bytes(tensor);
                const uint8_t* src =
                    model_data + tensor->relative_offset;
                tensor->f32_offset = offset;
                for (uint32_t row = 0; row < rows; ++row)
                {
                    dequant_row_to_f32(
                        tensor, src + static_cast<uint64_t>(row) * bytes,
                        weights_f32 + offset +
                            static_cast<uint64_t>(row) * in);
                }
                offset += static_cast<uint64_t>(in) * rows;
            }
            Uart::puts("[NAI] F32 weight shadow ready\n");
        }

        static void matvec(tensor_info* tensor, const float* input,
                           float* output)
        {
            const uint32_t in = tensor->dims[0];
            const uint32_t out_count = tensor->dims[1];
            const uint32_t bytes = row_bytes(tensor);
            const uint8_t* weights = model_data + tensor->relative_offset;
            if (weights_f32 != nullptr &&
                tensor->f32_offset != UINT64_MAX)
            {
                const float* f32_weights =
                    weights_f32 + tensor->f32_offset;
                for (uint32_t row = 0; row < out_count; ++row)
                {
                    output[row] = f32_row_dot(
                        f32_weights + static_cast<uint64_t>(row) * in,
                        input, in);
                }
                return;
            }
            for (uint32_t row = 0; row < out_count; ++row)
            {
                output[row] = dequant_row_dot(weights + row * bytes,
                                              tensor, input);
            }
            (void)in;
        }

        static void embedding(tensor_info* tensor, uint32_t token,
                              float* output)
        {
            const uint32_t bytes = row_bytes(tensor);
            const uint8_t* row = model_data +
                                 tensor->relative_offset +
                                 static_cast<uint64_t>(token) * bytes;
            if (tensor->type == GGML_TYPE_Q8_0)
            {
                for (uint32_t block = 0; block < EMBED / 32; ++block)
                {
                    const uint8_t* data = row + block * 34;
                    const float scale = half_to_float(read_u16(data));
                    const int8_t* quant =
                        reinterpret_cast<const int8_t*>(data + 2);
                    for (uint32_t index = 0; index < 32; ++index)
                    {
                        output[block * 32 + index] =
                            static_cast<float>(quant[index]) * scale;
                    }
                }
            }
            else
            {
                for (uint32_t index = 0; index < EMBED; ++index)
                {
                    output[index] = 0.0f;
                }
            }
        }

        static void add_in_place(float* destination, const float* source,
                                 uint32_t count)
        {
            for (uint32_t index = 0; index < count; ++index)
            {
                destination[index] += source[index];
            }
        }

        static void apply_rope(float* vector, uint32_t position,
                               uint32_t start, uint32_t head_count)
        {
            for (uint32_t head = 0; head < head_count; ++head)
            {
                for (uint32_t pair = 0; pair < HEAD_DIM / 2; ++pair)
                {
                    const float exponent =
                        -static_cast<float>(pair * 2) / 64.0f *
                        11.512925465f;
                    const float frequency = exp_small(exponent);
                    float angle =
                        static_cast<float>(position) * frequency;
                    constexpr float TWO_PI = 6.28318530718f;
                    constexpr float PI = 3.14159265359f;
                    const int32_t turns =
                        static_cast<int32_t>(angle / TWO_PI);
                    angle -= static_cast<float>(turns) * TWO_PI;
                    if (angle > PI)
                    {
                        angle -= TWO_PI;
                    }
                    else if (angle < -PI)
                    {
                        angle += TWO_PI;
                    }
                    float sin_value = angle;
                    float sin_term = angle;
                    float cos_value = 1.0f;
                    float cos_term = 1.0f;
                    for (uint32_t step = 1; step < 10; ++step)
                    {
                        cos_term *= -angle * angle /
                                    static_cast<float>((2 * step - 1) *
                                                       (2 * step));
                        cos_value += cos_term;
                        sin_term *= -angle * angle /
                                    static_cast<float>((2 * step) *
                                                       (2 * step + 1));
                        sin_value += sin_term;
                    }
                    const uint32_t base = start + head * HEAD_DIM + pair * 2;
                    const float a = vector[base];
                    const float b = vector[base + 1];
                    vector[base] = a * cos_value - b * sin_value;
                    vector[base + 1] = a * sin_value + b * cos_value;
                }
            }
        }

        static uint32_t argmax(const float* logits)
        {
            uint32_t result = 0;
            float best = logits[0];
            for (uint32_t index = 1; index < VOCAB_SIZE; ++index)
            {
                if (logits[index] > best)
                {
                    best = logits[index];
                    result = index;
                }
            }
            return result;
        }

        static uint32_t hash_bytes(const char* text, uint32_t length)
        {
            uint32_t hash = 2166136261u;
            for (uint32_t index = 0; index < length; ++index)
            {
                hash ^= static_cast<uint8_t>(text[index]);
                hash *= 16777619u;
            }
            return hash;
        }

        static uint32_t find_vocab(const char* text, uint32_t length)
        {
            if (text == nullptr || vocab_count == 0)
            {
                return UINT32_MAX;
            }
            uint32_t slot = hash_bytes(text, length) &
                            (VOCAB_HASH_SIZE - 1);
            for (uint32_t probe = 0; probe < VOCAB_HASH_SIZE; ++probe)
            {
                const uint32_t value = vocab_hash_table[slot];
                if (value == 0)
                {
                    return UINT32_MAX;
                }
                const uint32_t token = value - 1;
                if (vocab[token].length == length &&
                    memcmp(vocab[token].text, text, length) == 0)
                {
                    return token;
                }
                slot = (slot + 1) & (VOCAB_HASH_SIZE - 1);
            }
            return UINT32_MAX;
        }

        static bool build_vocab_index()
        {
            if (vocab_count != VOCAB_SIZE)
            {
                return false;
            }
            for (uint32_t slot = 0; slot < VOCAB_HASH_SIZE; ++slot)
            {
                vocab_hash_table[slot] = 0;
            }
            for (uint32_t token = 0; token < vocab_count; ++token)
            {
                uint32_t slot = hash_bytes(vocab[token].text,
                                           vocab[token].length) &
                                (VOCAB_HASH_SIZE - 1);
                while (vocab_hash_table[slot] != 0)
                {
                    const uint32_t existing =
                        vocab_hash_table[slot] - 1;
                    if (vocab[existing].length == vocab[token].length &&
                        memcmp(vocab[existing].text, vocab[token].text,
                               vocab[token].length) == 0)
                    {
                        return false;
                    }
                    slot = (slot + 1) & (VOCAB_HASH_SIZE - 1);
                }
                vocab_hash_table[slot] = token + 1;
            }
            return true;
        }

        static uint32_t merge_hash(uint32_t left, uint32_t right)
        {
            uint32_t hash = left * 0x9E3779B1u;
            hash ^= right + 0x85EBCA6Bu + (hash << 6) + (hash >> 2);
            hash ^= hash >> 16;
            return hash & (MERGE_HASH_SIZE - 1);
        }

        static bool find_merge(uint32_t left, uint32_t right,
                               uint32_t* merged, uint32_t* rank)
        {
            uint32_t slot = merge_hash(left, right);
            for (uint32_t probe = 0; probe < MERGE_HASH_SIZE; ++probe)
            {
                const uint32_t value = merge_hash_table[slot];
                if (value == 0)
                {
                    return false;
                }
                const merge_entry& entry = merges[value - 1];
                if (entry.left == left && entry.right == right)
                {
                    *merged = entry.merged;
                    *rank = entry.rank;
                    return true;
                }
                slot = (slot + 1) & (MERGE_HASH_SIZE - 1);
            }
            return false;
        }

        static bool build_merge_index()
        {
            if (merge_count == 0 || merge_count > MERGE_COUNT_MAX)
            {
                return false;
            }
            for (uint32_t slot = 0; slot < MERGE_HASH_SIZE; ++slot)
            {
                merge_hash_table[slot] = 0;
            }
            for (uint32_t rank = 0; rank < merge_count; ++rank)
            {
                const merge_text_entry& text = merge_texts[rank];
                uint32_t separator = 0;
                while (separator < text.length &&
                       text.text[separator] != ' ')
                {
                    ++separator;
                }
                if (separator == 0 || separator + 1 >= text.length)
                {
                    return false;
                }
                const uint32_t left =
                    find_vocab(text.text, separator);
                const uint32_t right = find_vocab(
                    text.text + separator + 1,
                    text.length - separator - 1);
                char merged_text[512];
                const uint32_t merged_length =
                    text.length - 1;
                if (left == UINT32_MAX || right == UINT32_MAX ||
                    merged_length >= sizeof(merged_text))
                {
                    return false;
                }
                memcpy(merged_text, text.text, separator);
                memcpy(merged_text + separator, text.text + separator + 1,
                       text.length - separator - 1);
                const uint32_t merged =
                    find_vocab(merged_text, merged_length);
                if (merged == UINT32_MAX)
                {
                    return false;
                }

                merges[rank] = {left, right, merged, rank};
                uint32_t slot = merge_hash(left, right);
                while (merge_hash_table[slot] != 0)
                {
                    const merge_entry& existing =
                        merges[merge_hash_table[slot] - 1];
                    if (existing.left == left &&
                        existing.right == right)
                    {
                        return false;
                    }
                    slot = (slot + 1) & (MERGE_HASH_SIZE - 1);
                }
                merge_hash_table[slot] = rank + 1;
            }
            return true;
        }

        static bool parse_metadata()
        {
            if (metadata == nullptr ||
                metadata_size < 28 ||
                metadata[0] != 'G' || metadata[1] != 'G' ||
                metadata[2] != 'U' || metadata[3] != 'F')
            {
                return false;
            }

            gguf_reader reader{metadata, metadata_size, 4};
            uint32_t gguf_version;
            uint64_t tensor_count64;
            uint64_t kv_count64;
            if (!read_u32_reader(&reader, &gguf_version) ||
                gguf_version != 3 ||
                !read_u64_reader(&reader, &tensor_count64) ||
                !read_u64_reader(&reader, &kv_count64) ||
                tensor_count64 > 272 || kv_count64 > 272)
            {
                return false;
            }
            tensor_count = static_cast<uint32_t>(tensor_count64);
            const uint64_t kv_count = kv_count64;

            Uart::puts("[NAI] GGUF header: ");
            Uart::puts(tensor_count == 272 && kv_count == 37
                           ? "expected model counts\n"
                           : "unexpected tensor/metadata counts\n");
            char key[128];
            for (uint64_t kv = 0; kv < kv_count; ++kv)
            {
                if (!read_string(&reader, key, sizeof(key)))
                {
                    uint64_t key_length;
                    reader.cursor -= 8;
                    if (!read_u64_reader(&reader, &key_length) ||
                        key_length >= sizeof(key) ||
                        key_length > reader.size - reader.cursor)
                    {
                        return false;
                    }
                    if (!read_bytes(&reader, key, static_cast<uint32_t>(key_length)))
                    {
                        return false;
                    }
                    key[key_length] = '\0';
                }

                uint32_t value_type;
                if (!read_u32_reader(&reader, &value_type))
                {
                    return false;
                }

                if (value_type == 9)
                {
                    uint32_t element_type;
                    uint64_t count;
                    if (!read_u32_reader(&reader, &element_type) ||
                        !read_u64_reader(&reader, &count))
                    {
                        return false;
                    }
                    if (element_type == 8 &&
                        text_equal(key, "tokenizer.ggml.tokens") &&
                        count <= VOCAB_SIZE)
                    {
                        vocab_count = static_cast<uint32_t>(count);
                        for (uint64_t index = 0; index < count; ++index)
                        {
                            uint64_t length;
                            if (!read_u64_reader(&reader, &length) ||
                                length > reader.size - reader.cursor)
                            {
                                return false;
                            }
                            const char* text =
                                reinterpret_cast<const char*>(reader.data + reader.cursor);
                            vocab[index].text = text;
                            vocab[index].length = static_cast<uint32_t>(length);
                            reader.cursor += static_cast<uint32_t>(length);
                        }
                    }
                    else if (element_type == 8 &&
                             text_equal(key, "tokenizer.ggml.merges") &&
                             count <= MERGE_COUNT_MAX)
                    {
                        merge_count = static_cast<uint32_t>(count);
                        for (uint64_t index = 0; index < count; ++index)
                        {
                            uint64_t length;
                            if (!read_u64_reader(&reader, &length) ||
                                length == 0 ||
                                length > reader.size - reader.cursor)
                            {
                                return false;
                            }
                            merge_texts[index].text =
                                reinterpret_cast<const char*>(
                                    reader.data + reader.cursor);
                            merge_texts[index].length =
                                static_cast<uint32_t>(length);
                            reader.cursor += static_cast<uint32_t>(length);
                        }
                    }
                    else
                    {
                        uint32_t element_size = 0;
                        switch (element_type)
                        {
                            case 0: case 1: case 7: element_size = 1; break;
                            case 2: case 3: element_size = 2; break;
                            case 4: case 5: case 6: element_size = 4; break;
                            case 10: case 11: case 12: element_size = 8; break;
                            default: break;
                        }
                        if (element_type == 8)
                        {
                            for (uint64_t index = 0; index < count; ++index)
                            {
                                uint64_t length;
                                if (!read_u64_reader(&reader, &length) ||
                                    length > reader.size - reader.cursor)
                                {
                                    return false;
                                }
                                reader.cursor += static_cast<uint32_t>(length);
                            }
                        }
                        else if (element_size == 0 ||
                                 count > (reader.size - reader.cursor) / element_size)
                        {
                            return false;
                        }
                        else
                        {
                            reader.cursor +=
                                static_cast<uint32_t>(count * element_size);
                        }
                    }
                }
                else if (value_type == 8)
                {
                    uint64_t length;
                    if (!read_u64_reader(&reader, &length) ||
                        length > reader.size - reader.cursor)
                    {
                        return false;
                    }
                    reader.cursor += static_cast<uint32_t>(length);
                }
                else if (value_type == 4)
                {
                    uint32_t value;
                    if (!read_u32_reader(&reader, &value))
                    {
                        return false;
                    }
                    if (text_equal(key, "tokenizer.ggml.bos_token_id"))
                    {
                        bos_token = value;
                    }
                    else if (text_equal(key, "tokenizer.ggml.eos_token_id"))
                    {
                        eos_token = value;
                    }
                    else if (text_equal(key, "tokenizer.ggml.padding_token_id"))
                    {
                        pad_token = value;
                    }
                }
                else
                {
                    if (!skip_metadata_value(&reader, value_type))
                    {
                        return false;
                    }
                }
            }

            if (!build_vocab_index() || !build_merge_index())
            {
                return false;
            }

            for (uint32_t index = 0; index < tensor_count; ++index)
            {
                uint64_t name_length;
                if (!read_u64_reader(&reader, &name_length) ||
                    name_length == 0 || name_length >= sizeof(tensors[index].name))
                {
                    return false;
                }
                if (!read_bytes(&reader, tensors[index].name,
                                static_cast<uint32_t>(name_length)))
                {
                    return false;
                }
                tensors[index].name[name_length] = '\0';

                uint32_t dimensions;
                if (!read_u32_reader(&reader, &dimensions) ||
                    dimensions > 2)
                {
                    return false;
                }
                tensors[index].dims[0] = 1;
                tensors[index].dims[1] = 1;
                for (uint32_t dimension = 0; dimension < dimensions; ++dimension)
                {
                    uint64_t size;
                    if (!read_u64_reader(&reader, &size) ||
                        size > UINT32_MAX)
                    {
                        return false;
                    }
                    tensors[index].dims[dimension] = static_cast<uint32_t>(size);
                }
                if (!read_u32_reader(&reader, &tensors[index].type) ||
                    !read_u64_reader(&reader,
                                     &tensors[index].relative_offset))
                {
                    return false;
                }
                tensors[index].f32_offset = UINT64_MAX;
            }

            return true;
        }

#if defined(__x86_64__)
        static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                                 uint32_t& eax, uint32_t& ebx,
                                 uint32_t& ecx, uint32_t& edx)
        {
            asm volatile("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(leaf), "c"(subleaf));
        }
#endif

        static bool cpu_has_xcr0_avx()
        {
#if defined(__x86_64__)
            uint64_t cr4;
            asm volatile("mov %%cr4, %0" : "=r"(cr4));
            cr4 |= 1ULL << 18; // CR4.OSXSAVE
            asm volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
            uint32_t xcr0_lo;
            uint32_t xcr0_hi;
            asm volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi)
                         : "c"(0));
            if ((xcr0_lo & 0x6) != 0x6)
            {
                asm volatile("xsetbv" : : "c"(0), "a"(xcr0_lo | 0x6),
                             "d"(xcr0_hi) : "memory");
            }
            return true;
#else
            return false;
#endif
        }

        static void detect_simd()
        {
#if defined(__x86_64__)
            uint32_t eax, ebx, ecx, edx;
            cpuid(0, 0, eax, ebx, ecx, edx);
            const uint32_t max_leaf = eax;
            cpuid(1, 0, eax, ebx, ecx, edx);
            const bool sse2 = (edx & (1u << 25)) != 0;
            const bool ssse3 = (ecx & (1u << 9)) != 0;
            const bool sse41 = (ecx & (1u << 19)) != 0;
            const bool osxsave = (ecx & (1u << 27)) != 0;
            const bool avx = (ecx & (1u << 28)) != 0;
            const bool fma = (ecx & (1u << 12)) != 0;
            bool avx2 = false;
            if (max_leaf >= 7)
            {
                cpuid(7, 0, eax, ebx, ecx, edx);
                avx2 = (ebx & (1u << 5)) != 0;
            }
            if (avx2 && osxsave && avx && fma && cpu_has_xcr0_avx())
            {
                use_avx2_fma = true;
                use_sse_simd = true;
                Uart::puts("[NAI] AVX2+FMA kernels enabled\n");
            }
            else if (sse2 && ssse3 && sse41)
            {
                use_sse_simd = true;
                Uart::puts("[NAI] SSE4.1 kernels enabled\n");
            }
#endif
        }

        static tensor_info* find_layer_tensor(uint32_t layer,
                                              const char* suffix)
        {
            char name[64];
            uint32_t index = 0;
            const char prefix[] = "blk.";
            while (prefix[index] != '\0')
            {
                name[index] = prefix[index];
                ++index;
            }
            if (layer >= 10)
            {
                name[index++] = static_cast<char>('0' + layer / 10);
            }
            name[index++] = static_cast<char>('0' + layer % 10);
            uint32_t suffix_index = 0;
            while (suffix[suffix_index] != '\0')
            {
                name[index++] = suffix[suffix_index++];
            }
            name[index] = '\0';
            return find_tensor(name);
        }

        static bool resolve_model_tensors()
        {
            token_embd_tensor = find_tensor("token_embd.weight");
            output_norm_tensor = find_tensor("output_norm.weight");
            for (uint32_t layer = 0; layer < LAYER_COUNT; ++layer)
            {
                layer_tensor_set& set = layer_tensors[layer];
                set.attn_norm = find_layer_tensor(layer, ".attn_norm.weight");
                set.attn_q = find_layer_tensor(layer, ".attn_q.weight");
                set.attn_k = find_layer_tensor(layer, ".attn_k.weight");
                set.attn_v = find_layer_tensor(layer, ".attn_v.weight");
                set.attn_output = find_layer_tensor(layer, ".attn_output.weight");
                set.ffn_norm = find_layer_tensor(layer, ".ffn_norm.weight");
                set.ffn_gate = find_layer_tensor(layer, ".ffn_gate.weight");
                set.ffn_up = find_layer_tensor(layer, ".ffn_up.weight");
                set.ffn_down = find_layer_tensor(layer, ".ffn_down.weight");
                if (set.attn_norm == nullptr || set.attn_q == nullptr ||
                    set.attn_k == nullptr || set.attn_v == nullptr ||
                    set.attn_output == nullptr || set.ffn_norm == nullptr ||
                    set.ffn_gate == nullptr || set.ffn_up == nullptr ||
                    set.ffn_down == nullptr)
                {
                    return false;
                }
            }
            return token_embd_tensor != nullptr &&
                   output_norm_tensor != nullptr;
        }

        static bool initialize()
        {
            if (initialized)
            {
                return true;
            }
            if (init_failed)
            {
                return false;
            }

            Uart::puts("[NAI] Initializing SmolLM2 backend\n");
            if (!Iso9660::init())
            {
                return fail("ATAPI/ISO9660 initialization");
            }
            uint32_t file_size;
            if (!Iso9660::file_size(MODEL_PATH, &file_size))
            {
                return fail("model file not found on ISO");
            }
            if (file_size != 90897760)
            {
                return fail("model file size mismatch");
            }
            Uart::puts("[NAI] Model file found; reading GGUF metadata\n");

            metadata = static_cast<uint8_t*>(Memory::heap::kmalloc(MAX_METADATA_SIZE));
            if (metadata == nullptr ||
                !Iso9660::read_file_range(MODEL_PATH, 0,
                                          metadata, MAX_METADATA_SIZE))
            {
                return fail("reading GGUF metadata");
            }
            metadata_size = MAX_METADATA_SIZE;
            if (!parse_metadata())
            {
                return fail("parsing GGUF metadata/tensors");
            }
            Uart::puts("[NAI] GGUF tensor directory parsed\n");
            detect_simd();
            if (!resolve_model_tensors())
            {
                return fail("model tensor lookup");
            }
            Uart::puts("[NAI] Model tensor map resolved\n");

            uint64_t tensor_info_end = 0;
            uint64_t highest_end = 0;
            for (uint32_t index = 0; index < tensor_count; ++index)
            {
                tensor_info* tensor = &tensors[index];
                const uint64_t elements =
                    static_cast<uint64_t>(tensor->dims[0]) *
                    static_cast<uint64_t>(tensor->dims[1]);
                uint64_t size = 0;
                switch (tensor->type)
                {
                    case GGML_TYPE_F32: size = elements * 4; break;
                    case GGML_TYPE_Q8_0:
                        size = elements / 32 * 34; break;
                    case GGML_TYPE_Q3_K:
                        size = elements / 256 * 110; break;
                    case GGML_TYPE_IQ4_NL:
                        size = elements / 32 * 18; break;
                    case GGML_TYPE_IQ4_XS:
                        size = elements / 256 * 136; break;
                    default: break;
                }
                const uint64_t end = tensor->relative_offset + size;
                if (end > highest_end)
                {
                    highest_end = end;
                }
            }
            tensor_info_end = highest_end;
            (void)tensor_info_end;

            // Tensor directory starts at 0x1b013b. Derive the data base from
            // the aligned end of the metadata and tensor directory in memory.
            uint64_t data_base = 0;
            if (tensor_count > 0)
            {
                gguf_reader reader{metadata, metadata_size, 4};
                uint32_t gguf_version;
                uint64_t tensor_count64;
                uint64_t kv_count64;
                if (!read_u32_reader(&reader, &gguf_version) ||
                    gguf_version != 3 ||
                    !read_u64_reader(&reader, &tensor_count64) ||
                    !read_u64_reader(&reader, &kv_count64))
                {
                    return fail("reading GGUF tensor directory");
                }
                for (uint64_t kv = 0; kv < kv_count64; ++kv)
                {
                    uint64_t key_length;
                    if (!read_u64_reader(&reader, &key_length) ||
                        key_length > reader.size - reader.cursor)
                    {
                        return fail("reading GGUF metadata keys");
                    }
                    reader.cursor += static_cast<uint32_t>(key_length);
                    uint32_t value_type;
                    if (!read_u32_reader(&reader, &value_type) ||
                        !skip_metadata_value(&reader, value_type))
                    {
                        return fail("skipping GGUF metadata");
                    }
                }
                for (uint32_t index = 0; index < tensor_count; ++index)
                {
                    uint64_t name_length;
                    if (!read_u64_reader(&reader, &name_length) ||
                        name_length > reader.size - reader.cursor)
                    {
                        return fail("reading GGUF tensor names");
                    }
                    reader.cursor += static_cast<uint32_t>(name_length);
                    uint32_t dimensions;
                    if (!read_u32_reader(&reader, &dimensions))
                    {
                        return fail("reading GGUF tensor dimensions");
                    }
                    reader.cursor += dimensions * 8;
                    reader.cursor += 12;
                }
                data_base = (reader.cursor + 31) & ~31ULL;
                if (data_base > file_size)
                {
                    return fail("tensor data offset out of range");
                }
            }

            model_data_size = file_size - static_cast<uint32_t>(data_base);
            if (highest_end > model_data_size)
            {
                return fail("tensor offsets exceed the model file");
            }
            Uart::puts("[NAI] Loading quantized weights from ISO\n");
            model_data = static_cast<uint8_t*>(Memory::heap::kmalloc(model_data_size));
            if (model_data == nullptr)
            {
                return fail("allocating model weights");
            }
            for (uint32_t offset = 0; offset < model_data_size;)
            {
                const uint32_t chunk =
                    model_data_size - offset > 4 * 1024 * 1024
                        ? 4 * 1024 * 1024
                        : model_data_size - offset;
                if (!Iso9660::read_file_range(
                        MODEL_PATH,
                        static_cast<uint32_t>(data_base) + offset,
                        model_data + offset, chunk))
                {
                    return fail("reading model tensor data from ISO");
                }
                offset += chunk;
            }

            key_cache = static_cast<float*>(Memory::heap::kmalloc(
                LAYER_COUNT * CONTEXT_SIZE * KV_EMBED * sizeof(float)));
            value_cache = static_cast<float*>(Memory::heap::kmalloc(
                LAYER_COUNT * CONTEXT_SIZE * KV_EMBED * sizeof(float)));
            if (key_cache == nullptr || value_cache == nullptr)
            {
                return fail("allocating attention cache");
            }
            for (uint32_t index = 0;
                 index < LAYER_COUNT * CONTEXT_SIZE * KV_EMBED;
                 ++index)
            {
                key_cache[index] = 0.0f;
                value_cache[index] = 0.0f;
            }

            if (!use_avx2_fma)
            {
                build_f32_weights();
            }

            initialized = true;
            Uart::puts("[NAI] SmolLM2 backend ready\n");
            return true;
        }

        static bool gpt2_byte_is_direct(uint32_t value)
        {
            return (value >= 0x21 && value <= 0x7E) ||
                   (value >= 0xA1 && value <= 0xAC) ||
                   (value >= 0xAE && value <= 0xFF);
        }

        static uint32_t gpt2_codepoint_for_byte(uint8_t byte)
        {
            if (gpt2_byte_is_direct(byte))
            {
                return byte;
            }

            uint32_t codepoint = 256;
            for (uint32_t candidate = 0; candidate < byte; ++candidate)
            {
                if (!gpt2_byte_is_direct(candidate))
                {
                    ++codepoint;
                }
            }
            return codepoint;
        }

        static bool gpt2_byte_for_codepoint(uint32_t codepoint,
                                            uint8_t* byte)
        {
            if (codepoint <= 0xFF && gpt2_byte_is_direct(codepoint))
            {
                *byte = static_cast<uint8_t>(codepoint);
                return true;
            }

            uint32_t mapped_codepoint = 256;
            for (uint32_t candidate = 0; candidate <= 0xFF; ++candidate)
            {
                if (!gpt2_byte_is_direct(candidate))
                {
                    if (mapped_codepoint == codepoint)
                    {
                        *byte = static_cast<uint8_t>(candidate);
                        return true;
                    }
                    ++mapped_codepoint;
                }
            }
            return false;
        }

        static uint32_t encode_gpt2_bytes(const char* text,
                                          uint32_t length,
                                          char* output,
                                          uint32_t capacity)
        {
            uint32_t written = 0;
            for (uint32_t index = 0; index < length; ++index)
            {
                const uint32_t codepoint = gpt2_codepoint_for_byte(
                    static_cast<uint8_t>(text[index]));
                if (codepoint < 0x80)
                {
                    if (written >= capacity)
                    {
                        break;
                    }
                    output[written++] = static_cast<char>(codepoint);
                }
                else
                {
                    if (capacity - written < 2)
                    {
                        break;
                    }
                    output[written++] =
                        static_cast<char>(0xC0 | (codepoint >> 6));
                    output[written++] =
                        static_cast<char>(0x80 | (codepoint & 0x3F));
                }
            }
            return written;
        }

        static uint32_t utf8_sequence_length(const char* text,
                                             uint32_t length,
                                             uint32_t position)
        {
            const uint8_t first =
                static_cast<uint8_t>(text[position]);
            if (first < 0x80)
            {
                return 1;
            }
            if ((first & 0xE0) == 0xC0 && length - position >= 2)
            {
                return 2;
            }
            if ((first & 0xF0) == 0xE0 && length - position >= 3)
            {
                return 3;
            }
            if ((first & 0xF8) == 0xF0 && length - position >= 4)
            {
                return 4;
            }
            return 1;
        }

        static uint32_t encode_bpe_piece(const char* text, uint32_t length,
                                         uint32_t* output,
                                         uint32_t capacity)
        {
            uint32_t symbols[512];
            uint32_t symbol_count = 0;
            for (uint32_t position = 0; position < length;)
            {
                const uint32_t character_length =
                    utf8_sequence_length(text, length, position);
                if (symbol_count >= sizeof(symbols) / sizeof(symbols[0]))
                {
                    return 0;
                }
                const uint32_t token = find_vocab(
                    text + position, character_length);
                if (token == UINT32_MAX)
                {
                    return 0;
                }
                symbols[symbol_count++] = token;
                position += character_length;
            }

            while (symbol_count > 1)
            {
                uint32_t best_rank = UINT32_MAX;
                uint32_t best_position = UINT32_MAX;
                uint32_t best_merged = UINT32_MAX;
                for (uint32_t position = 0;
                     position + 1 < symbol_count;
                     ++position)
                {
                    uint32_t merged;
                    uint32_t rank;
                    if (find_merge(symbols[position],
                                   symbols[position + 1],
                                   &merged, &rank) &&
                        rank < best_rank)
                    {
                        best_rank = rank;
                        best_position = position;
                        best_merged = merged;
                    }
                }
                if (best_position == UINT32_MAX)
                {
                    break;
                }
                symbols[best_position] = best_merged;
                for (uint32_t index = best_position + 1;
                     index + 1 < symbol_count;
                     ++index)
                {
                    symbols[index] = symbols[index + 1];
                }
                --symbol_count;
            }

            const uint32_t output_count =
                symbol_count < capacity ? symbol_count : capacity;
            for (uint32_t index = 0; index < output_count; ++index)
            {
                output[index] = symbols[index];
            }
            return output_count;
        }

        static bool is_whitespace(uint32_t codepoint)
        {
            return codepoint == ' ' || codepoint == '\t' ||
                   codepoint == '\n' || codepoint == '\r' ||
                   codepoint == '\f' || codepoint == '\v' ||
                   codepoint == 0x00A0;
        }

        static bool is_digit(uint32_t codepoint)
        {
            return codepoint >= '0' && codepoint <= '9';
        }

        static bool is_letter(uint32_t codepoint)
        {
            return (codepoint >= 'a' && codepoint <= 'z') ||
                   (codepoint >= 'A' && codepoint <= 'Z') ||
                   (codepoint >= 0x80 && !is_whitespace(codepoint) &&
                    !(codepoint >= 0x0300 && codepoint <= 0x036F));
        }

        static uint32_t raw_codepoint(const char* text, uint32_t length,
                                      uint32_t position)
        {
            const uint8_t first =
                static_cast<uint8_t>(text[position]);
            const uint32_t count =
                utf8_sequence_length(text, length, position);
            if (count == 1)
            {
                return first;
            }
            uint32_t value = first & (count == 2 ? 0x1F :
                                      count == 3 ? 0x0F : 0x07);
            for (uint32_t index = 1; index < count; ++index)
            {
                value = (value << 6) |
                    (static_cast<uint8_t>(text[position + index]) & 0x3F);
            }
            return value;
        }

        static uint32_t contraction_length(const char* text,
                                           uint32_t length,
                                           uint32_t position)
        {
            static const char* const contractions[] =
            {
                "'s", "'t", "'re", "'ve", "'m", "'ll", "'d",
                "'S", "'T", "'RE", "'VE", "'M", "'LL", "'D"
            };
            static const uint8_t sizes[] =
            {
                2, 2, 3, 3, 2, 3, 2, 2, 2, 3, 3, 2, 3, 2
            };
            for (uint32_t index = 0;
                 index < sizeof(sizes) / sizeof(sizes[0]);
                 ++index)
            {
                if (position + sizes[index] <= length &&
                    memcmp(text + position, contractions[index],
                           sizes[index]) == 0)
                {
                    return sizes[index];
                }
            }
            return 0;
        }

        static uint32_t encode_text(const char* text, uint32_t length,
                                    uint32_t* output, uint32_t capacity)
        {
            uint32_t count = 0;
            uint32_t position = 0;
            char encoded[512];
            while (position < length && count < capacity)
            {
                const uint32_t contraction =
                    contraction_length(text, length, position);
                uint32_t end = position;
                if (contraction != 0)
                {
                    end += contraction;
                }
                else
                {
                    uint32_t codepoint = raw_codepoint(
                        text, length, position);
                    uint32_t category = is_whitespace(codepoint) ? 0 :
                        is_digit(codepoint) ? 1 :
                        is_letter(codepoint) ? 2 : 3;

                    if (codepoint == ' ' &&
                        position + 1 < length &&
                        !is_whitespace(raw_codepoint(
                            text, length,
                            position + utf8_sequence_length(
                                text, length, position))))
                    {
                        end = position + 1;
                        codepoint = raw_codepoint(text, length, end);
                        category = is_digit(codepoint) ? 1 :
                            is_letter(codepoint) ? 2 : 3;
                    }

                    if (category == 0)
                    {
                        end = position;
                        while (end < length &&
                               is_whitespace(raw_codepoint(
                                   text, length, end)))
                        {
                            end += utf8_sequence_length(text, length, end);
                        }
                    }
                    else if (category == 1)
                    {
                        end += utf8_sequence_length(text, length, end);
                    }
                    else
                    {
                        if (end == position)
                        {
                            end += utf8_sequence_length(
                                text, length, end);
                        }
                        while (end < length)
                        {
                            const uint32_t next = raw_codepoint(
                                text, length, end);
                            const uint32_t next_category =
                                is_whitespace(next) ? 0 :
                                is_digit(next) ? 1 :
                                is_letter(next) ? 2 : 3;
                            if (next_category != category)
                            {
                                break;
                            }
                            end += utf8_sequence_length(text, length, end);
                        }
                    }
                }

                const uint32_t encoded_length = encode_gpt2_bytes(
                    text + position, end - position,
                    encoded, sizeof(encoded));
                count += encode_bpe_piece(
                    encoded, encoded_length,
                    output + count, capacity - count);
                position = end;
            }
            return count;
        }

        static uint32_t tokenize(const char* prompt, uint32_t* tokens,
                                 uint32_t capacity)
        {
            static const char start[] = "<|im_start|>";
            static const char end[] = "<|im_end|>";
            if (tokens == nullptr || capacity == 0)
            {
                return 0;
            }

            uint32_t count = 0;
            tokens[count++] = bos_token;

            if (count < capacity)
            {
                static const char user[] = "user\n";
                count += encode_text(user, sizeof(user) - 1,
                                      tokens + count,
                                      capacity - count);
            }

            uint32_t prompt_length = 0;
            while (prompt[prompt_length] != '\0' &&
                   prompt_length < 256)
            {
                ++prompt_length;
            }
            if (count < capacity)
            {
                count += encode_text(prompt, prompt_length,
                                      tokens + count,
                                      capacity - count);
            }

            if (count < capacity)
            {
                uint32_t token = find_vocab(end, sizeof(end) - 1);
                if (token != UINT32_MAX)
                {
                    tokens[count++] = token;
                }
            }
            if (count < capacity)
            {
                static const char newline[] = "\n";
                count += encode_text(newline, sizeof(newline) - 1,
                                      tokens + count,
                                      capacity - count);
            }
            if (count < capacity)
            {
                uint32_t token = find_vocab(start, sizeof(start) - 1);
                if (token != UINT32_MAX)
                {
                    tokens[count++] = token;
                }
            }
            if (count < capacity)
            {
                static const char assistant[] = "assistant";
                count += encode_text(assistant, sizeof(assistant) - 1,
                                      tokens + count,
                                      capacity - count);
            }
            if (count < capacity)
            {
                static const char newline[] = "\n";
                count += encode_text(newline, sizeof(newline) - 1,
                                      tokens + count,
                                      capacity - count);
            }
            return count;
        }

        static uint32_t eval_token(uint32_t token, uint32_t position,
                                   inference_buffers* buffers)
        {
            float* hidden = buffers->hidden;
            float* norm = buffers->norm;
            float* q = buffers->q;
            float* k = buffers->k;
            float* v = buffers->v;
            float* attn = buffers->attn;
            float* gate = buffers->gate;
            float* up = buffers->up;
            float* logits = buffers->logits;

            tensor_info* token_embd = token_embd_tensor;
            if (token_embd == nullptr)
            {
                Uart::puts("[NAI] Failed: token_embd.weight is missing\n");
                return UINT32_MAX;
            }
            embedding(token_embd, token, hidden);

            for (uint32_t layer = 0; layer < LAYER_COUNT; ++layer)
            {
                const layer_tensor_set& layer_set = layer_tensors[layer];
                tensor_info* attn_norm = layer_set.attn_norm;
                tensor_info* attn_q = layer_set.attn_q;
                tensor_info* attn_k = layer_set.attn_k;
                tensor_info* attn_v = layer_set.attn_v;
                tensor_info* attn_output = layer_set.attn_output;
                tensor_info* ffn_norm = layer_set.ffn_norm;
                tensor_info* ffn_gate = layer_set.ffn_gate;
                tensor_info* ffn_up = layer_set.ffn_up;
                tensor_info* ffn_down = layer_set.ffn_down;

                if (attn_norm == nullptr || attn_q == nullptr ||
                    attn_k == nullptr || attn_v == nullptr ||
                    attn_output == nullptr || ffn_norm == nullptr ||
                    ffn_gate == nullptr || ffn_up == nullptr ||
                    ffn_down == nullptr)
                {
                    Uart::puts("[NAI] Failed: model layer tensor missing\n");
                    return UINT32_MAX;
                }

                rms_norm(hidden,
                         reinterpret_cast<const float*>(
                             model_data + attn_norm->relative_offset),
                         norm, EMBED);
                matvec(attn_q, norm, q);
                matvec(attn_k, norm, k);
                matvec(attn_v, norm, v);

                apply_rope(q, position, 0, HEAD_COUNT);
                apply_rope(k, position, 0, KV_HEAD_COUNT);

                float* k_cache = key_cache +
                    static_cast<uint64_t>(layer) * CONTEXT_SIZE * KV_EMBED +
                    static_cast<uint64_t>(position) * KV_EMBED;
                float* v_cache = value_cache +
                    static_cast<uint64_t>(layer) * CONTEXT_SIZE * KV_EMBED +
                    static_cast<uint64_t>(position) * KV_EMBED;
                memcpy(k_cache, k, KV_EMBED * sizeof(float));
                memcpy(v_cache, v, KV_EMBED * sizeof(float));

                for (uint32_t head = 0; head < EMBED / HEAD_DIM; ++head)
                {
                    const uint32_t kv_head = head / 3;
                    float scores[CONTEXT_SIZE];
                    float score_sum = 0.0f;
                    float max_score = -1000000.0f;
                    for (uint32_t at = 0; at <= position; ++at)
                    {
                        const float* key = key_cache +
                            static_cast<uint64_t>(layer) *
                                CONTEXT_SIZE * KV_EMBED +
                            static_cast<uint64_t>(at) * KV_EMBED +
                            kv_head * HEAD_DIM;
                        float score = 0.0f;
                        for (uint32_t dim = 0; dim < HEAD_DIM; ++dim)
                        {
                            score += q[head * HEAD_DIM + dim] * key[dim];
                        }
                        score /= sqrt_positive(static_cast<float>(HEAD_DIM));
                        scores[at] = score;
                        if (score > max_score)
                        {
                            max_score = score;
                        }
                    }
                    for (uint32_t at = 0; at <= position; ++at)
                    {
                        scores[at] = exp_small(scores[at] - max_score);
                        score_sum += scores[at];
                    }
                    for (uint32_t dim = 0; dim < HEAD_DIM; ++dim)
                    {
                        float value = 0.0f;
                        for (uint32_t at = 0; at <= position; ++at)
                        {
                            const float* vector = value_cache +
                                static_cast<uint64_t>(layer) *
                                    CONTEXT_SIZE * KV_EMBED +
                                static_cast<uint64_t>(at) * KV_EMBED +
                                kv_head * HEAD_DIM;
                            value += scores[at] * vector[dim];
                        }
                        attn[head * HEAD_DIM + dim] =
                            value / (score_sum > 0.0f ? score_sum : 1.0f);
                    }
                }

                matvec(attn_output, attn, norm);
                add_in_place(hidden, norm, EMBED);

                rms_norm(hidden,
                         reinterpret_cast<const float*>(
                             model_data + ffn_norm->relative_offset),
                         norm, EMBED);
                matvec(ffn_gate, norm, gate);
                matvec(ffn_up, norm, up);
                for (uint32_t index = 0; index < FF; ++index)
                {
                    gate[index] = gate[index] /
                        (1.0f + exp_small(-gate[index]));
                    gate[index] *= up[index];
                }
                matvec(ffn_down, gate, norm);
                add_in_place(hidden, norm, EMBED);
            }

            tensor_info* output_norm = output_norm_tensor;
            if (output_norm == nullptr)
            {
                Uart::puts("[NAI] Failed: output_norm.weight is missing\n");
                return UINT32_MAX;
            }
            rms_norm(hidden,
                     reinterpret_cast<const float*>(
                         model_data + output_norm->relative_offset),
                     norm, EMBED);
            matvec(token_embd, norm, logits);
            return argmax(logits);
        }

        static bool read_piece_codepoint(const vocab_entry& piece,
                                         uint32_t* position,
                                         uint32_t* codepoint)
        {
            const uint8_t first =
                static_cast<uint8_t>(piece.text[*position]);
            if (first < 0x80)
            {
                *codepoint = first;
                ++*position;
                return true;
            }

            uint32_t continuation_count;
            uint32_t value;
            if ((first & 0xE0) == 0xC0)
            {
                continuation_count = 1;
                value = first & 0x1F;
            }
            else if ((first & 0xF0) == 0xE0)
            {
                continuation_count = 2;
                value = first & 0x0F;
            }
            else if ((first & 0xF8) == 0xF0)
            {
                continuation_count = 3;
                value = first & 0x07;
            }
            else
            {
                ++*position;
                return false;
            }

            if (piece.length - *position <= continuation_count)
            {
                ++*position;
                return false;
            }
            for (uint32_t index = 1; index <= continuation_count; ++index)
            {
                const uint8_t next =
                    static_cast<uint8_t>(piece.text[*position + index]);
                if ((next & 0xC0) != 0x80)
                {
                    ++*position;
                    return false;
                }
                value = (value << 6) | (next & 0x3F);
            }
            *position += continuation_count + 1;
            *codepoint = value;
            return true;
        }

        static void append_piece(char* output, uint32_t* length,
                                 uint32_t capacity, uint32_t token)
        {
            if (token >= VOCAB_SIZE || *length >= capacity)
            {
                return;
            }
            const vocab_entry& piece = vocab[token];
            uint32_t position = 0;
            while (position < piece.length && *length + 1 < capacity)
            {
                uint32_t codepoint = 0;
                uint8_t byte = '?';
                if (read_piece_codepoint(piece, &position, &codepoint) &&
                    gpt2_byte_for_codepoint(codepoint, &byte))
                {
                    output[(*length)++] = static_cast<char>(byte);
                }
                else
                {
                    output[(*length)++] = '?';
                }
            }
            output[*length] = '\0';
        }

        static void log_exchange(const char* prompt, const char* output)
        {
            Uart::puts("[NAI] Input: ");
            Uart::puts(prompt != nullptr ? prompt : "<null>");
            Uart::puts("\n[NAI] Output: ");
            Uart::puts(output != nullptr ? output : "<null>");
            Uart::puts("\n");
        }

        static void record_generation_stats(uint32_t tokens,
                                            uint64_t start_ticks)
        {
            const uint64_t elapsed_ticks =
                Timer::pit_get_ticks() - start_ticks;
            last_generated_tokens = tokens;
            last_elapsed_ms = elapsed_ticks * 10;
            last_stats_valid = true;
            Uart::puts("[NAI] Stats: ");
            Uart::putdec(tokens);
            Uart::puts(" tokens in ");
            Uart::putdec(last_elapsed_ms);
            Uart::puts(" ms (");
            if (last_elapsed_ms != 0)
            {
                Uart::putdec(tokens * 1000 / last_elapsed_ms);
            }
            else
            {
                Uart::putdec(0);
            }
            Uart::puts(" tok/s)\n");
        }

        bool generate_impl(const char* prompt, char* output,
                      uint32_t output_capacity, uint32_t max_tokens)
        {
            last_stats_valid = false;
            if (prompt == nullptr || output == nullptr ||
                output_capacity == 0)
            {
                log_exchange(prompt,
                             output != nullptr && output_capacity > 0
                                 ? output
                                 : "");
                return false;
            }
            output[0] = '\0';
            if (!initialize())
            {
                log_exchange(prompt, output);
                return false;
            }

            inference_buffers buffers;
            buffers.hidden = static_cast<float*>(
                Memory::heap::kmalloc(EMBED * sizeof(float)));
            buffers.norm = static_cast<float*>(
                Memory::heap::kmalloc(EMBED * sizeof(float)));
            buffers.q = static_cast<float*>(
                Memory::heap::kmalloc(EMBED * sizeof(float)));
            buffers.k = static_cast<float*>(
                Memory::heap::kmalloc(KV_EMBED * sizeof(float)));
            buffers.v = static_cast<float*>(
                Memory::heap::kmalloc(KV_EMBED * sizeof(float)));
            buffers.attn = static_cast<float*>(
                Memory::heap::kmalloc(EMBED * sizeof(float)));
            buffers.gate = static_cast<float*>(
                Memory::heap::kmalloc(FF * sizeof(float)));
            buffers.up = static_cast<float*>(
                Memory::heap::kmalloc(FF * sizeof(float)));
            buffers.logits = static_cast<float*>(
                Memory::heap::kmalloc(VOCAB_SIZE * sizeof(float)));
            if (buffers.hidden == nullptr || buffers.norm == nullptr ||
                buffers.q == nullptr || buffers.k == nullptr ||
                buffers.v == nullptr || buffers.attn == nullptr ||
                buffers.gate == nullptr || buffers.up == nullptr ||
                buffers.logits == nullptr)
            {
                Uart::puts("[NAI] Failed: allocating inference scratch buffers\n");
                log_exchange(prompt, output);
                return false;
            }

            const uint64_t start_ticks = Timer::pit_get_ticks();
            uint32_t response_tokens = 0;
            uint32_t tokens[CONTEXT_SIZE];
            const uint32_t token_count =
                tokenize(prompt, tokens, CONTEXT_SIZE);
            if (token_count == 0)
            {
                Uart::puts("[NAI] Failed: prompt tokenization produced no tokens\n");
                log_exchange(prompt, output);
                return false;
            }

            Uart::puts("[NAI] Prompt token count: ");
            Uart::putdec(token_count);
            Uart::puts("; suffix token IDs:");
            const uint32_t suffix_start =
                token_count > 4 ? token_count - 4 : 0;
            for (uint32_t index = suffix_start;
                 index < token_count;
                 ++index)
            {
                Uart::puts(" ");
                Uart::putdec(tokens[index]);
            }
            Uart::puts("\n");
            Uart::puts("[NAI] Running transformer inference\n");
            uint32_t next = pad_token;
            for (uint32_t index = 0; index < token_count &&
                                    index < CONTEXT_SIZE; ++index)
            {
                next = eval_token(tokens[index], index, &buffers);
                if (next == UINT32_MAX)
                {
                    record_generation_stats(response_tokens, start_ticks);
                    log_exchange(prompt, output);
                    return false;
                }
            }

            Uart::puts("[NAI] Prompt processed; generating response\n");
            Uart::puts("[NAI] First predicted token ID: ");
            Uart::putdec(next);
            Uart::puts("\n");
            uint32_t output_length = 0;
            for (uint32_t generated = 0;
                 generated < max_tokens &&
                 generated + token_count < CONTEXT_SIZE;
                 ++generated)
            {
                if (next == eos_token || next == bos_token || next == 0)
                {
                    break;
                }
                append_piece(output, &output_length,
                             output_capacity, next);
                if (output_length == 0)
                {
                    Uart::puts("[NAI] Model generated an empty/unmapped token\n");
                    record_generation_stats(response_tokens, start_ticks);
                    log_exchange(prompt, output);
                    return false;
                }
                ++response_tokens;
                next = eval_token(next, token_count + generated, &buffers);
                if (next == UINT32_MAX)
                {
                    record_generation_stats(response_tokens, start_ticks);
                    log_exchange(prompt, output);
                    return false;
                }
                if ((generated + 1) % 8 == 0)
                {
                    Uart::puts("[NAI] Generated ");
                    Uart::putdec(generated + 1);
                    Uart::puts(" tokens\n");
                }
            }
            Uart::puts(output_length == 0
                           ? "[NAI] Model stopped before generating a response\n"
                           : "[NAI] Response generated\n");
            record_generation_stats(response_tokens, start_ticks);
            log_exchange(prompt, output);
            return output_length != 0;
        }

        static void generation_worker(void*)
        {
            const bool ok = generate_impl(current_job.prompt,
                                          current_job.result,
                                          sizeof(current_job.result),
                                          current_job.max_tokens);
            current_job.state = ok ? JOB_DONE : JOB_FAILED;
            Scheduler::exit_current();
        }
    }

    bool generate(const char* prompt, char* output,
                  uint32_t output_capacity, uint32_t max_tokens)
    {
        return generate_impl(prompt, output, output_capacity, max_tokens);
    }

    bool generate_stats(uint32_t* tokens, uint64_t* elapsed_ms)
    {
        if (!last_stats_valid || tokens == nullptr ||
            elapsed_ms == nullptr)
        {
            return false;
        }
        *tokens = last_generated_tokens;
        *elapsed_ms = last_elapsed_ms;
        return true;
    }

    uint32_t generate_start(const char* prompt, uint32_t max_tokens)
    {
        if (prompt == nullptr || job_active)
        {
            return 0;
        }

        uint32_t length = 0;
        while (prompt[length] != '\0' &&
               length + 1 < sizeof(current_job.prompt))
        {
            ++length;
        }
        memcpy(current_job.prompt, prompt, length);
        current_job.prompt[length] = '\0';
        current_job.max_tokens = max_tokens == 0 ? 1 : max_tokens;
        current_job.id = next_job_id++;
        if (next_job_id == 0)
        {
            next_job_id = 1;
        }
        current_job.state = JOB_RUNNING;
        job_active = true;

        if (Scheduler::create_task(generation_worker, nullptr) == 0)
        {
            current_job.state = JOB_IDLE;
            job_active = false;
            return 0;
        }
        return current_job.id;
    }

    int32_t generate_poll(uint32_t job, char* output,
                          uint32_t output_capacity)
    {
        if (!job_active || job != current_job.id ||
            output == nullptr || output_capacity == 0)
        {
            return -1;
        }
        if (current_job.state == JOB_RUNNING)
        {
            return 0;
        }

        const uint32_t state = current_job.state;
        job_active = false;
        current_job.state = JOB_IDLE;
        if (state != JOB_DONE)
        {
            return -1;
        }

        uint32_t length = 0;
        while (length + 1 < output_capacity &&
               current_job.result[length] != '\0')
        {
            output[length] = current_job.result[length];
            ++length;
        }
        output[length] = '\0';
        return 1;
    }
}
