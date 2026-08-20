#include "isr.hpp"

#include "pic.hpp"
#include "page_fault.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/drivers/timer/driver.hpp"

#include "kernel/kernel_panic/kernel_panic.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

// Exception name table indexed by vector number
static const char* exception_names[] = {
    "Divide Error (#DE)",                // 0
    "Debug (#DB)",                       // 1
    "NMI Interrupt",                     // 2
    "Breakpoint (#BP)",                  // 3
    "Overflow (#OF)",                    // 4
    "Bound Range Exceeded (#BR)",        // 5
    "Invalid Opcode (#UD)",              // 6
    "Device Not Available (#NM)",        // 7
    "Double Fault (#DF)",                // 8
    "Coprocessor Segment Overrun",       // 9
    "Invalid TSS (#TS)",                 // 10
    "Segment Not Present (#NP)",         // 11
    "Stack-Segment Fault (#SS)",         // 12
    "General Protection Fault (#GP)",    // 13
    "Page Fault (#PF)",                  // 14
    "Reserved",                          // 15
    "x87 FPU Error (#MF)",               // 16
    "Alignment Check (#AC)",             // 17
    "Machine Check (#MC)",               // 18
    "SIMD Floating-Point (#XM)",         // 19
    "Virtualization Exception (#VE)",    // 20
    "Control Protection (#CP)",          // 21
};

extern "C"
void isr_handler(Registers* regs)
{
    // Spurious LAPIC interrupt — ignore
    if (regs->vector == 0xFF)
    {
        return;
    }

    // PIT timer tick
    if (regs->vector == 32)
    {
        Timer::pit_handler();
        return;
    }

    // Page fault — attempt demand paging; fatal cases handled inside
    if (regs->vector == 14)
    {
        page_fault_handler(regs);
        return;
    }

    // All other CPU exceptions — trigger kernel panic
    if (regs->vector < 22)
    {
        char rip_buf[20];
        char rsp_buf[20];
        char err_buf[20];

        to_hex64(regs->rip,   rip_buf);
        to_hex64(regs->rsp,   rsp_buf);
        to_hex64(regs->error, err_buf);

        const char* name = (regs->vector < 22) ? exception_names[regs->vector] : "Unknown Exception";

        Uart::puts("\n[ISR] CPU Exception vector=");
        Uart::puthex(regs->vector);
        Uart::puts(" rip=");
        Uart::puts(rip_buf);
        Uart::puts(" error=");
        Uart::puts(err_buf);
        Uart::puts("\n");

        kernel_panic(name, err_buf, rip_buf, rsp_buf);
        return;
    }

    // Unknown / hardware IRQ with no handler
    Uart::puts("\nUNHANDLED INTERRUPT\n");
    Uart::puts("Vector: ");
    Uart::puthex(regs->vector);
    Uart::puts("\n");
    Uart::puts("Error:  ");
    Uart::puthex(regs->error);
    Uart::puts("\n");

    while (true)
    {
        asm volatile("cli\nhlt");
    }
}
