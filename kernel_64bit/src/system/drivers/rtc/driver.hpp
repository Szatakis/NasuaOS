#pragma once
#include <stdint.h>

struct RtcTime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

RtcTime get_rtc_time();
void set_rtc_time(RtcTime time);
bool is_rtc_battery_ok(void);
void print_uptime();