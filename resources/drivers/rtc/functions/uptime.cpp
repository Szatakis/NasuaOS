#include "../driver.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/timer/driver.hpp"

#include "libs/libc/libc.hpp"

namespace Rtc
{
    void print_uptime()
    {
        uint64_t ticks = Timer::pit_get_ticks();

        uint64_t uptime = ticks / 100;

        uint64_t hours = uptime / 3600;
        uint64_t minutes = (uptime % 3600) / 60;
        uint64_t seconds = uptime % 60;

        Gpu::print_info("System uptime: ");
        Gpu::print_int(hours);
        Gpu::print("h ");
        Gpu::print_int(minutes);
        Gpu::print("m ");
        Gpu::print_int(seconds);
        Gpu::print("s\n");
    }
}