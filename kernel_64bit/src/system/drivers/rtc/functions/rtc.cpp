#include "../driver.hpp"

#include "libs/asm/asm.hpp"

#define RTC_CMOS_ADDRESS  0x70
#define RTC_CMOS_DATA     0x71

#define RTC_SEC           0x00
#define RTC_MIN           0x02
#define RTC_HOUR          0x04
#define RTC_DAY           0x07
#define RTC_MONTH         0x08
#define RTC_YEAR          0x09
#define RTC_CENTURY       0x32
#define RTC_WEEKDAY       0x06

#define RTC_REGISTER_A    0x0A
#define RTC_REGISTER_B    0x0B
#define RTC_REGISTER_D    0x0D

namespace Rtc
{

static uint8_t bcd_to_binary(uint8_t bcd)
{
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static uint8_t binary_to_bcd(uint8_t binary)
{
    return ((binary / 10) << 4) + (binary % 10);
}

static bool rtc_update_in_progress()
{
    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_A);
    return (inb(RTC_CMOS_DATA) & 0x80) != 0;
}

static uint8_t read_rtc_register(int reg)
{
    while (rtc_update_in_progress());

    outb(RTC_CMOS_ADDRESS, reg);
    return inb(RTC_CMOS_DATA);
}

RtcTime get_time()
{
    RtcTime time;
    uint8_t regB;

    time.second = read_rtc_register(RTC_SEC);
    time.minute = read_rtc_register(RTC_MIN);
    time.hour   = read_rtc_register(RTC_HOUR);
    time.day    = read_rtc_register(RTC_DAY);
    time.month  = read_rtc_register(RTC_MONTH);
    time.year   = read_rtc_register(RTC_YEAR);

    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_B);
    regB = inb(RTC_CMOS_DATA);

    if (!(regB & 0x04))
    {
        time.second = bcd_to_binary(time.second);
        time.minute = bcd_to_binary(time.minute);
        time.hour   = bcd_to_binary(time.hour);
        time.day    = bcd_to_binary(time.day);
        time.month  = bcd_to_binary(time.month);
        time.year   = bcd_to_binary(time.year);
    }

    if (!(regB & 0x02) && (time.hour & 0x80))
    {
        time.hour = ((time.hour & 0x7F) + 12) % 24;
    }

    time.year += 2000;

    return time;
}

void set_time(RtcTime time)
{
    uint8_t regB;

    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_B);
    regB = inb(RTC_CMOS_DATA);

    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_B);
    outb(RTC_CMOS_DATA, regB | 0x80);

    if (!(regB & 0x04))
    {
        time.second = binary_to_bcd(time.second);
        time.minute = binary_to_bcd(time.minute);
        time.hour   = binary_to_bcd(time.hour);
        time.day    = binary_to_bcd(time.day);
        time.month  = binary_to_bcd(time.month);
        time.year   = binary_to_bcd(time.year % 100);
    }

    outb(RTC_CMOS_ADDRESS, RTC_SEC);
    outb(RTC_CMOS_DATA, time.second);

    outb(RTC_CMOS_ADDRESS, RTC_MIN);
    outb(RTC_CMOS_DATA, time.minute);

    outb(RTC_CMOS_ADDRESS, RTC_HOUR);
    outb(RTC_CMOS_DATA, time.hour);

    outb(RTC_CMOS_ADDRESS, RTC_DAY);
    outb(RTC_CMOS_DATA, time.day);

    outb(RTC_CMOS_ADDRESS, RTC_MONTH);
    outb(RTC_CMOS_DATA, time.month);

    outb(RTC_CMOS_ADDRESS, RTC_YEAR);
    outb(RTC_CMOS_DATA, time.year);

    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_B);
    outb(RTC_CMOS_DATA, regB & ~0x80);
}

bool is_battery_ok()
{
    outb(RTC_CMOS_ADDRESS, RTC_REGISTER_D);

    uint8_t regD = inb(RTC_CMOS_DATA);

    return (regD & 0x80) != 0;
}

}