#pragma once
#include <stdint.h>

//CPUID

// true if CPU has Local APIC (CPUID.1:EDX.APIC[9])
bool apic_available();

//Local APIC

uint64_t apic_read_base();
bool apic_enabled();

void apic_enable();   // ustawia bit Global Enable w IA32_APIC_BASE
void apic_disable();  // czysci bit Global Enable w IA32_APIC_BASE

void lapic_init();       // wlacza LAPIC (Spurious Vector Register) i zeruje TPR
void lapic_send_eoi();   // wysyla EOI do Local APIC

//I/O APIC 

void ioapic_init();
void ioapic_mask_irq(uint8_t irq);
void ioapic_set_irq(uint8_t irq, uint8_t vector, uint32_t dest_apic_id);

//Choose interrupts controller

// true if kernel activly use APIC (LAPIC+IOAPIC) instead of 8259 PIC controller
bool apic_is_active();

// Detects anvibility of APIC. If anvible use LAPIC/IOAPIC.
// If not anvible use 8259 PIC.
void interrupts_controller_init();
