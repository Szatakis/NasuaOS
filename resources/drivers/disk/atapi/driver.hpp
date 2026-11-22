#pragma once

#include <stdint.h>

namespace ATAPI
{
    bool init();
    bool read_sectors(uint32_t lba, uint8_t count, void* buffer);
}
