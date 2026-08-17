#pragma once

#include <stdint.h>
#include <stddef.h>

#define FAT_MAX_NAME 128

enum fat_type
{
    FAT_TYPE_NONE = 0,
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32
};

struct fat_volume
{
    const uint8_t* image;
    uint64_t image_size;

    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_entry_count;
    uint32_t total_sectors;

    uint32_t fat_sector;
    uint32_t root_sector;
    uint32_t root_cluster;
    uint32_t root_sectors;
    uint32_t data_sector;
    uint32_t cluster_count;

    fat_type type;
    bool mounted;
};

struct fat_entry_info
{
    char name[FAT_MAX_NAME];

    uint32_t size;
    uint32_t first_cluster;

    bool directory;
};

// Mounts a FAT12/FAT16/FAT32 image that already lives in memory.
bool fat_mount(fat_volume* volume, const void* image, uint64_t image_size);

bool fat_stat(fat_volume* volume, const char* path, fat_entry_info* info);
bool fat_read_file(fat_volume* volume, const char* path, void* buffer, uint32_t buffer_size, uint32_t* read_size);

// Fills entries with the content of the directory, returns the number of entries.
uint32_t fat_list_directory(fat_volume* volume, const char* path, fat_entry_info* entries, uint32_t max_entries);
