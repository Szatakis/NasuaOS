#include <napp.h>

NAPP_APPLICATION("wc", "Count lines, words, and bytes in files", false);

namespace
{
    bool is_flag(const char* argument, const char* flag)
    {
        uint32_t index = 0;
        while (flag[index] != '\0' && argument[index] == flag[index])
        {
            ++index;
        }
        return flag[index] == '\0' && argument[index] == '\0';
    }

    struct FileCounts
    {
        uint64_t lines = 0;
        uint64_t words = 0;
        uint64_t bytes = 0;
    };

    bool is_whitespace(char character)
    {
        return character == ' ' || character == '\t' || character == '\n' ||
               character == '\r' || character == '\v' || character == '\f';
    }

    void print_count(const napp_api* api, uint64_t value)
    {
        char digits[20];
        uint32_t length = 0;

        do
        {
            digits[length++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);

        while (length > 0)
        {
            char character[2] = { digits[--length], '\0' };
            api->print(character);
        }
    }

    void print_counts(const napp_api* api, const FileCounts& counts,
                      const char* filename)
    {
        print_count(api, counts.lines);
        api->print(" ");
        print_count(api, counts.words);
        api->print(" ");
        print_count(api, counts.bytes);
        api->print(" ");
        api->print(filename);
        api->print("\n");
    }

    bool count_file(const napp_api* api, const char* path, FileCounts& counts)
    {
        uint32_t file_size = 0;
        if (!api->clawfs_get_file_size(path, &file_size))
        {
            api->print_error("wc: cannot read file: ");
            api->print(path);
            api->print("\n");
            return false;
        }

        char buffer[512];
        bool in_word = false;
        uint32_t offset = 0;

        while (offset < file_size)
        {
            uint32_t chunk_size = file_size - offset;
            if (chunk_size > sizeof(buffer))
            {
                chunk_size = sizeof(buffer);
            }

            if (!api->clawfs_read_file_range(path, offset, buffer, chunk_size))
            {
                api->print_error("wc: failed to read file: ");
                api->print(path);
                api->print("\n");
                return false;
            }

            for (uint32_t index = 0; index < chunk_size; ++index)
            {
                const char character = buffer[index];
                ++counts.bytes;

                if (character == '\n')
                {
                    ++counts.lines;
                }

                if (is_whitespace(character))
                {
                    in_word = false;
                }
                else if (!in_word)
                {
                    ++counts.words;
                    in_word = true;
                }
            }

            offset += chunk_size;
        }

        return true;
    }
}

int _start(const napp_api* api)
{
    if (api->argc < 2)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: wc --file <file> [--file <file> ...]\n");
        return 1;
    }

    FileCounts total;
    bool all_files_read = true;
    uint32_t file_count = 0;

    for (int index = 1; index < api->argc; ++index)
    {
        if (!is_flag(api->argv[index], "--file") ||
            index + 1 >= api->argc ||
            api->argv[index + 1][0] == '-')
        {
            api->print_error("Syntax error!\n");
            api->print("Usage: wc --file <file> [--file <file> ...]\n");
            return 1;
        }

        const char* path = api->argv[++index];
        FileCounts counts;
        ++file_count;
        if (!count_file(api, path, counts))
        {
            all_files_read = false;
            continue;
        }

        print_counts(api, counts, path);
        total.lines += counts.lines;
        total.words += counts.words;
        total.bytes += counts.bytes;
    }

    if (file_count > 1 && all_files_read)
    {
        print_counts(api, total, "total");
    }

    return all_files_read ? 0 : 1;
}
