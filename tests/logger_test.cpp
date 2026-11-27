#include "system/sysfunc/logger/logger.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/rtc/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "libs/libc/libc.hpp"

#include <cassert>

namespace Rtc
{
    RtcTime get_time()
    {
        return RtcTime{12, 34, 56, 7, 11, 2026};
    }
}

namespace Gpu
{
    void print(const char*) {}
    void print_num8(uint32_t) {}
}

void Uart::putc(char) {}
void Uart::puts(const char*) {}
void Uart::putdec(uint64_t) {}
void Uart::puthex(uint64_t) {}

int strcmp(const char* left, const char* right)
{
    while (*left && *right && *left == *right)
    {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

int main()
{
    logger_init();
    assert(get_log_count() == 0);

    log(INFO, "TEST", "first");
    log(WARN, "TEST", "second");
    log(ERROR, "OTHER", "third");

    assert(get_log_count() == 3);
    log(INFO, nullptr, nullptr);
    assert(get_log_count() == 4);

    clear_logs();
    assert(get_log_count() == 0);

    for (int i = 0; i < 140; ++i)
    {
        log(DEBUG, "WRAP", "entry");
    }
    assert(get_log_count() == 128);

    logger_init();
    assert(get_log_count() == 0);
}
