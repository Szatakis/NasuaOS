#include <napp.h>

NAPP_APPLICATION("cat");

int _start(const napp_api* api)
{
    if (api->argc < 2)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: cat <file1> [file2] ...\n");
        return 1;
    }

    // Check for flag usage (incorrect syntax)
    if (api->argv[1][0] == '-')
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: cat <file1> [file2] ...\n");
        return 1;
    }

    bool any_success = false;

    for (int i = 1; i < api->argc; i++)
    {
        const char* path = api->argv[i];
        uint32_t sector = api->clawfs_resolve_path(api->current_path, path);

        if (sector == 0)
        {
            api->print("Error: File not found: ");
            api->print(path);
            api->print("\n");
            continue;
        }

        char buffer[512];
        if (api->clawfs_read_sector(sector, buffer))
        {
            for (int j = 0; j < 512; j++)
            {
                if (buffer[j] == '\0')
                {
                    break;
                }
                char c[2] = { buffer[j], 0 };
                api->print(c);
            }
            api->print("\n");
            any_success = true;
        }
        else
        {
            api->print_error("Failed to read file: ");
            api->print(path);
            api->print("\n");
        }
    }

    return any_success ? 0 : 1;
}
