#include "isr.hpp"

#include "pic.hpp"
#include "page_fault.hpp"

#include "system/drivers/uart/driver.h"
#include "system/drivers/timer/driver.h"

#include "libs/asm/asm.h"

extern "C"
void isr_handler(Registers* regs)
{
    if(regs->vector == 0xFF)
    {
        return;
    }

    if(regs->vector == 32)
    {
        pit_handler();
        return;
    }

    if(regs->vector == 14)
    {
        page_fault_handler(regs->error);
        return;
    }

    Uart::puts("\n========== EXCEPTION ==========\n");
    Uart::puts("Vector: ");
    Uart::puthex(regs->vector);
    Uart::puts("\n");
    Uart::puts("Error: ");
    Uart::puthex(regs->error);
    Uart::puts("\n");

    while(true)
    {
        asm volatile("cli\nhlt");
    }
}
