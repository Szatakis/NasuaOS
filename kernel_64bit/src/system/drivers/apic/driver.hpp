#ifndef APIC_H
#define APIC_H

#include <stdint.h>
#include <stdbool.h>

class Apic
{
public:
    // CPUID
    static bool available();

    // Local APIC: Base MSR
    static uint64_t read_base();
    static bool enabled();
    static void enable();   // ustawia bit Global Enable w IA32_APIC_BASE
    static void disable();  // czysci bit Global Enable w IA32_APIC_BASE

    // Local APIC: init / EOI / ID
    static void init();       // wlacza LAPIC (Spurious Vector Register) i zeruje TPR
    static void send_eoi();   // wysyla EOI do Local APIC
    static uint32_t get_id();

    // Controller state & choice
    static bool is_active();
    static void controller_init();
};

class IOApic
{
public:
    static void init();
    static void mask_irq(uint8_t irq);
    static void set_irq(uint8_t irq, uint8_t vector, uint32_t dest_apic_id);
};

#endif