#pragma once

#include <stdint.h>

namespace Iso9660
{
    bool init();
    bool file_size(const char* path, uint32_t* size);
    bool read_file_range(const char* path, uint32_t offset,
                         void* buffer, uint32_t length);
}
