#include <napp.h>

NAPP_APPLICATION("rm");

static bool strcmp(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 == *s2;
}

static bool is_flag(const char* arg, const char* flag)
{
    int i = 0;
    while (flag[i] && arg[i] == flag[i]) i++;
    return flag[i] == '\0' && arg[i] == '\0';
}

int _start(const napp_api* api)
{
    const char* name = nullptr;
    int type = -1;

    // Only accept flag format
    for (int i = 1; i < api->argc; i++)
    {
        if (is_flag(api->argv[i], "--name") && i + 1 < api->argc)
        {
            name = api->argv[i + 1];
            i++;
        }
        else if (is_flag(api->argv[i], "--type") && i + 1 < api->argc)
        {
            const char* type_str = api->argv[i + 1];
            if (strcmp(type_str, "file")) type = 0; // CLAWFS_FILE
            else if (strcmp(type_str, "dir")) type = 1; // CLAWFS_DIRECTORY
            i++;
        }
    }

    if (name == nullptr || *name == '\0')
    {
        api->print("Syntax error: rm requires --name flag\n");
        api->print("Usage: rm --name <name> --type <file|dir>\n");
        return 1;
    }

    if (type == -1)
    {
        type = api->clawfs_get_entry_type(api->current_path, name);
        if (type == -1)
        {
            api->print("Error: File or directory not found!\n");
            return 1;
        }
    }

    api->clawfs_rm(api->current_path, name, type);
    return 0;
}
