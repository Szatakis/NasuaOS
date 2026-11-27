#include "png.hpp"

namespace Nasua
{
    namespace Png
    {
        namespace
        {
            constexpr uint8_t PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};
            constexpr uint32_t MAX_DIMENSION = 8192;
            constexpr size_t MAX_IMAGE_BYTES = 256U * 1024U * 1024U;
            constexpr uint32_t ADAM7_X[7] = {0, 4, 0, 2, 0, 1, 0};
            constexpr uint32_t ADAM7_Y[7] = {0, 0, 4, 0, 2, 0, 1};
            constexpr uint32_t ADAM7_DX[7] = {8, 8, 4, 4, 2, 2, 1};
            constexpr uint32_t ADAM7_DY[7] = {8, 8, 8, 4, 4, 2, 2};
            constexpr uint16_t LENGTH_BASE[29] =
            {
                3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
            };
            constexpr uint8_t LENGTH_EXTRA[29] =
            {
                0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
            };
            constexpr uint16_t DISTANCE_BASE[30] =
            {
                1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                6145, 8193, 12289, 16385, 24577
            };
            constexpr uint8_t DISTANCE_EXTRA[30] =
            {
                0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
            };
            constexpr uint8_t CODE_LENGTH_ORDER[19] =
            {
                16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
            };

            struct Chunk
            {
                uint32_t length;
                uint32_t type;
                const uint8_t* data;
            };

            struct PngInfo
            {
                uint32_t width = 0;
                uint32_t height = 0;
                uint8_t bit_depth = 0;
                uint8_t color_type = 0;
                uint8_t interlace = 0;
                uint8_t palette[256 * 3] = {};
                uint8_t transparency[256] = {};
                uint16_t transparent_gray = 0;
                uint16_t transparent_red = 0;
                uint16_t transparent_green = 0;
                uint16_t transparent_blue = 0;
                uint16_t palette_size = 0;
                uint16_t transparency_size = 0;
                bool has_transparent_gray = false;
                bool has_transparent_rgb = false;
                bool has_palette = false;
            };

            struct Huffman
            {
                uint16_t count[16] = {};
                uint16_t symbols[288] = {};
            };

            struct BitReader
            {
                const uint8_t* input;
                size_t size;
                size_t position;
                uint64_t bits;
                uint8_t bit_count;
                bool failed;
            };

            struct Inflater
            {
                BitReader reader;
                uint8_t* output;
                size_t output_size;
                size_t output_position;
            };

            uint32_t read_be32(const uint8_t* bytes)
            {
                return (static_cast<uint32_t>(bytes[0]) << 24) |
                       (static_cast<uint32_t>(bytes[1]) << 16) |
                       (static_cast<uint32_t>(bytes[2]) << 8) |
                       static_cast<uint32_t>(bytes[3]);
            }

            uint16_t read_be16(const uint8_t* bytes)
            {
                return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                                             bytes[1]);
            }

            bool multiply_size(size_t left, size_t right, size_t& result)
            {
                if (right != 0 && left > static_cast<size_t>(-1) / right)
                {
                    return false;
                }
                result = left * right;
                return true;
            }

            bool add_size(size_t left, size_t right, size_t& result)
            {
                if (left > static_cast<size_t>(-1) - right)
                {
                    return false;
                }
                result = left + right;
                return true;
            }

            uint32_t crc32(const uint8_t* data, size_t size)
            {
                uint32_t crc = 0xFFFFFFFF;
                for (size_t index = 0; index < size; ++index)
                {
                    crc ^= data[index];
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
                    }
                }
                return ~crc;
            }

            bool valid_bit_depth(uint8_t color_type, uint8_t bit_depth)
            {
                switch (color_type)
                {
                    case 0:
                        return bit_depth == 1 || bit_depth == 2 || bit_depth == 4 ||
                               bit_depth == 8 || bit_depth == 16;
                    case 2:
                        return bit_depth == 8 || bit_depth == 16;
                    case 3:
                        return bit_depth == 1 || bit_depth == 2 || bit_depth == 4 ||
                               bit_depth == 8;
                    case 4:
                    case 6:
                        return bit_depth == 8 || bit_depth == 16;
                    default:
                        return false;
                }
            }

            uint8_t channels_for_color_type(uint8_t color_type)
            {
                switch (color_type)
                {
                    case 0: return 1;
                    case 2: return 3;
                    case 3: return 1;
                    case 4: return 2;
                    case 6: return 4;
                    default: return 0;
                }
            }

            bool parse_png(const uint8_t* png, size_t size, PngInfo& info,
                           size_t& idat_size, bool& has_idat, Error& error)
            {
                if (size < sizeof(PNG_SIGNATURE))
                {
                    error = Error::InvalidSignature;
                    return false;
                }
                for (size_t index = 0; index < sizeof(PNG_SIGNATURE); ++index)
                {
                    if (png[index] != PNG_SIGNATURE[index])
                    {
                        error = Error::InvalidSignature;
                        return false;
                    }
                }

                size_t position = sizeof(PNG_SIGNATURE);
                bool has_header = false;
                bool has_end = false;
                bool saw_idat = false;
                while (position < size)
                {
                    if (size - position < 12)
                    {
                        error = Error::InvalidChunk;
                        return false;
                    }

                    const uint32_t chunk_size = read_be32(png + position);
                    const uint32_t type = read_be32(png + position + 4);
                    position += 8;
                    if (chunk_size > size - position - 4)
                    {
                        error = Error::InvalidChunk;
                        return false;
                    }

                    const uint8_t* data = png + position;
                    const uint32_t expected_crc = read_be32(data + chunk_size);
                    if (crc32(png + position - 4, static_cast<size_t>(chunk_size) + 4) !=
                        expected_crc)
                    {
                        error = Error::InvalidChunk;
                        return false;
                    }

                    if (type == 0x49484452U)
                    {
                        if (has_header || position != sizeof(PNG_SIGNATURE) + 8 || chunk_size != 13)
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                        info.width = read_be32(data);
                        info.height = read_be32(data + 4);
                        info.bit_depth = data[8];
                        info.color_type = data[9];
                        if (data[10] != 0 || data[11] != 0 || data[12] > 1)
                        {
                            error = Error::UnsupportedFormat;
                            return false;
                        }
                        info.interlace = data[12];
                        if (info.width == 0 || info.height == 0 ||
                            info.width > MAX_DIMENSION || info.height > MAX_DIMENSION ||
                            !valid_bit_depth(info.color_type, info.bit_depth))
                        {
                            error = Error::UnsupportedFormat;
                            return false;
                        }
                        has_header = true;
                    }
                    else if (type == 0x504C5445U)
                    {
                        if (!has_header || saw_idat || chunk_size == 0 ||
                            chunk_size > sizeof(info.palette) || chunk_size % 3 != 0)
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                        for (uint32_t index = 0; index < chunk_size; ++index)
                        {
                            info.palette[index] = data[index];
                        }
                        info.palette_size = static_cast<uint16_t>(chunk_size / 3);
                        info.has_palette = true;
                    }
                    else if (type == 0x74524E53U)
                    {
                        if (!has_header || saw_idat)
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                        if (info.color_type == 3)
                        {
                            if (chunk_size > sizeof(info.transparency))
                            {
                                error = Error::InvalidChunk;
                                return false;
                            }
                            for (uint32_t index = 0; index < chunk_size; ++index)
                            {
                                info.transparency[index] = data[index];
                            }
                            info.transparency_size = static_cast<uint16_t>(chunk_size);
                        }
                        else if (info.color_type == 0 && chunk_size == 2)
                        {
                            info.transparent_gray = read_be16(data);
                            info.has_transparent_gray = true;
                        }
                        else if (info.color_type == 2 && chunk_size == 6)
                        {
                            info.transparent_red = read_be16(data);
                            info.transparent_green = read_be16(data + 2);
                            info.transparent_blue = read_be16(data + 4);
                            info.has_transparent_rgb = true;
                        }
                        else
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                    }
                    else if (type == 0x49444154U)
                    {
                        if (!has_header)
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                        saw_idat = true;
                        if (!add_size(idat_size, chunk_size, idat_size))
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                    }
                    else if (type == 0x49454E44U)
                    {
                        if (chunk_size != 0 || !saw_idat)
                        {
                            error = Error::InvalidChunk;
                            return false;
                        }
                        has_end = true;
                        position += chunk_size + 4;
                        break;
                    }
                    else if ((png[position - 4] & 0x20U) == 0)
                    {
                        error = Error::UnsupportedFormat;
                        return false;
                    }

                    position += chunk_size + 4;
                }

                if (!has_header || !has_end || !saw_idat ||
                    (info.color_type == 3 && !info.has_palette))
                {
                    error = Error::InvalidChunk;
                    return false;
                }
                has_idat = true;
                return true;
            }

            bool gather_idat(const uint8_t* png, size_t size, uint8_t* output,
                             size_t output_size)
            {
                size_t position = sizeof(PNG_SIGNATURE);
                size_t written = 0;
                while (position + 12 <= size)
                {
                    const uint32_t chunk_size = read_be32(png + position);
                    const uint32_t type = read_be32(png + position + 4);
                    position += 8;
                    if (chunk_size > size - position - 4)
                    {
                        return false;
                    }
                    if (type == 0x49444154U)
                    {
                        if (chunk_size > output_size - written)
                        {
                            return false;
                        }
                        for (uint32_t index = 0; index < chunk_size; ++index)
                        {
                            output[written++] = png[position + index];
                        }
                    }
                    position += chunk_size + 4;
                    if (type == 0x49454E44U)
                    {
                        break;
                    }
                }
                return written == output_size;
            }

            bool read_bits(BitReader& reader, uint8_t count, uint32_t& value)
            {
                while (reader.bit_count < count)
                {
                    if (reader.position >= reader.size)
                    {
                        reader.failed = true;
                        return false;
                    }
                    reader.bits |= static_cast<uint64_t>(reader.input[reader.position++])
                                   << reader.bit_count;
                    reader.bit_count = static_cast<uint8_t>(reader.bit_count + 8);
                }

                value = static_cast<uint32_t>(reader.bits & ((1ULL << count) - 1));
                reader.bits >>= count;
                reader.bit_count = static_cast<uint8_t>(reader.bit_count - count);
                return true;
            }

            bool build_huffman(Huffman& table, const uint8_t* lengths, uint16_t count)
            {
                for (uint16_t index = 0; index < count; ++index)
                {
                    if (lengths[index] > 15)
                    {
                        return false;
                    }
                    ++table.count[lengths[index]];
                }
                table.count[0] = 0;

                int32_t available = 1;
                for (uint8_t length = 1; length <= 15; ++length)
                {
                    available = (available << 1) - table.count[length];
                    if (available < 0)
                    {
                        return false;
                    }
                }

                uint16_t offsets[16] = {};
                for (uint8_t length = 1; length < 15; ++length)
                {
                    offsets[length + 1] = static_cast<uint16_t>(
                        offsets[length] + table.count[length]);
                }
                for (uint16_t symbol = 0; symbol < count; ++symbol)
                {
                    if (lengths[symbol] != 0)
                    {
                        table.symbols[offsets[lengths[symbol]]++] = symbol;
                    }
                }
                return true;
            }

            bool decode_symbol(BitReader& reader, const Huffman& table, uint16_t& symbol)
            {
                uint32_t code = 0;
                uint32_t first = 0;
                uint32_t index = 0;
                for (uint8_t length = 1; length <= 15; ++length)
                {
                    uint32_t bit = 0;
                    if (!read_bits(reader, 1, bit))
                    {
                        return false;
                    }
                    code |= bit;
                    const uint32_t count = table.count[length];
                    if (code >= first && code - first < count)
                    {
                        symbol = table.symbols[index + code - first];
                        return true;
                    }
                    index += count;
                    first = (first + count) << 1;
                    code <<= 1;
                }
                return false;
            }

            bool emit_byte(Inflater& inflater, uint8_t value)
            {
                if (inflater.output_position >= inflater.output_size)
                {
                    return false;
                }
                inflater.output[inflater.output_position++] = value;
                return true;
            }

            bool inflate_codes(Inflater& inflater, const Huffman& literal_table,
                               const Huffman& distance_table)
            {
                for (;;)
                {
                    uint16_t symbol = 0;
                    if (!decode_symbol(inflater.reader, literal_table, symbol))
                    {
                        return false;
                    }
                    if (symbol < 256)
                    {
                        if (!emit_byte(inflater, static_cast<uint8_t>(symbol)))
                        {
                            return false;
                        }
                        continue;
                    }
                    if (symbol == 256)
                    {
                        return true;
                    }
                    if (symbol < 257 || symbol > 285)
                    {
                        return false;
                    }

                    const uint16_t length_index = static_cast<uint16_t>(symbol - 257);
                    uint32_t extra = 0;
                    if (!read_bits(inflater.reader, LENGTH_EXTRA[length_index], extra))
                    {
                        return false;
                    }
                    const uint32_t length = LENGTH_BASE[length_index] + extra;

                    uint16_t distance_symbol = 0;
                    if (!decode_symbol(inflater.reader, distance_table, distance_symbol) ||
                        distance_symbol >= 30)
                    {
                        return false;
                    }
                    if (!read_bits(inflater.reader, DISTANCE_EXTRA[distance_symbol], extra))
                    {
                        return false;
                    }
                    const uint32_t distance = DISTANCE_BASE[distance_symbol] + extra;
                    if (distance == 0 || distance > inflater.output_position)
                    {
                        return false;
                    }

                    for (uint32_t index = 0; index < length; ++index)
                    {
                        if (!emit_byte(inflater,
                                       inflater.output[inflater.output_position - distance]))
                        {
                            return false;
                        }
                    }
                }
            }

            bool inflate_dynamic_block(Inflater& inflater)
            {
                uint32_t value = 0;
                if (!read_bits(inflater.reader, 5, value))
                {
                    return false;
                }
                const uint16_t literal_count = static_cast<uint16_t>(value + 257);
                if (!read_bits(inflater.reader, 5, value))
                {
                    return false;
                }
                const uint16_t distance_count = static_cast<uint16_t>(value + 1);
                if (!read_bits(inflater.reader, 4, value))
                {
                    return false;
                }
                const uint8_t code_length_count = static_cast<uint8_t>(value + 4);
                if (literal_count > 286 || distance_count > 32)
                {
                    return false;
                }

                uint8_t code_lengths[19] = {};
                for (uint8_t index = 0; index < code_length_count; ++index)
                {
                    if (!read_bits(inflater.reader, 3, value))
                    {
                        return false;
                    }
                    code_lengths[CODE_LENGTH_ORDER[index]] = static_cast<uint8_t>(value);
                }

                Huffman code_table{};
                if (!build_huffman(code_table, code_lengths, 19))
                {
                    return false;
                }

                uint8_t lengths[286 + 32] = {};
                const uint16_t total = literal_count + distance_count;
                uint16_t position = 0;
                while (position < total)
                {
                    uint16_t symbol = 0;
                    if (!decode_symbol(inflater.reader, code_table, symbol))
                    {
                        return false;
                    }
                    if (symbol <= 15)
                    {
                        lengths[position++] = static_cast<uint8_t>(symbol);
                    }
                    else if (symbol == 16)
                    {
                        if (position == 0 || !read_bits(inflater.reader, 2, value))
                        {
                            return false;
                        }
                        const uint16_t repeat = static_cast<uint16_t>(value + 3);
                        if (repeat > total - position)
                        {
                            return false;
                        }
                        const uint8_t previous = lengths[position - 1];
                        for (uint16_t index = 0; index < repeat; ++index)
                        {
                            lengths[position++] = previous;
                        }
                    }
                    else if (symbol == 17 || symbol == 18)
                    {
                        const uint8_t extra_bits = symbol == 17 ? 3 : 7;
                        const uint16_t base = symbol == 17 ? 3 : 11;
                        if (!read_bits(inflater.reader, extra_bits, value))
                        {
                            return false;
                        }
                        const uint16_t repeat = static_cast<uint16_t>(value + base);
                        if (repeat > total - position)
                        {
                            return false;
                        }
                        for (uint16_t index = 0; index < repeat; ++index)
                        {
                            lengths[position++] = 0;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }

                if (lengths[256] == 0)
                {
                    return false;
                }

                Huffman literal_table{};
                Huffman distance_table{};
                if (!build_huffman(literal_table, lengths, literal_count) ||
                    !build_huffman(distance_table, lengths + literal_count, distance_count))
                {
                    return false;
                }
                return inflate_codes(inflater, literal_table, distance_table);
            }

            bool inflate_zlib(const uint8_t* input, size_t input_size,
                              uint8_t* output, size_t output_size)
            {
                if (input_size < 6)
                {
                    return false;
                }
                const uint8_t cmf = input[0];
                const uint8_t flags = input[1];
                if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 ||
                    ((static_cast<uint16_t>(cmf) << 8) | flags) % 31 != 0 ||
                    (flags & 0x20) != 0)
                {
                    return false;
                }

                Inflater inflater{};
                inflater.reader.input = input;
                inflater.reader.size = input_size - 4;
                inflater.reader.position = 2;
                inflater.output = output;
                inflater.output_size = output_size;

                bool final_block = false;
                while (!final_block)
                {
                    uint32_t value = 0;
                    if (!read_bits(inflater.reader, 1, value))
                    {
                        return false;
                    }
                    final_block = value != 0;
                    if (!read_bits(inflater.reader, 2, value))
                    {
                        return false;
                    }

                    if (value == 0)
                    {
                        inflater.reader.bits = 0;
                        inflater.reader.bit_count = 0;
                        if (inflater.reader.position + 4 > inflater.reader.size)
                        {
                            return false;
                        }
                        const uint16_t length = static_cast<uint16_t>(
                            inflater.reader.input[inflater.reader.position] |
                            (static_cast<uint16_t>(inflater.reader.input[inflater.reader.position + 1]) << 8));
                        const uint16_t complement = static_cast<uint16_t>(
                            inflater.reader.input[inflater.reader.position + 2] |
                            (static_cast<uint16_t>(inflater.reader.input[inflater.reader.position + 3]) << 8));
                        inflater.reader.position += 4;
                        if (static_cast<uint16_t>(~length) != complement ||
                            length > inflater.output_size - inflater.output_position ||
                            length > inflater.reader.size - inflater.reader.position)
                        {
                            return false;
                        }
                        for (uint16_t index = 0; index < length; ++index)
                        {
                            if (!emit_byte(inflater,
                                           inflater.reader.input[inflater.reader.position++]))
                            {
                                return false;
                            }
                        }
                    }
                    else if (value == 1)
                    {
                        uint8_t literal_lengths[288];
                        uint8_t distance_lengths[32];
                        for (uint16_t symbol = 0; symbol <= 287; ++symbol)
                        {
                            literal_lengths[symbol] = symbol <= 143 ? 8 :
                                symbol <= 255 ? 9 : symbol <= 279 ? 7 : 8;
                        }
                        for (uint16_t symbol = 0; symbol < 32; ++symbol)
                        {
                            distance_lengths[symbol] = 5;
                        }
                        Huffman literal_table{};
                        Huffman distance_table{};
                        if (!build_huffman(literal_table, literal_lengths, 288) ||
                            !build_huffman(distance_table, distance_lengths, 32) ||
                            !inflate_codes(inflater, literal_table, distance_table))
                        {
                            return false;
                        }
                    }
                    else if (value == 2)
                    {
                        if (!inflate_dynamic_block(inflater))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }

                if (inflater.output_position != output_size)
                {
                    return false;
                }

                inflater.reader.bits = 0;
                inflater.reader.bit_count = 0;
                if (inflater.reader.position + 4 > input_size)
                {
                    return false;
                }

                uint32_t a = 1;
                uint32_t b = 0;
                for (size_t index = 0; index < output_size; ++index)
                {
                    a = (a + output[index]) % 65521;
                    b = (b + a) % 65521;
                }
                const uint32_t expected_adler = (b << 16) | a;
                return read_be32(input + inflater.reader.position) == expected_adler;
            }

            uint32_t pass_extent(uint32_t extent, uint32_t start, uint32_t step)
            {
                return extent <= start ? 0 : (extent - start + step - 1) / step;
            }

            bool calculate_decoded_size(const PngInfo& info, size_t& decoded_size)
            {
                const uint8_t channels = channels_for_color_type(info.color_type);
                const uint64_t bits_per_pixel =
                    static_cast<uint64_t>(channels) * info.bit_depth;
                decoded_size = 0;

                const uint8_t pass_count = info.interlace == 0 ? 1 : 7;
                for (uint8_t pass = 0; pass < pass_count; ++pass)
                {
                    const uint32_t start_x = info.interlace == 0 ? 0 : ADAM7_X[pass];
                    const uint32_t start_y = info.interlace == 0 ? 0 : ADAM7_Y[pass];
                    const uint32_t step_x = info.interlace == 0 ? 1 : ADAM7_DX[pass];
                    const uint32_t step_y = info.interlace == 0 ? 1 : ADAM7_DY[pass];
                    const uint32_t width = pass_extent(info.width, start_x, step_x);
                    const uint32_t height = pass_extent(info.height, start_y, step_y);
                    if (width == 0 || height == 0)
                    {
                        continue;
                    }

                    const uint64_t row_size_64 =
                        (static_cast<uint64_t>(width) * bits_per_pixel + 7) / 8;
                    if (row_size_64 >
                        static_cast<uint64_t>(static_cast<size_t>(-1)))
                    {
                        return false;
                    }
                    size_t row_with_filter = 0;
                    size_t pass_size = 0;
                    if (!add_size(static_cast<size_t>(row_size_64), 1, row_with_filter) ||
                        !multiply_size(row_with_filter, height, pass_size) ||
                        !add_size(decoded_size, pass_size, decoded_size) ||
                        decoded_size > MAX_IMAGE_BYTES)
                    {
                        return false;
                    }
                }
                return true;
            }

            uint8_t paeth(uint8_t left, uint8_t above, uint8_t upper_left)
            {
                const int32_t estimate = static_cast<int32_t>(left) + above - upper_left;
                const int32_t left_distance = estimate > left ? estimate - left : left - estimate;
                const int32_t above_distance = estimate > above ? estimate - above : above - estimate;
                const int32_t upper_distance = estimate > upper_left
                    ? estimate - upper_left
                    : upper_left - estimate;
                if (left_distance <= above_distance && left_distance <= upper_distance)
                {
                    return left;
                }
                return above_distance <= upper_distance ? above : upper_left;
            }

            bool unfilter_row(uint8_t filter, uint8_t* row, const uint8_t* previous,
                              size_t row_size, size_t bytes_per_pixel)
            {
                for (size_t index = 0; index < row_size; ++index)
                {
                    const uint8_t left = index >= bytes_per_pixel
                        ? row[index - bytes_per_pixel]
                        : 0;
                    const uint8_t above = previous == nullptr ? 0 : previous[index];
                    const uint8_t upper_left =
                        previous != nullptr && index >= bytes_per_pixel
                            ? previous[index - bytes_per_pixel]
                            : 0;
                    uint8_t predictor = 0;
                    switch (filter)
                    {
                        case 0: break;
                        case 1: predictor = left; break;
                        case 2: predictor = above; break;
                        case 3: predictor = static_cast<uint8_t>(
                            (static_cast<uint16_t>(left) + above) / 2); break;
                        case 4: predictor = paeth(left, above, upper_left); break;
                        default: return false;
                    }
                    row[index] = static_cast<uint8_t>(row[index] + predictor);
                }
                return true;
            }

            uint16_t sample_at(const uint8_t* row, uint32_t pixel, uint8_t channel,
                               uint8_t channels, uint8_t bit_depth)
            {
                const uint64_t sample_index =
                    static_cast<uint64_t>(pixel) * channels + channel;
                if (bit_depth == 16)
                {
                    const size_t offset = static_cast<size_t>(sample_index * 2);
                    return read_be16(row + offset);
                }
                if (bit_depth == 8)
                {
                    return row[sample_index];
                }
                const uint32_t bit_offset =
                    static_cast<uint32_t>(sample_index) * bit_depth;
                const uint8_t shift = static_cast<uint8_t>(
                    8 - bit_depth - (bit_offset % 8));
                return static_cast<uint16_t>(
                    (row[bit_offset / 8] >> shift) & ((1U << bit_depth) - 1));
            }

            uint8_t scale_sample(uint16_t sample, uint8_t bit_depth)
            {
                if (bit_depth == 16)
                {
                    return static_cast<uint8_t>(sample >> 8);
                }
                if (bit_depth == 8)
                {
                    return static_cast<uint8_t>(sample);
                }
                const uint32_t maximum = (1U << bit_depth) - 1;
                return static_cast<uint8_t>((static_cast<uint32_t>(sample) * 255 +
                                             maximum / 2) / maximum);
            }

            bool store_pixel(const PngInfo& info, const uint8_t* row,
                             uint32_t source_x, uint32_t destination_x,
                             uint32_t destination_y, uint8_t* output)
            {
                const uint8_t channels = channels_for_color_type(info.color_type);
                uint8_t red = 0;
                uint8_t green = 0;
                uint8_t blue = 0;
                uint8_t alpha = 255;

                switch (info.color_type)
                {
                    case 0:
                    {
                        const uint16_t gray = sample_at(row, source_x, 0, 1, info.bit_depth);
                        red = green = blue = scale_sample(gray, info.bit_depth);
                        if (info.has_transparent_gray && gray == info.transparent_gray)
                        {
                            alpha = 0;
                        }
                        break;
                    }
                    case 2:
                    {
                        const uint16_t source_red =
                            sample_at(row, source_x, 0, channels, info.bit_depth);
                        const uint16_t source_green =
                            sample_at(row, source_x, 1, channels, info.bit_depth);
                        const uint16_t source_blue =
                            sample_at(row, source_x, 2, channels, info.bit_depth);
                        red = scale_sample(source_red, info.bit_depth);
                        green = scale_sample(source_green, info.bit_depth);
                        blue = scale_sample(source_blue, info.bit_depth);
                        if (info.has_transparent_rgb &&
                            source_red == info.transparent_red &&
                            source_green == info.transparent_green &&
                            source_blue == info.transparent_blue)
                        {
                            alpha = 0;
                        }
                        break;
                    }
                    case 3:
                    {
                        const uint16_t palette_index =
                            sample_at(row, source_x, 0, 1, info.bit_depth);
                        if (palette_index >= info.palette_size)
                        {
                            return false;
                        }
                        red = info.palette[palette_index * 3];
                        green = info.palette[palette_index * 3 + 1];
                        blue = info.palette[palette_index * 3 + 2];
                        if (palette_index < info.transparency_size)
                        {
                            alpha = info.transparency[palette_index];
                        }
                        break;
                    }
                    case 4:
                    {
                        const uint16_t gray =
                            sample_at(row, source_x, 0, channels, info.bit_depth);
                        red = green = blue = scale_sample(gray, info.bit_depth);
                        alpha = scale_sample(
                            sample_at(row, source_x, 1, channels, info.bit_depth),
                            info.bit_depth);
                        break;
                    }
                    case 6:
                        red = scale_sample(sample_at(row, source_x, 0, channels, info.bit_depth),
                                           info.bit_depth);
                        green = scale_sample(sample_at(row, source_x, 1, channels, info.bit_depth),
                                             info.bit_depth);
                        blue = scale_sample(sample_at(row, source_x, 2, channels, info.bit_depth),
                                            info.bit_depth);
                        alpha = scale_sample(sample_at(row, source_x, 3, channels, info.bit_depth),
                                             info.bit_depth);
                        break;
                    default:
                        return false;
                }

                const size_t output_offset =
                    (static_cast<size_t>(destination_y) * info.width + destination_x) * 4;
                output[output_offset] = red;
                output[output_offset + 1] = green;
                output[output_offset + 2] = blue;
                output[output_offset + 3] = alpha;
                return true;
            }

            bool convert_scanlines(const PngInfo& info, const uint8_t* decoded,
                                   uint8_t* output, const Allocator& allocator,
                                   Error& error)
            {
                const uint8_t channels = channels_for_color_type(info.color_type);
                const size_t bits_per_pixel =
                    static_cast<size_t>(channels) * info.bit_depth;
                const size_t filter_bpp = (bits_per_pixel + 7) / 8 > 1
                    ? (bits_per_pixel + 7) / 8
                    : 1;
                const size_t row_size =
                    (static_cast<size_t>(info.width) * bits_per_pixel + 7) / 8;
                size_t scratch_size = 0;
                if (!multiply_size(row_size, 2, scratch_size))
                {
                    return false;
                }
                uint8_t* scratch = static_cast<uint8_t*>(allocator.allocate(scratch_size));
                if (scratch == nullptr)
                {
                    error = Error::OutOfMemory;
                    return false;
                }
                uint8_t* previous_row = scratch;
                uint8_t* current_row = scratch + row_size;
                size_t position = 0;

                const uint8_t pass_count = info.interlace == 0 ? 1 : 7;
                for (uint8_t pass = 0; pass < pass_count; ++pass)
                {
                    const uint32_t start_x = info.interlace == 0 ? 0 : ADAM7_X[pass];
                    const uint32_t start_y = info.interlace == 0 ? 0 : ADAM7_Y[pass];
                    const uint32_t step_x = info.interlace == 0 ? 1 : ADAM7_DX[pass];
                    const uint32_t step_y = info.interlace == 0 ? 1 : ADAM7_DY[pass];
                    const uint32_t pass_width = pass_extent(info.width, start_x, step_x);
                    const uint32_t pass_height = pass_extent(info.height, start_y, step_y);
                    if (pass_width == 0 || pass_height == 0)
                    {
                        continue;
                    }

                    const size_t pass_row_size =
                        (static_cast<size_t>(pass_width) * bits_per_pixel + 7) / 8;
                    for (size_t index = 0; index < row_size; ++index)
                    {
                        previous_row[index] = 0;
                    }

                    for (uint32_t row_index = 0; row_index < pass_height; ++row_index)
                    {
                        if (position >= MAX_IMAGE_BYTES)
                        {
                            allocator.deallocate(scratch);
                            return false;
                        }
                        const uint8_t filter = decoded[position++];
                        if (pass_row_size > MAX_IMAGE_BYTES - position)
                        {
                            allocator.deallocate(scratch);
                            return false;
                        }
                        for (size_t index = 0; index < pass_row_size; ++index)
                        {
                            current_row[index] = decoded[position + index];
                        }
                        position += pass_row_size;

                        if (!unfilter_row(filter, current_row,
                                          row_index == 0 ? nullptr : previous_row,
                                          pass_row_size, filter_bpp))
                        {
                            error = Error::InvalidImageData;
                            allocator.deallocate(scratch);
                            return false;
                        }

                        const uint32_t destination_y = start_y + row_index * step_y;
                        for (uint32_t column = 0; column < pass_width; ++column)
                        {
                            const uint32_t destination_x = start_x + column * step_x;
                            if (!store_pixel(info, current_row, column, destination_x,
                                             destination_y, output))
                            {
                                error = Error::InvalidImageData;
                                allocator.deallocate(scratch);
                                return false;
                            }
                        }

                        for (size_t index = 0; index < pass_row_size; ++index)
                        {
                            previous_row[index] = current_row[index];
                        }
                    }
                }
                allocator.deallocate(scratch);
                return true;
            }

            void set_error(Error* output, Error error)
            {
                if (output != nullptr)
                {
                    *output = error;
                }
            }
        }

        bool decode(const uint8_t* png, size_t png_size, const Allocator& allocator,
                    Image* image, Error* error)
        {
            if (image != nullptr)
            {
                *image = {};
            }
            set_error(error, Error::None);
            if (png == nullptr || image == nullptr || allocator.allocate == nullptr ||
                allocator.deallocate == nullptr)
            {
                set_error(error, Error::InvalidArgument);
                return false;
            }

            PngInfo info{};
            size_t idat_size = 0;
            bool has_idat = false;
            Error parse_error = Error::None;
            if (!parse_png(png, png_size, info, idat_size, has_idat, parse_error))
            {
                set_error(error, parse_error);
                return false;
            }

            size_t decoded_size = 0;
            size_t pixel_count = 0;
            size_t rgba_size = 0;
            if (!calculate_decoded_size(info, decoded_size) ||
                !multiply_size(info.width, info.height, pixel_count) ||
                !multiply_size(pixel_count, 4, rgba_size) ||
                rgba_size > MAX_IMAGE_BYTES || decoded_size == 0 || idat_size < 6)
            {
                set_error(error, Error::UnsupportedFormat);
                return false;
            }

            uint8_t* compressed = static_cast<uint8_t*>(allocator.allocate(idat_size));
            if (compressed == nullptr)
            {
                set_error(error, Error::OutOfMemory);
                return false;
            }
            if (!gather_idat(png, png_size, compressed, idat_size))
            {
                allocator.deallocate(compressed);
                set_error(error, Error::InvalidChunk);
                return false;
            }

            uint8_t* decoded = static_cast<uint8_t*>(allocator.allocate(decoded_size));
            if (decoded == nullptr)
            {
                allocator.deallocate(compressed);
                set_error(error, Error::OutOfMemory);
                return false;
            }
            if (!inflate_zlib(compressed, idat_size, decoded, decoded_size))
            {
                allocator.deallocate(decoded);
                allocator.deallocate(compressed);
                set_error(error, Error::InvalidImageData);
                return false;
            }
            allocator.deallocate(compressed);

            uint8_t* rgba = static_cast<uint8_t*>(allocator.allocate(rgba_size));
            if (rgba == nullptr)
            {
                allocator.deallocate(decoded);
                set_error(error, Error::OutOfMemory);
                return false;
            }
            Error conversion_error = Error::InvalidImageData;
            if (!convert_scanlines(info, decoded, rgba, allocator, conversion_error))
            {
                allocator.deallocate(rgba);
                allocator.deallocate(decoded);
                set_error(error, conversion_error);
                return false;
            }
            allocator.deallocate(decoded);

            image->width = info.width;
            image->height = info.height;
            image->rgba = rgba;
            image->size = rgba_size;
            return true;
        }

        void release(Image* image, const Allocator& allocator)
        {
            if (image == nullptr)
            {
                return;
            }
            if (image->rgba != nullptr && allocator.deallocate != nullptr)
            {
                allocator.deallocate(image->rgba);
            }
            *image = {};
        }
    }
}
