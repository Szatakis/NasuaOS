#include <napp.h>

NAPP_APPLICATION("cat", "Display the contents of one or more files", false);

static bool is_flag(const char* arg, const char* flag)
{
    int i = 0;

    while (flag[i] && arg[i] == flag[i])
        i++;

    return flag[i] == '\0' && arg[i] == '\0';
}

int _start(const napp_api* api)
{
    if (api->argc < 3)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: cat --file <file> [--file_n <file>] ...\n");
        return 1;
    }

    bool any_success = false;
    int i = 1;
    bool first_file = true;

    while (i < api->argc)
    {
        const char* flag = api->argv[i];

        // First file must use --file
        if (first_file)
        {
            if (!is_flag(flag, "--file"))
            {
                api->print_error("Syntax error!\n");
                api->print("Usage: cat --file <file> [--file_n <file>] ...\n");
                return 1;
            }

            first_file = false;
        }
        // Every next file must use --file_n
        else
        {
            if (!is_flag(flag, "--file_n"))
            {
                api->print_error("Syntax error!\n");
                api->print("Usage: cat --file <file> [--file_n <file>] ...\n");
                return 1;
            }
        }

        // Flag must have a filename after it
        if (i + 1 >= api->argc)
        {
            api->print_error("Syntax error!\n");
            api->print("Usage: cat --file <file> [--file_n <file>] ...\n");
            return 1;
        }

        const char* path = api->argv[i + 1];

        // Filename cannot be another flag
        if (path[0] == '-')
        {
            api->print_error("Syntax error!\n");
            api->print("Usage: cat --file <file> [--file_n <file>] ...\n");
            return 1;
        }

        uint32_t sector =
            api->clawfs_resolve_path(api->current_path, path);

        if (sector == 0)
        {
            api->print("Error: File not found: ");
            api->print(path);
            api->print("\n");

            i += 2;
            continue;
        }

        char buffer[512];

        if (!api->clawfs_read_sector(sector, buffer))
        {
            api->print_error("Failed to read file: ");
            api->print(path);
            api->print("\n");

            i += 2;
            continue;
        }

        for (int j = 0; j < 512; j++)
        {
            if (buffer[j] == '\0')
                break;

            char c[2] = { buffer[j], '\0' };
            api->print(c);
        }

        api->print("\n");

        any_success = true;

        i += 2;
    }

    return any_success ? 0 : 1;
}