#include <napp.h>

NAPP_APPLICATION("mkdir");

static bool is_flag(const char* arg, const char* flag)
{
    int i = 0;
    while (flag[i] && arg[i] == flag[i]) i++;
    return flag[i] == '\0' && arg[i] == '\0';
}

int _start(const napp_api* api)
{
    const char* dir_name = nullptr;

    // Only accept flag format
    for (int i = 1; i < api->argc; i++)
    {
        if (is_flag(api->argv[i], "--dir_name") && i + 1 < api->argc)
        {
            dir_name = api->argv[i + 1];
            i++;
        }
    }

    if (dir_name == nullptr || *dir_name == '\0')
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: mkdir --dir_name <folder_name>\n");
        return 1;
    }

    api->clawfs_mkdir(api->current_path, dir_name);
    return 0;
}
