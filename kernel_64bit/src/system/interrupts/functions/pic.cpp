#include "pic.hpp"
#include "system/drivers/apic/driver.hpp"

#include "libs/asm/asm.h"

#include "system/drivers/uart/driver.hpp"
#include "kernel/include/logger/logger.hpp"


#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1


#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01

#define ICW4_8086  0x01



void pic_send_eoi(uint8_t irq)
{
    if(irq >= 8)
    {
        outb(PIC2_COMMAND, 0x20);
    }

    outb(PIC1_COMMAND, 0x20);
}

void irq_send_eoi(uint8_t irq)
{
    if(apic_is_active())
    {
        lapic_send_eoi();
        return;
    }

    pic_send_eoi(irq);
}

void pic_remap()
{
    Uart::puts("[PIC] Remapping...\n");
    log(INFO,"PIC","Remapping...");

    // ICW1 - start initialization

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);

    io_wait();

    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    io_wait();

    // ICW2 - vector offsets

    outb(PIC1_DATA, 0x20);

    io_wait();

    outb(PIC2_DATA, 0x28);

    io_wait();

    // ICW3 - cascade wiring

    outb(PIC1_DATA, 0x04);

    io_wait();

    outb(PIC2_DATA, 0x02);

    io_wait();

    // ICW4 - 8086 mode

    outb(PIC1_DATA, ICW4_8086);

    io_wait();

    outb(PIC2_DATA, ICW4_8086);

    io_wait();

    /*
        Mask:
        
        IRQ0  = ON
        reszta = OFF
    */

    outb(PIC1_DATA, 0xFE);

    io_wait();

    outb(PIC2_DATA, 0xFF);

    io_wait();

    Uart::puts("[PIC] Master mask: ");
    Uart::puthex(inb(PIC1_DATA));
    Uart::puts("\n");
    Uart::puts("[PIC] Slave mask: ");
    Uart::puthex(inb(PIC2_DATA));
    Uart::puts("\n");
    Uart::puts("[PIC] Ready\n");
    log(INFO,"PIC","Ready");
}

void pic_disable()
{
    outb(PIC1_DATA, 0xFF);

    outb(PIC2_DATA, 0xFF);
}
