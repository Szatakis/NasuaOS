#include <napp.h>

NAPP_APPLICATION("cd");

static void strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++));
}

static void strcat(char* dest, const char* src)
{
    while (*dest) dest++;
    while ((*dest++ = *src++));
}

static char* strrchr(const char* str, int ch)
{
    char* last = nullptr;
    while (*str)
    {
        if (*str == ch) last = (char*)str;
        str++;
    }
    return last;
}

static bool streq(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 == *s2;
}

int _start(const napp_api* api)
{
    const char* arg = (api->argc > 1) ? api->argv[1] : "";

    // Check for flag usage (incorrect syntax)
    if (api->argc > 1 && arg[0] == '-')
    {
        api->print("Syntax error: cd uses positional arguments, not flags\n");
        api->print("Usage: cd [directory_path]\n");
        return 1;
    }

    char new_path[256];

    if (arg[0] == '\0' || streq(arg, "~"))
    {
        strcpy(new_path, "/home");
    }
    else if (streq(arg, ".."))
    {
        strcpy(new_path, api->current_path);
        char* last_slash = strrchr(new_path, '/');
        if (last_slash != nullptr && last_slash != new_path)
        {
            *last_slash = '\0';
        }
        else
        {
            strcpy(new_path, "/");
        }
    }
    else
    {
        if (arg[0] == '/')
        {
            strcpy(new_path, arg);
        }
        else
        {
            strcpy(new_path, api->current_path);
            if (!streq(new_path, "/"))
            {
                strcat(new_path, "/");
            }
            strcat(new_path, arg);
        }
    }

    if (api->clawfs_get_sector(new_path) != 0)
    {
        api->set_cwd(new_path);
    }
    else
    {
        api->print("Error: Directory not found: ");
        api->print(new_path);
        api->print("\n");
        return 1;
    }

    return 0;
}
