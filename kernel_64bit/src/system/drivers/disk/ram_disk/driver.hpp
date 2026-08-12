#pragma once

#include <stdint.h>

#define RAM_DISK_SIZE_BYTES (64ULL * 1024ULL * 1024ULL)
#define RAM_DISK_SECTOR_SIZE 512ULL
#define RAM_DISK_PAGE_SIZE 4096ULL

#define RAM_DISK_SECTOR_COUNT (RAM_DISK_SIZE_BYTES / RAM_DISK_SECTOR_SIZE)

#define RAM_DISK_PAGE_COUNT (RAM_DISK_SIZE_BYTES / RAM_DISK_PAGE_SIZE)

bool ram_disk_init();

void ram_disk_shutdown();

bool ram_disk_is_enabled();

bool ram_disk_read_sector(uint32_t lba, uint8_t* buffer);
bool ram_disk_write_sector(uint32_t lba, uint8_t* buffer);

uint64_t ram_disk_size();

uint32_t ram_disk_sector_count();