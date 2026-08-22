#include <napp.h>

NAPP_APPLICATION("ls", "List files and directories in a path");

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
        s1++;
        s2++;
    }

    return *s1 == *s2;
}

static void normalize_path(const char* input, char* output)
{
    char temp[256];
    char* parts[64];

    strcpy(temp, input);

    int count = 0;
    char* p = temp;

    // Skip the leading slash
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

        if (*p == '/')
        {
            *p = '\0';
            p++;
        }

        // Ignore "."
        if (streq(start, ".") || start[0] == '\0')
        {
            continue;
        }

        // Go one directory up
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
    const char* path = api->current_path;
    char combined_path[256];
    char normalized_path[256];

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

    // Absolute path
    if (path[0] == '/')
    {
        normalize_path(path, normalized_path);
    }
    // Relative path
    else
    {
        strcpy(combined_path, api->current_path);

        if (!streq(combined_path, "/"))
        {
            strcat(combined_path, "/");
        }

        strcat(combined_path, path);

        normalize_path(combined_path, normalized_path);
    }

    api->clawfs_dir(normalized_path);

    return 0;
}