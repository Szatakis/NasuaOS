#include <stdint.h>
#include <stdlib.h>

#include "w_file.h"

extern int doom_nasua_file_size(const char* path, uint32_t* size);
extern int doom_nasua_read_range(const char* path, uint32_t offset,
                                 void* buffer, uint32_t length);

typedef struct
{
    wad_file_t wad;
    char path[256];
} nasua_wad_file_t;

static wad_file_t* nasua_wad_open(char* path);
static void nasua_wad_close(wad_file_t* wad);
static size_t nasua_wad_read(wad_file_t* wad, unsigned int offset,
                             void* buffer, size_t buffer_len);

wad_file_class_t stdc_wad_file =
{
    nasua_wad_open,
    nasua_wad_close,
    nasua_wad_read
};

static wad_file_t* nasua_wad_open(char* path)
{
    uint32_t file_size = 0;
    if (path == NULL || !doom_nasua_file_size(path, &file_size) || file_size == 0)
    {
        return NULL;
    }

    nasua_wad_file_t* file = malloc(sizeof(*file));
    if (file == NULL)
    {
        return NULL;
    }

    file->wad.file_class = &stdc_wad_file;
    file->wad.mapped = NULL;
    file->wad.length = file_size;

    size_t path_length = 0;
    while (path[path_length] != '\0' && path_length < sizeof(file->path) - 1)
    {
        file->path[path_length] = path[path_length];
        ++path_length;
    }
    if (path[path_length] != '\0')
    {
        free(file);
        return NULL;
    }
    file->path[path_length] = '\0';
    return &file->wad;
}

static void nasua_wad_close(wad_file_t* wad)
{
    free(wad);
}

static size_t nasua_wad_read(wad_file_t* wad, unsigned int offset,
                             void* buffer, size_t buffer_len)
{
    nasua_wad_file_t* file = (nasua_wad_file_t*)wad;
    if (offset >= wad->length || buffer == NULL)
    {
        return 0;
    }

    size_t available = wad->length - offset;
    if (buffer_len > available)
    {
        buffer_len = available;
    }
    if (buffer_len > UINT32_MAX ||
        !doom_nasua_read_range(file->path, offset, buffer, (uint32_t)buffer_len))
    {
        return 0;
    }
    return buffer_len;
}
