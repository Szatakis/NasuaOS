#pragma once
#include <stdint.h>

#define CLAWFS_VERSION 1
#define CLAWFS_START_LBA 100
#define CLAWFS_ROOT_SECTOR 101

#define CLAWFS_FILE 0
#define CLAWFS_DIRECTORY 1

struct CLAWFSHeader 
{
    char signature[6];
    uint8_t version;
    uint16_t entryCount;
} __attribute__((packed));

struct CLAWFSEntry 
{
    char name[28];
    uint32_t type;
    uint32_t data_sector; // TO JEST TO NOWE POLE
    uint32_t entry_count; 
} __attribute__((packed));

void clawfs_format();
void clawfs_dir(const char* path);
bool clawfs_exists();
void clawfs_create_file(const char* name);
void clawfs_mkdir(const char* parent_path, const char* dir_name);
void clawfs_create_file_in(const char* path, const char* name);
uint32_t get_sector_by_path(const char* path);
bool clawfs_exists();
void clawfs_rm(const char* parent_path, const char* name, uint32_t type);