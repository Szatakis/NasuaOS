#include <napp.h>

NAPP_APPLICATION("ls");

int _start(const napp_api* api)
{
    const char* path = api->current_path;

    if (api->argc == 1)
    {
        api->clawfs_dir(path);
        return 0;
    }

    if (api->argc != 3)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: ls --file <directory_path>\n");
        return 1;
    }

    if (api->argv[1][0] != '-' ||
        api->argv[1][1] != '-' ||
        api->argv[1][2] != 'f' ||
        api->argv[1][3] != 'i' ||
        api->argv[1][4] != 'l' ||
        api->argv[1][5] != 'e' ||
        api->argv[1][6] != '\0')
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: ls --file \"directory_path\"\n");
        return 1;
    }

    path = api->argv[2];

    api->clawfs_dir(path);
    return 0;
}
