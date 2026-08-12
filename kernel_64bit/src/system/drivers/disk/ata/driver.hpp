#pragma once
#include <stdint.h>
#include <stddef.h>

// Declaration of functions to read and write to disk sectors
// 'lba' parameter (Logical Block Addressing) is sector number on disk (from 0)
// 'buffer' parametr is pointer to 512 bajts table

void disk_read_sector(uint32_t lba, uint8_t* buffer);
void disk_write_sector(uint32_t lba, uint8_t* buffer);

bool ata_identify(uint32_t* sectors);

void storage_init();

bool storage_is_ram();

bool storage_read_sector(
    uint32_t lba,
    uint8_t* buffer
);

bool storage_write_sector(
    uint32_t lba,
    uint8_t* buffer
);

uint64_t storage_total();
uint64_t storage_used();