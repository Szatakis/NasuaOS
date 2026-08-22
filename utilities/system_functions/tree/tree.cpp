#include <napp.h>

NAPP_APPLICATION("tree", "Display directory tree structure");

#define CLAWFS_FILE 0
#define CLAWFS_DIRECTORY 1

#define CLAWFS_MAX_ENTRIES 12

struct CLAWFSEntry
{
    char name[28];
    uint32_t type;
    uint32_t data_sector;
    uint32_t entry_count;
} __attribute__((packed));

static void strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++));
}

static void strcat(char* dest, const char* src)
{
    while (*dest) dest++;
    while ((*dest++ = *src++));
}

static bool streq(const char* s1, const char* s2)
{
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 == *s2;
}

static int strlen(const char* s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
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

static void split_path(const char* path, char* parent, char* filename)
{
    const char* last_slash = strrchr(path, '/');
    if (last_slash == nullptr)
    {
        strcpy(parent, ".");
        strcpy(filename, path);
    }
    else if (last_slash == path)
    {
        strcpy(parent, "/");
        strcpy(filename, path + 1);
    }
    else
    {
        int len = last_slash - path;
        for (int i = 0; i < len; i++) parent[i] = path[i];
        parent[len] = '\0';
        strcpy(filename, last_slash + 1);
    }
}

static void normalize_path(const char* input, char* output)
{
    char temp[256];
    strcpy(temp, input);

    char* parts[32];
    int count = 0;

    char* p = temp;

    // Skip leading slash
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

        // Skip "."
        if (streq(start, ".") || start[0] == '\0')
        {
            continue;
        }

        // Handle ".."
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

    // Build result (always absolute)
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

static void resolve_path(const char* path, const char* current_path, char* out)
{
    if (path[0] == '/')
    {
        strcpy(out, path);
    }
    else if (streq(path, "."))
    {
        strcpy(out, current_path);
    }
    else
    {
        strcpy(out, current_path);
        int len = strlen(out);
        if (len > 0 && out[len - 1] != '/')
        {
            strcat(out, "/");
        }
        strcat(out, path);
    }
}

static void build_child_path(const char* parent, const char* name, char* out)
{
    strcpy(out, parent);
    int len = strlen(out);
    if (len > 0 && out[len - 1] != '/')
    {
        strcat(out, "/");
    }
    strcat(out, name);
}

static bool is_hidden(const char* name)
{
    return name[0] == '.';
}

static void print_tree(const char* path, const char* prefix, const napp_api* api)
{
    uint32_t sector = api->clawfs_get_sector(path);
    if (sector == 0)
    {
        return;
    }

    uint8_t sector_buf[512];
    if (!api->clawfs_read_sector(sector, sector_buf))
    {
        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)sector_buf;

    // Count visible entries (non-empty name, not hidden)
    int total = 0;
    for (int i = 0; i < CLAWFS_MAX_ENTRIES; i++)
    {
        if (entries[i].name[0] != '\0' && !is_hidden(entries[i].name))
        {
            total++;
        }
    }

    int index = 0;
    for (int i = 0; i < CLAWFS_MAX_ENTRIES; i++)
    {
        if (entries[i].name[0] == '\0' || is_hidden(entries[i].name))
        {
            continue;
        }

        bool is_last = (index == total - 1);

        api->print(prefix);

        if (is_last)
        {
            api->print("`-- ");
        }
        else
        {
            api->print("|-- ");
        }

        if (entries[i].type == CLAWFS_DIRECTORY)
        {
            api->print("<DIR>  ");
        }
        else
        {
            api->print("<FILE> ");
        }

        api->print(entries[i].name);
        api->print("\n");

        // Only recurse into directories
        if (entries[i].type == CLAWFS_DIRECTORY)
        {
            char child_path[256];
            build_child_path(path, entries[i].name, child_path);

            char new_prefix[256];
            strcpy(new_prefix, prefix);

            if (is_last)
            {
                strcat(new_prefix, "    ");
            }
            else
            {
                strcat(new_prefix, "|   ");
            }

            print_tree(child_path, new_prefix, api);
        }

        index++;
    }
}

int _start(const napp_api* api)
{
    const char* path_arg = nullptr;

    if (api->argc == 1)
    {
        path_arg = api->current_path;
    }
    else if (api->argc == 3)
    {
        if (api->argv[1][0] != '-' ||
            api->argv[1][1] != 'f' ||
            api->argv[1][2] != 'i' ||
            api->argv[1][3] != 'l' ||
            api->argv[1][4] != 'e' ||
            api->argv[1][5] != '\0')
        {
            api->print_error("Syntax error!\n");
            api->print("Usage: tree -file <directory_path>\n");
            return 1;
        }

        path_arg = api->argv[2];
    }
    else
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: tree -file <directory_path>\n");
        return 1;
    }

    char resolved[256];
    resolve_path(path_arg, api->current_path, resolved);

    char abs_path[256];
    normalize_path(resolved, abs_path);

    uint32_t sector = api->clawfs_get_sector(abs_path);

    if (sector == 0)
    {
        api->print_error("Directory not found: ");
        api->print(abs_path);
        api->print("\n");
        return 1;
    }

    int entry_type = -1;

    if (streq(abs_path, "/"))
    {
        entry_type = CLAWFS_DIRECTORY;
    }
    else
    {
        char parent[256];
        char name[64];

        split_path(abs_path, parent, name);

        entry_type = api->clawfs_get_entry_type(parent, name);
    }

    api->print(abs_path);
    api->print("\n");

    if (entry_type == CLAWFS_DIRECTORY)
    {
        print_tree(abs_path, "", api);
    }

    return 0;
}
