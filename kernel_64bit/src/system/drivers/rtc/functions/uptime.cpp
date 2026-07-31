#include "../driver.h"

#include "libs/libc/libc.h"

#include "system/drivers/video/driver.hpp"

#include "system/drivers/timer/driver.h"


void print_uptime()
{
    uint64_t ticks = pit_get_ticks();

    uint64_t uptime = ticks / 100;

    uint64_t hours = uptime / 3600;
    uint64_t minutes = (uptime % 3600) / 60;
    uint64_t seconds = uptime % 60;


    print_info("System uptime: ");
    print_int(hours);
    print("h ");
    print_int(minutes);
    print("m ");
    print_int(seconds);
    print("s\n");
}