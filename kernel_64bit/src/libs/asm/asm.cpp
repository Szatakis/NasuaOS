#include "asm.hpp"

void disable_interrupts()
{
    asm volatile("cli");
}


void halt_cpu()
{
    asm volatile("hlt");
}


// Delay
void delay(uint64_t count)
{
    while (count--)
    {
        asm volatile("pause");
    }
}

void cpu_delay(uint64_t count)
{
    while (count--)
    {
        asm volatile("nop");
    }
}
