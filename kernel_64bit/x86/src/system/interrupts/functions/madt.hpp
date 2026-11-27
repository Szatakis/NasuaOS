#pragma once
#include <stdint.h>

struct MadtInfo
{
    bool valid;                // MADT valid
    bool has_ioapic;           // MADT I/O APIC (typ 1)
    uint64_t ioapic_base;      // physic address I/O APIC
    uint32_t ioapic_gsi_base;  // GSI base I/O APIC
    uint32_t irq0_gsi;         // GSI
};

MadtInfo madt_parse();
