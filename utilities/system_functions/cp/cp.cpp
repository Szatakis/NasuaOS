#include <napp.h>

NAPP_APPLICATION("cp");

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

int _start(const napp_api* api)
{
    if (api->argc < 3)
    {
        api->print("Usage: cp <source_file> <destination>\n");
        return 1;
    }

    const char* src_path = api->argv[1];
    const char* dest_path = api->argv[2];

    char src_parent[128], src_name[128];
    split_path(src_path, src_parent, src_name);

    char src_parent_abs[256];
    if (strcmp(src_parent, ".")) {
        strcpy(src_parent_abs, api->current_path);
    } else {
        if (src_parent[0] == '/') {
            strcpy(src_parent_abs, src_parent);
        } else {
            strcpy(src_parent_abs, api->current_path);
            if (strcmp(src_parent_abs, "/") != 0) strcat(src_parent_abs, "/");
            strcat(src_parent_abs, src_parent);
        }
    }

    int src_type = api->clawfs_get_entry_type(src_parent_abs, src_name);
    if (src_type != 0)
    {
        api->print("Source is not a file or does not exist!\n");
        return 1;
    }

    uint32_t src_sector = api->clawfs_resolve_path(api->current_path, src_path);
    if (src_sector == 0)
    {
        api->print("Failed to resolve source file!\n");
        return 1;
    }

    char file_buffer[512];
    if (!api->clawfs_read_sector(src_sector, file_buffer))
    {
        api->print("Failed to read source file!\n");
        return 1;
    }

    char dest_parent[128], dest_name[128];
    split_path(dest_path, dest_parent, dest_name);

    char dest_parent_abs[256];
    if (strcmp(dest_parent, ".")) {
        strcpy(dest_parent_abs, api->current_path);
    } else {
        if (dest_parent[0] == '/') {
            strcpy(dest_parent_abs, dest_parent);
        } else {
            strcpy(dest_parent_abs, api->current_path);
            if (strcmp(dest_parent_abs, "/") != 0) strcat(dest_parent_abs, "/");
            strcat(dest_parent_abs, dest_parent);
        }
    }

    uint32_t dest_sector = api->clawfs_resolve_path(api->current_path, dest_path);
    if (dest_sector != 0)
    {
        int dest_type = api->clawfs_get_entry_type(dest_parent_abs, dest_name);
        if (dest_type == 1)
        {
            strcpy(dest_parent_abs, dest_path);
            strcpy(dest_name, src_name);
        }
    }

    api->clawfs_create_file_in(dest_parent_abs, dest_name);

    uint32_t new_file_sector = api->clawfs_resolve_path(dest_parent_abs, dest_name);
    if (new_file_sector == 0)
    {
        api->print("Failed to create destination file!\n");
        return 1;
    }

    if (!api->clawfs_write_sector(new_file_sector, file_buffer))
    {
        api->print("Failed to write destination file!\n");
        return 1;
    }

    return 0;
}
