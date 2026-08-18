#include <napp.h>

NAPP_APPLICATION("ls");

int _start(const napp_api* api)
{
    const char* path = api->current_path;
    if (api->argc > 1)
    {
        path = api->argv[1];
    }
    api->clawfs_dir(path);
    return 0;
}
