#ifndef RAM_DISK_HPP
#define RAM_DISK_HPP

#pragma once

#include <stdint.h>

#define RAM_DISK_SIZE_BYTES (64ULL * 1024ULL * 1024ULL)
#define RAM_DISK_SECTOR_SIZE 512ULL
#define RAM_DISK_PAGE_SIZE 4096ULL

#define RAM_DISK_SECTOR_COUNT (RAM_DISK_SIZE_BYTES / RAM_DISK_SECTOR_SIZE)
#define RAM_DISK_PAGE_COUNT (RAM_DISK_SIZE_BYTES / RAM_DISK_PAGE_SIZE)

class RAM_Disk
{
public:
    static bool init();
    static void shutdown();
    static bool is_enabled();

    static bool read_sector(uint32_t lba, uint8_t* buffer);
    static bool write_sector(uint32_t lba, uint8_t* buffer);

    static uint64_t size();
    static uint32_t sector_count();
};

#endif