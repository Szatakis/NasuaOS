#pragma once

#include <stdint.h>

namespace BootSplash
{
    bool is_active();
    void init();
    void set_progress(uint32_t percent);
    void add_log(const char* subsystem, const char* message);
    void finish();
}
