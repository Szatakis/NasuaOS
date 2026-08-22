#include "../driver.hpp"

#include "system/interrupts/functions/pic.hpp"
#include "drivers/uart/driver.hpp"
#include "drivers/gpu/driver.hpp"

#include "system/sysfunc/logger/logger.hpp"

#include "libs/asm/asm.hpp"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

#define PIT_BASE_FREQ 1193182
#define PIT_FREQUENCY 100

namespace Timer
{
    static volatile uint64_t ticks = 0;

    volatile bool redraw = true;

    void pit_init()
    {
        Uart::puts("[PIT] Initializing...\n");
        log(INFO, "PIT", "Initializing...");

        uint16_t divisor = PIT_BASE_FREQ / PIT_FREQUENCY;

        outb(PIT_COMMAND, 0x36);
        io_wait();

        outb(PIT_CHANNEL0, divisor & 0xFF);
        io_wait();

        outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
        io_wait();

        Uart::puts("[PIT] Ready\n");
        log(INFO, "PIT", "Ready");
    }

    void pit_handler()
    {
        ticks = ticks + 1;

        redraw = true;

        irq_send_eoi(0);
    }

    uint64_t pit_get_ticks()
    {
        return ticks;
    }

    void sleep(uint64_t ms)
    {
        uint64_t wait_ticks = ms / 10;

        if (wait_ticks == 0)
        {
            wait_ticks = 1;
        }

        uint64_t start = ticks;

        while ((ticks - start) < wait_ticks)
        {
            asm volatile("hlt");
        }
    }
}