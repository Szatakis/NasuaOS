#include "qr_code.hpp"

#include <stdint.h>
#include <stddef.h>

namespace
{
    /*
        ============================================================
                        QR CODE GENERATOR
        ============================================================
     
        Features:
        - QR Version 1..10
        - Byte mode
        - Error correction level L
        - Reed-Solomon
        - RS block interleaving
        - Data interleaving
        - All 8 QR masks
        - Mask penalty calculation
        - Finder patterns
        - Timing patterns
        - Alignment patterns
        - Format information
        - No malloc
        - No STL
        - Freestanding compatible
     */

    static const int MAX_VERSION = 10;
    static const int MAX_QR_SIZE = 57;

    struct VersionInfo
    {
        int size;
        int total_codewords;
        int data_codewords;
        int blocks;
        int ecc_per_block;

        int alignment_count;
        int alignment[7];
    };

    static const VersionInfo version_table[MAX_VERSION] =
    {
        {
            21, 26, 19, 1, 7, 0,
            { 0, 0, 0, 0, 0, 0, 0 }
        },
        {
            25, 44, 34, 1, 10, 1,
            { 18, 0, 0, 0, 0, 0, 0 }
        },
        {
            29, 70, 55, 1, 15, 1,
            { 22, 0, 0, 0, 0, 0, 0 }
        },
        {
            33, 100, 80, 1, 20, 1,
            { 26, 0, 0, 0, 0, 0, 0 }
        },

        {
            37, 134, 108, 1, 26, 1,
            { 30, 0, 0, 0, 0, 0, 0 }
        },
        {
            41, 172, 136, 2, 18, 1,
            { 34, 0, 0, 0, 0, 0, 0 }
        },
        {
            45, 196, 156, 2, 20, 2,
            { 22, 38, 0, 0, 0, 0, 0 }
        },
        {
            49,242, 194, 2, 24, 2,
            { 24, 42, 0, 0, 0, 0, 0 }
        },
        {
            53, 292, 232, 2, 30, 2,
            { 26, 46, 0, 0, 0, 0, 0 }
        },
        {
            57, 346, 274, 2, 36, 2,
            { 28, 50, 0, 0, 0, 0, 0 }
        }
    };


    static uint8_t qr_matrix[MAX_QR_SIZE * MAX_QR_SIZE];
    static uint8_t function_matrix[MAX_QR_SIZE * MAX_QR_SIZE];
    static uint8_t best_matrix[MAX_QR_SIZE * MAX_QR_SIZE];


    static uint8_t gf_exp[512];
    static uint8_t gf_log[256];

    static bool gf_ready = false;


    static void init_galois_field()
    {
        if (gf_ready)
        {
            return;
        }

        uint16_t value = 1;

        for (int i = 0; i < 255; ++i)
        {
            gf_exp[i] = (uint8_t)value;
            gf_log[value] = (uint8_t)i;

            value <<= 1;

            if (value & 0x100)
            {
                value ^= 0x11D;
            }
        }

        for (int i = 255; i < 512; ++i)
        {
            gf_exp[i] = gf_exp[i - 255];
        }

        gf_ready = true;
    }


    static uint8_t gf_multiply(uint8_t a,uint8_t b)
    {
        if (a == 0 || b == 0)
        {
            return 0;
        }

        return gf_exp[gf_log[a] + gf_log[b]];
    }


    static void clear_matrix(int size)
    {
        for (int i = 0; i < size * size; ++i)
        {
            qr_matrix[i] = 0;
            function_matrix[i] = 0;
        }
    }


    static void set_module(int size, int x, int y, bool black, bool function)
    {
        if (x < 0 || y < 0 || x >= size || y >= size)
        {
            return;
        }

        qr_matrix[y * size + x] = black ? 1 : 0;

        if (function)
        {
            function_matrix[y * size + x] = 1;
        }
    }


    static void draw_finder(int size, int ox, int oy)
    {
        for (int dy = -1; dy <= 7; ++dy)
        {
            for (int dx = -1; dx <= 7; ++dx)
            {
                int x = ox + dx;
                int y = oy + dy;

                if (x < 0 || y < 0 || x >= size || y >= size)
                {
                    continue;
                }

                bool black = false;

                if (dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6)
                {
                    if (
                        dx == 0 || dx == 6 || dy == 0 || dy == 6)
                    {
                        black = true;
                    }

                    if (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4)
                    {
                        black = true;
                    }
                }

                set_module(size, x, y, black, true);
            }
        }
    }


    static void draw_timing(int size)
    {
        for (int i = 8; i < size - 8; ++i)
        {
            if (!function_matrix[6 * size + i])
            {
                set_module(size, i, 6, (i & 1) == 0, true);
            }

            if (!function_matrix[i * size + 6])
            {
                set_module(size, 6, i, (i & 1) == 0, true);
            }
        }
    }


    static void draw_alignment(int size, int cx, int cy)
    {
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                int ax = dx < 0 ? -dx : dx;
                int ay = dy < 0 ? -dy : dy;
                bool black = ax == 2 || ay == 2 || (ax == 0 && ay == 0);

                set_module(size, cx + dx, cy + dy, black, true);
            }
        }
    }


    static bool finder_overlap(int size, int cx, int cy)
    {
        if (cx - 2 <= 6 && cx + 2 >= 0 && cy - 2 <= 6 && cy + 2 >= 0)
        {
            return true;
        }

        if (cx - 2 <= size - 1 && cx + 2 >= size - 7 && cy - 2 <= 6 && cy + 2 >= 0)
        {
            return true;
        }

        if (cx - 2 <= 6 && cx + 2 >= 0 && cy - 2 <= size - 1 && cy + 2 >= size - 7)
        {
            return true;
        }

        return false;
    }


    static void draw_alignment_patterns(int version)
    {
        if (version == 1)
        {
            return;
        }

        const VersionInfo& info = version_table[ version - 1];
        int positions[7];
        positions[0] = 6;

        for (int i = 0; i < info.alignment_count; ++i)
        {
            positions[i + 1] = info.alignment[i];
        }

        int count = info.alignment_count + 1;

        for (int y = 0; y < count; ++y)
        {
            for (int x = 0; x < count; ++x)
            {
                int cx = positions[x];
                int cy = positions[y];

                if (finder_overlap(info.size, cx, cy))
                {
                    continue;
                }

                draw_alignment(info.size, cx, cy);
            }
        }
    }


    static uint16_t make_format_bits(int mask)
    {
        uint16_t data = (uint16_t)((1 << 3) |mask);
        uint16_t value = data << 10;
        const uint16_t polynomial = 0x537;

        for (int i = 14; i >= 10; --i)
        {
            if ((value >> i) & 1)
            {
                value ^= polynomial << (i - 10);
            }
        }

        return ((data << 10) | value) ^ 0x5412;
    }


    static void draw_format_bits(int size, int mask)
    {
        uint16_t bits = make_format_bits(mask);

        for (int i = 0; i <= 5; ++i)
        {
            set_module(size, 8, i, ((bits >> i) & 1) != 0, true);
        }

        set_module(size, 8, 7, ((bits >> 6) & 1) != 0, true);
        set_module(size, 8, 8, ((bits >> 7) & 1) != 0, true);
        set_module(size, 7, 8, ((bits >> 8) & 1) != 0, true);

        for (int i = 9; i < 15; ++i)
        {
            set_module(size, 14 - i, 8, ((bits >> i) & 1) != 0, true);
        }


        for (int i = 0; i < 8; ++i)
        {
            set_module(size, size - 1 - i, 8, ((bits >> i) & 1) != 0, true);
        }

        for (int i = 8; i < 15; ++i)
        {
            set_module(size, 8, size - 15 + i, ((bits >> i) & 1) != 0, true);
        }


        set_module(size, 8, size - 8, true, true);
    }


    static bool mask_condition(int mask, int x, int y)
    {
        switch (mask)
        {
            case 0:
                return ((x + y) & 1) == 0;

            case 1:
                return (y & 1) == 0;

            case 2:
                return (x % 3) == 0;

            case 3:
                return ((x + y) % 3) == 0;

            case 4:
                return (((y / 2) + (x / 3)) & 1) == 0;

            case 5:
            {
                int v = x * y;

                return ((v % 2) + (v % 3)) == 0;
            }

            case 6:
            {
                int v = x * y;

                return (((v % 2) + (v % 3)) & 1) == 0;
            }

            case 7:
                return ((((x * y) % 3) + (x + y)) & 1) == 0;
        }

        return false;
    }

    static void make_generator(int degree, uint8_t* generator)
    {
        for (int i = 0; i < 40; ++i)
        {
            generator[i] = 0;
        }

        generator[0] = 1;
        int current_degree = 0;

        for (int i = 0; i < degree; ++i)
        {
            uint8_t temp[40];

            for (int j = 0; j < 40; ++j)
            {
                temp[j] = 0;
            }

            uint8_t root = gf_exp[i];

            for (int j = 0; j <= current_degree; ++j)
            {
                temp[j] ^= generator[j];
                temp[j + 1] ^= gf_multiply( generator[j], root);
            }

            ++current_degree;

            for (int j = 0; j <= current_degree; ++j)
            {
                generator[j] = temp[j];
            }
        }
    }


    static void make_ecc(const uint8_t* data, int data_len, int ecc_len, uint8_t* ecc)
    {
        uint8_t generator[40];

        make_generator(ecc_len, generator);

        for (int i = 0; i < ecc_len; ++i)
        {
            ecc[i] = 0;
        }

        for (int i = 0; i < data_len; ++i)
        {
            uint8_t factor = data[i] ^ ecc[0];

            for (int j = 0; j < ecc_len - 1; ++j)
            {
                ecc[j] = ecc[j + 1];
            }

            ecc[ecc_len - 1] = 0;

            if (factor != 0)
            {
                for (int j = 0; j < ecc_len; ++j)
                {
                    ecc[j] ^= gf_multiply(generator[j + 1], factor);
                }
            }
        }
    }


    static void write_bit(uint8_t* data, int& bit_pos, bool bit)
    {
        if (bit)
        {
            int byte = bit_pos >> 3;
            int shift = 7 - (bit_pos & 7);

            data[byte] |= (uint8_t)(1 << shift);
        }

        ++bit_pos;
    }


    static int make_data_codewords(const char* text, int length, int version, uint8_t* data)
    {
        const VersionInfo& info = version_table[version - 1];

        for (int i = 0; i < info.data_codewords; ++i)
        {
            data[i] = 0;
        }

        int bit_pos = 0;


        write_bit(data, bit_pos, false);
        write_bit(data, bit_pos, true);
        write_bit(data, bit_pos, false);
        write_bit(data, bit_pos, false);

        int count_bits = version <= 9 ? 8 : 16;

        for (int i = count_bits - 1; i >= 0; --i)
        {
            write_bit(data, bit_pos, ((length >> i) & 1) != 0);
        }


        for (int i = 0; i < length; ++i)
        {
            uint8_t c = (uint8_t)text[i];

            for (int b = 7; b >= 0; --b)
            {
                write_bit(data, bit_pos, ((c >> b) & 1) != 0);
            }
        }

        int total_bits = info.data_codewords * 8;
        int remaining = total_bits - bit_pos;
        int terminator = remaining < 4 ? remaining : 4;

        for (int i = 0; i < terminator; ++i)
        {
            write_bit(data, bit_pos, false);
        }


        while (bit_pos & 7)
        {
            write_bit(data, bit_pos, false);
        }


        int byte_pos = bit_pos >> 3;
        uint8_t pad = 0xEC;

        while ( byte_pos < info.data_codewords)
        {
            data[byte_pos++] = pad;
            pad = pad == 0xEC ? 0x11 : 0xEC;
        }

        return info.data_codewords;
    }


    static int make_codewords(const char* text, int length, int version, uint8_t* output)
    {
        const VersionInfo& info = version_table[version - 1];

        uint8_t data[300];

        int data_count = make_data_codewords(text, length, version, data);
        int base = data_count / info.blocks;
        int remainder = data_count % info.blocks;
        int block_data_len[4];

        for (int b = 0; b < info.blocks; ++b)
        {
            block_data_len[b] = base + (b >= info.blocks - remainder ? 1 : 0);
        }

        uint8_t block_data[4][150];
        uint8_t block_ecc[4][40];

        int input_pos = 0;

        for (int b = 0; b < info.blocks; ++b)
        {
            for (int i = 0; i < block_data_len[b]; ++i)
            {
                block_data[b][i] = data[input_pos++];
            }

            make_ecc(block_data[b], block_data_len[b], info.ecc_per_block, block_ecc[b]);
        }

        int output_pos = 0;
        int max_data = 0;

        for (int b = 0; b < info.blocks; ++b)
        {
            if (block_data_len[b] > max_data)
            {
                max_data = block_data_len[b];
            }
        }

        for (int i = 0; i < max_data; ++i)
        {
            for (int b = 0; b < info.blocks; ++b)
            {
                if (i < block_data_len[b])
                {
                    output[output_pos++] = block_data[b][i];
                }
            }
        }


        for (int i = 0; i < info.ecc_per_block; ++i)
        {
            for (int b = 0; b < info.blocks; ++b)
            {
                output[output_pos++] = block_ecc[b][i];
            }
        }

        return output_pos;
    }


    static void build_function_patterns(int version)
    {
        const VersionInfo& info = version_table[version - 1];

        clear_matrix(info.size);
        draw_finder(info.size, 0, 0);
        draw_finder(info.size, info.size - 7, 0);
        draw_finder(info.size, 0, info.size - 7);
        draw_timing(info.size);
        draw_alignment_patterns(version);


        for (int i = 0; i < 9; ++i)
        {
            function_matrix[8 * info.size + i] = 1;
            function_matrix[i * info.size + 8] = 1;
        }

        for (int i = 0; i < 8; ++i)
        {
            function_matrix[8 * info.size + info.size - 1 - i] = 1;
            function_matrix[(info.size - 1 - i) * info.size +8] = 1;
        }

        function_matrix[(info.size - 8) * info.size + 8] = 1;
    }


    static void place_data(int size, const uint8_t* codewords, int codeword_count, int mask)
    {
        int bit_index = 0;
        int x = size - 1;
        int direction = -1;

        while (x > 0)
        {
            if (x == 6)
            {
                --x;
            }

            for (int i = 0; i < size; ++i)
            {
                int y = direction == 1 ? i : size - 1 - i;

                for (int dx = 0; dx < 2; ++dx)
                {
                    int px = x - dx;

                    if (function_matrix[y * size + px])
                    {
                        continue;
                    }

                    bool bit = false;

                    if (bit_index < codeword_count * 8 )
                    {
                        int byte_index =bit_index >> 3;
                        int shift = 7 - (bit_index & 7);

                        bit = ((codewords[byte_index] >> shift) & 1) != 0;
                    }


                    if (mask_condition(mask, px, y))
                    {
                        bit = !bit;
                    }

                    qr_matrix[y * size + px] = bit ? 1 : 0;

                    ++bit_index;
                }
            }

            x -= 2;

            direction = -direction;
        }
    }

    static int calculate_penalty(int size)
    {
        int score = 0;

        for (int y = 0; y < size; ++y)
        {
            int run = 1;

            for (int x = 1; x < size; ++x)
            {
                if (qr_matrix[y * size + x] == qr_matrix[y * size + x - 1])
                {
                    ++run;
                }
                else
                {
                    if (run >= 5)
                    {
                        score += 3 + (run - 5);
                    }

                    run = 1;
                }
            }

            if (run >= 5)
            {
                score += 3 + (run - 5);
            }
        }


        for (int x = 0; x < size; ++x)
        {
            int run = 1;

            for (int y = 1; y < size; ++y)
            {
                if (
                    qr_matrix[y * size + x] == qr_matrix[(y - 1) * size + x]
                )
                {
                    ++run;
                }
                else
                {
                    if (run >= 5)
                    {
                        score += 3 + (run - 5);
                    }

                    run = 1;
                }
            }

            if (run >= 5)
            {
                score += 3 + (run - 5);
            }
        }


        for (int y = 0; y < size - 1; ++y)
        {
            for (int x = 0; x < size - 1; ++x)
            {
                uint8_t a = qr_matrix[y * size + x];

                if (
                    qr_matrix[y * size + x + 1] == a &&
                    qr_matrix[(y + 1) * size + x] == a &&
                    qr_matrix[(y + 1) * size + x + 1] == a
                )
                {
                    score += 3;
                }
            }
        }


        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x + 6 < size; ++x)
            {
                if (
                    qr_matrix[y * size + x] == 1 &&
                    qr_matrix[y * size + x + 1] == 0 &&
                    qr_matrix[y * size + x + 2] == 1 &&
                    qr_matrix[y * size + x + 3] == 1 &&
                    qr_matrix[y * size + x + 4] == 1 &&
                    qr_matrix[y * size + x + 5] == 0 &&
                    qr_matrix[y * size + x + 6] == 1
                )
                {
                    score += 40;
                }
            }
        }


        for (int x = 0; x < size; ++x)
        {
            for (int y = 0; y + 6 < size; ++y)
            {
                if (
                    qr_matrix[y * size + x] == 1 &&
                    qr_matrix[(y + 1) * size + x] == 0 &&
                    qr_matrix[(y + 2) * size + x] == 1 &&
                    qr_matrix[(y + 3) * size + x] == 1 &&
                    qr_matrix[(y + 4) * size + x] == 1 &&
                    qr_matrix[(y + 5) * size + x] == 0 &&
                    qr_matrix[(y + 6) * size + x] == 1
                )
                {
                    score += 40;
                }
            }
        }


        int dark_modules = 0;

        for (int i = 0; i < size * size; ++i)
        {
            if (qr_matrix[i])
            {
                ++dark_modules;
            }
        }

        int total = size * size;
        int percent = (dark_modules * 100) / total;
        int deviation = percent > 50 ? percent - 50 : 50 - percent;
        score += (deviation / 5) * 10;

        return score;
    }


    static bool encode_qr(const char* text, int length, int& output_size)
    {
        init_galois_field();

        int selected_version = 0;

        for (int version = 1; version <= MAX_VERSION; ++version)
        {
            int count_bits = version <= 9 ? 8 : 16;
            int required_bits = 4 + count_bits + length * 8;
            int required_bytes = (required_bits + 7) / 8;

            if (required_bytes <= version_table[version - 1].data_codewords)
            {
                selected_version = version;

                break;
            }
        }

        if (selected_version == 0)
        {
            return false;
        }

        const VersionInfo& info = version_table[selected_version - 1];
        uint8_t codewords[400];

        int codeword_count = make_codewords(text, length, selected_version, codewords);
        int best_score = 0x7FFFFFFF;
        int best_mask = 0;

        for (int mask = 0; mask < 8; ++mask)
        {
            build_function_patterns(selected_version);
            place_data(info.size, codewords, codeword_count, mask);
            draw_format_bits(info.size, mask);
            int score = calculate_penalty(info.size);

            if (score < best_score)
            {
                best_score = score;
                best_mask = mask;

                for (int i = 0; i < info.size * info.size; ++i)
                {
                    best_matrix[i] = qr_matrix[i];
                }
            }
        }

        for (int i = 0; i < info.size * info.size; ++i)
        {
            qr_matrix[i] = best_matrix[i];
        }

        output_size = info.size;

        (void)best_mask;

        return true;
    }
}

bool generate_qr_code(int size, const char* text, uint32_t* out_buffer)
{
    if (!text || !out_buffer || size <= 0)
    {
        return false;
    }

    int length = 0;

    while (text[length] != '\0')
    {
        ++length;

        if (length > 271)
        {
            return false;
        }
    }

    int qr_size = 0;

    if (!encode_qr(text, length, qr_size))
    {
        return false;
    }

    int module_size = size / qr_size;

    if (module_size < 1)
    {
        module_size = 1;
    }

    int rendered_size = qr_size * module_size;
    int offset_x = (size - rendered_size) / 2;
    int offset_y = (size - rendered_size) / 2;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            out_buffer[y * size + x] = 0xFFFFFFFF;
        }
    }


    //Draw modules.
    for (int qy = 0; qy < qr_size; ++qy)
    {
        for (int qx = 0; qx < qr_size; ++qx)
        {
            bool black = qr_matrix[qy * qr_size + qx] != 0;
            uint32_t pixel = black ? 0xFF000000 : 0xFFFFFFFF;
            int start_x = offset_x + qx * module_size;
            int start_y = offset_y + qy * module_size;

            for (int py = 0; py < module_size; ++py)
            {
                int y = start_y + py;

                if (y < 0 || y >= size)
                {
                    continue;
                }

                for (int px = 0; px < module_size; ++px)
                {
                    int x = start_x + px;

                    if (x < 0 || x >= size)
                    {
                        continue;
                    }

                    out_buffer[y * size + x] = pixel;
                }
            }
        }
    }

    return true;
}