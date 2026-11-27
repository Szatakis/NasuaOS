#pragma once

#include <stdint.h>

namespace Rtc
{
    struct RtcTime
    {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t month;
        uint32_t year;
    };

    RtcTime get_time();

    void set_time(RtcTime time);

    bool is_battery_ok();

    void print_uptime();
}