#include <napp.h>

NAPP_APPLICATION("cd", "Change the current working directory");

static void strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++));
}

static void strcat(char* dest, const char* src)
{
    while (*dest) 
    {
        dest++;
    }

    while ((*dest++ = *src++));
}

static bool streq(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) 
    { 
        s1++; s2++; 
    }

    return *s1 == *s2;
}

static void normalize_path(const char* input, char* output)
{
    char temp[256];
    strcpy(temp, input);

    char* parts[64];
    int count = 0;

    char* p = temp;

    // Pomijamy początkowe /
    if (*p == '/')
    {
        p++;
    }

    while (*p)
    {
        char* start = p;

        while (*p && *p != '/')
        {
            p++;
        }

        if (*p)
        {
            *p = '\0';
            p++;
        }

        // .
        if (streq(start, ".") || start[0] == '\0')
        {
            continue;
        }

        // ..
        if (streq(start, ".."))
        {
            if (count > 0)
            {
                count--;
            }

            continue;
        }

        parts[count++] = start;
    }

    // Zbuduj wynik
    output[0] = '/';
    output[1] = '\0';

    for (int i = 0; i < count; i++)
    {
        strcat(output, parts[i]);

        if (i < count - 1)
        {
            strcat(output, "/");
        }
    }
}

int _start(const napp_api* api)
{
    const char* arg = (api->argc > 1) ? api->argv[1] : "";

    // Check for flag usage (incorrect syntax)
    if (api->argc > 1 && arg[0] == '-')
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: cd [directory_path]\n");
        return 1;
    }

    char new_path[256];
    char path_to_normalize[256];

    if (arg[0] == '\0' || streq(arg, "~"))
    {
        strcpy(new_path, "/home");
    }
    else
    {
        if (arg[0] == '/')
        {
            strcpy(path_to_normalize, arg);
        }
        else
        {
            strcpy(path_to_normalize, api->current_path);

            if (!streq(path_to_normalize, "/"))
            {
                strcat(path_to_normalize, "/");
            }

            strcat(path_to_normalize, arg);
        }

        normalize_path(path_to_normalize, new_path);
    }

    if (api->clawfs_get_sector(new_path) != 0)
    {
        api->set_cwd(new_path);
    }
    else
    {
        api->print_error("Directory not found: ");
        api->print(new_path);
        api->print("\n");
        return 1;
    }

    return 0;
}
