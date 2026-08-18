#include <napp.h>

NAPP_APPLICATION("cat");

int _start(const napp_api* api)
{
    if (api->argc < 2)
    {
        api->print("Usage: cat <file>\n");
        return 1;
    }

    const char* path = api->argv[1];
    uint32_t sector = api->clawfs_resolve_path(api->current_path, path);

    if (sector == 0)
    {
        api->print("File not found!\n");
        return 1;
    }

    char buffer[512];
    if (api->clawfs_read_sector(sector, buffer))
    {
        for (int i = 0; i < 512; i++)
        {
            if (buffer[i] == '\0')
            {
                break;
            }
            char c[2] = { buffer[i], 0 };
            api->print(c);
        }
        api->print("\n");
    }
    else
    {
        api->print("Failed to read file contents!\n");
        return 1;
    }

    return 0;
}
