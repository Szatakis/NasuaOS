#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ATA_disk
{
    bool identify(uint32_t* sectors);
    bool read_sector(uint32_t lba, uint8_t* buffer);
    bool write_sector(uint32_t lba, uint8_t* buffer);
}

namespace Disk
{
    bool read_sector(uint32_t lba, uint8_t* buffer);
    bool write_sector(uint32_t lba, uint8_t* buffer);

    void init();

    bool storage_is_ram();

    uint64_t total();
    uint64_t used();
}