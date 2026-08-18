#include <napp.h>

NAPP_APPLICATION("ls");

int _start(const napp_api* api)
{
    const char* path = api->current_path;
    
    if (api->argc > 1)
    {
        // Check for flag usage (incorrect syntax)
        if (api->argv[1][0] == '-')
        {
            api->print("Syntax error: ls uses positional arguments, not flags\n");
            api->print("Usage: ls [directory_path]\n");
            return 1;
        }
        path = api->argv[1];
    }
    
    api->clawfs_dir(path);
    return 0;
}
