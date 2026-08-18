#include <napp.h>

NAPP_APPLICATION("mkdir");

static bool strcmp(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 == *s2;
}

int _start(const napp_api* api)
{
    const char* dir_name = nullptr;
    for (int i = 1; i < api->argc; i++)
    {
        if (strcmp(api->argv[i], "--dir_name") && i + 1 < api->argc)
        {
            dir_name = api->argv[i + 1];
            break;
        }
        else if (api->argv[i][0] != '-')
        {
            dir_name = api->argv[i];
        }
    }

    if (dir_name == nullptr || *dir_name == '\0')
    {
        api->print("Usage: mkdir --dir_name <folder_name>\n");
        return 1;
    }

    api->clawfs_mkdir(api->current_path, dir_name);
    return 0;
}
