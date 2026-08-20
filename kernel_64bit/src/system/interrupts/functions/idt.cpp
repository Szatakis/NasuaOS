#include "idt.hpp"

#include "isr.hpp"
#include "system/drivers/uart/driver.hpp"

#include "system/sysfunc/logger/logger.hpp"

extern "C" void irq0();

struct IDTEntry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IDTR
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static IDTEntry idt[256];
static IDTR idtr;

static void set_gate(uint8_t vector, void* handler)
{
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low = addr & 0xFFFF;
    idt[vector].selector = 0x28;
    idt[vector].ist = 0;
    idt[vector].type_attr = 0x8E;
    idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

void idt_init()
{
    Uart::puts("[IDT] Initializing...\n");
    log(INFO,"IDT","Initializing...");

    for(int i = 0; i < 256; i++)
    {
        set_gate(i, (void*)isr_default);
    }

    // CPU Exceptions

    set_gate(0,  (void*)isr_divide_error);          // #DE Divide Error
    set_gate(4,  (void*)isr_overflow);              // #OF Overflow
    set_gate(5,  (void*)isr_bound_range);           // #BR Bound Range Exceeded
    set_gate(6,  (void*)isr_invalid_opcode);        // #UD Invalid Opcode
    set_gate(8,  (void*)isr_double_fault);          // #DF Double Fault
    set_gate(10, (void*)isr_invalid_tss);           // #TS Invalid TSS
    set_gate(11, (void*)isr_segment_not_present);   // #NP Segment Not Present
    set_gate(12, (void*)isr_stack_fault);           // #SS Stack-Segment Fault
    set_gate(13, (void*)isr_gpf);                   // #GP General Protection Fault
    set_gate(14, (void*)isr_page_fault);            // #PF Page Fault
    set_gate(17, (void*)isr_alignment_check);       // #AC Alignment Check

    // IRQ0 - PIT Timer

    extern void irq0();

    set_gate(32, (void*)irq0);

    Uart::puts("[IDT] IRQ0 installed\n");
    log(INFO,"IDT","IRQ0 installed");

    // LAPIC Spurious Interrupt (vector 0xFF)

    extern void isr_spurious();

    set_gate(0xFF, (void*)isr_spurious);

    Uart::puts("[IDT] Spurious vector (0xFF) installed\n");
    log(INFO,"IDT","Spurious vector (0xFF) installed");

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    Uart::puts("[IDT] Loading...\n");
    log(INFO,"IDT","Loading...");

    uint16_t cs;

    asm volatile("mov %%cs, %0" : "=r"(cs));

    Uart::puts("[IDT] CS: ");
    Uart::puthex(cs);
    Uart::puts("\n");

    asm volatile("lidt %0" : : "m"(idtr));

    Uart::puts("[IDT] Loaded\n");
    log(INFO,"IDT","Loaded");
}