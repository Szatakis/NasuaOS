#include <napp.h>

NAPP_APPLICATION("mv", "Move or rename a file or directory");

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

static void resolve_path(const char* path, const char* current_path, char* resolved)
{
    if (path[0] == '/')
    {
        strcpy(resolved, path);
    }
    else if (path[0] == '.' && path[1] == '\0')
    {
        strcpy(resolved, current_path);
    }
    else
    {
        strcpy(resolved, current_path);
        if (!strcmp(resolved, "/"))
        {
            strcat(resolved, "/");
        }
        strcat(resolved, path);
    }
}

int _start(const napp_api* api)
{
    const char* src_path = nullptr;
    const char* dest_path = nullptr;

    // Only accept flag format
    for (int i = 1; i < api->argc; i++)
    {
        if (is_flag(api->argv[i], "--source") && i + 1 < api->argc)
        {
            src_path = api->argv[i + 1];
            i++;
        }
        else if (is_flag(api->argv[i], "--destination") && i + 1 < api->argc)
        {
            dest_path = api->argv[i + 1];
            i++;
        }
    }

    if (src_path == nullptr || dest_path == nullptr)
    {
        api->print_error("Syntax error!\n");
        api->print("Usage: mv --source <source> --destination <destination>\n");
        return 1;
    }

    uint32_t src_sector = api->clawfs_resolve_path(api->current_path, src_path);
    if (src_sector == 0)
    {
        api->print_error("Source file not found!\n");
        return 1;
    }

    char file_buffer[512];
    if (!api->clawfs_read_sector(src_sector, file_buffer))
    {
        api->print_error("Failed to read source file!\n");
        return 1;
    }

    char dest_parent[128], dest_name[128];
    split_path(dest_path, dest_parent, dest_name);

    char dest_parent_abs[256];
    resolve_path(dest_parent, api->current_path, dest_parent_abs);

    uint32_t dest_sector = api->clawfs_resolve_path(api->current_path, dest_path);
    if (dest_sector != 0)
    {
        int dest_type = api->clawfs_get_entry_type(dest_parent_abs, dest_name);
        if (dest_type == 1)
        {
            // Destination is a directory, move into it with source name
            char src_parent[128], src_name[128];
            split_path(src_path, src_parent, src_name);
            resolve_path(dest_path, api->current_path, dest_parent_abs);
            strcpy(dest_name, src_name);
        }
    }

    api->clawfs_create_file_in(dest_parent_abs, dest_name);

    uint32_t new_file_sector = api->clawfs_resolve_path(dest_parent_abs, dest_name);
    if (new_file_sector == 0)
    {
        api->print_error("Failed to create destination file!\n");
        return 1;
    }

    if (!api->clawfs_write_sector(new_file_sector, file_buffer))
    {
        api->print_error("Failed to write destination file!\n");
        return 1;
    }

    // Delete source file
    char src_parent[128], src_name[128];
    split_path(src_path, src_parent, src_name);
    char src_parent_abs[256];
    resolve_path(src_parent, api->current_path, src_parent_abs);
    api->clawfs_rm(src_parent_abs, src_name, 0);

    return 0;
}
