#pragma once
#include <stdint.h>
#include <stddef.h>

// Declaration of functions to read and write to disk sectors
// 'lba' parameter (Logical Block Addressing) is sector number on disk (from 0)
// 'buffer' parametr is pointer to 512 bajts table

void disk_read_sector(uint32_t lba, uint8_t* buffer);
void disk_write_sector(uint32_t lba, uint8_t* buffer);

// Inits disk uses system
void storage_init();

// Returns disk size in bajts
uint64_t storage_total();

// Returns used disk space in bajts
uint64_t storage_used();