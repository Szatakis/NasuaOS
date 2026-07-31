#include "madt.hpp"
#include <limine.h>

#include "system/drivers/uart/driver.hpp"
#include "kernel/include/logger/logger.hpp"

extern volatile limine_rsdp_request rsdp_request;
extern volatile limine_hhdm_request hhdm_request;


static inline uint64_t hhdm_offset()
{
    if(!hhdm_request.response)
    {
        return 0;
    }

    return hhdm_request.response->offset;
}

static bool sig_matches(const char* a, const char* b, int len)
{
    for(int i = 0; i < len; i++)
    {
        if(a[i] != b[i])
        {
            return false;
        }
    }

    return true;
}


struct ACPISDTHeader
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct RSDPDescriptor
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct RSDPDescriptor20
{
    RSDPDescriptor first;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct MADTHeader
{
    ACPISDTHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

struct MADTEntryHeader
{
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct MADTIOAPIC
{
    MADTEntryHeader header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} __attribute__((packed));

struct MADTInterruptSourceOverride
{
    MADTEntryHeader header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));


static ACPISDTHeader* find_table_by_signature(const char* wanted, RSDPDescriptor* rsdp)
{
    // ACPI 2.0+
    if(rsdp->revision >= 2)
    {
        RSDPDescriptor20* rsdp2 = (RSDPDescriptor20*)rsdp;
        uint64_t xsdt_phys = rsdp2->xsdt_address;

        if(!xsdt_phys)
        {
            return nullptr;
        }

        ACPISDTHeader* xsdt = (ACPISDTHeader*)(uintptr_t)(hhdm_offset() + xsdt_phys);

        uint32_t entry_count = (xsdt->length - sizeof(ACPISDTHeader)) / 8;
        uint64_t* entries = (uint64_t*)((uint8_t*)xsdt + sizeof(ACPISDTHeader));

        for(uint32_t i = 0; i < entry_count; i++)
        {
            ACPISDTHeader* table = (ACPISDTHeader*)(uintptr_t)(hhdm_offset() + entries[i]);

            if(sig_matches(table->signature, wanted, 4))
            {
                return table;
            }
        }
    }
    else
    {
        uint32_t rsdt_phys = rsdp->rsdt_address;

        if(!rsdt_phys)
        {
            return nullptr;
        }

        ACPISDTHeader* rsdt = (ACPISDTHeader*)(uintptr_t)(hhdm_offset() + rsdt_phys);

        uint32_t entry_count = (rsdt->length - sizeof(ACPISDTHeader)) / 4;
        uint32_t* entries = (uint32_t*)((uint8_t*)rsdt + sizeof(ACPISDTHeader));

        for(uint32_t i = 0; i < entry_count; i++)
        {
            ACPISDTHeader* table = (ACPISDTHeader*)(uintptr_t)(hhdm_offset() + entries[i]);

            if(sig_matches(table->signature, wanted, 4))
            {
                return table;
            }
        }
    }

    return nullptr;
}

MadtInfo madt_parse()
{
    MadtInfo info{};
    info.valid = false;
    info.has_ioapic = false;
    info.ioapic_base = 0;
    info.ioapic_gsi_base = 0;
    info.irq0_gsi = 0;

    if(!rsdp_request.response || !rsdp_request.response->address)
    {
        Uart::puts("[MADT] No RSDP\n");
        log(INFO,"MADT","No RSDP");
        return info;
    }

    RSDPDescriptor* rsdp = (RSDPDescriptor*)rsdp_request.response->address;

    if(!sig_matches(rsdp->signature, "RSD PTR ", 8))
    {
        Uart::puts("[MADT] Bad RSDP signature\n");
        log(INFO,"MADT","Bad RSDP signature");
        return info;
    }

    ACPISDTHeader* madt_hdr = find_table_by_signature("APIC", rsdp);

    if(!madt_hdr)
    {
        Uart::puts("[MADT] MADT table not found\n");
        log(INFO,"MADT","MADT table not found");
        return info;
    }

    MADTHeader* madt = (MADTHeader*)madt_hdr;

    info.valid = true;

    uint8_t* p = (uint8_t*)madt + sizeof(MADTHeader);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    bool override_found = false;

    while(p < end)
    {
        MADTEntryHeader* entry = (MADTEntryHeader*)p;

        if(entry->length == 0)
        {
            break;
        }

        if(entry->type == 1)
        {
            // I/O APIC
            MADTIOAPIC* ioapic = (MADTIOAPIC*)p;

            if(!info.has_ioapic)
            {
                info.has_ioapic = true;
                info.ioapic_base = ioapic->ioapic_address;
                info.ioapic_gsi_base = ioapic->gsi_base;
            }
        }
        else if(entry->type == 2)
        {
            // Interrupt Source Override
            MADTInterruptSourceOverride* iso = (MADTInterruptSourceOverride*)p;

            if(iso->bus_source == 0 && iso->irq_source == 0)
            {
                info.irq0_gsi = iso->gsi;
                override_found = true;
            }
        }

        p += entry->length;
    }

    if(!override_found)
    {
        info.irq0_gsi = 0;
    }

    Uart::puts("[MADT] IOAPIC present: ");
    Uart::puts(info.has_ioapic ? "yes" : "no");
    Uart::puts("\n");
    log(INFO,"MADT", info.has_ioapic ? "IOAPIC present: yes" : "IOAPIC present: no");

    if(info.has_ioapic)
    {
        Uart::puts("[MADT] IOAPIC base: ");
        Uart::puthex(info.ioapic_base);
        Uart::puts("\n");
        Uart::puts("[MADT] IRQ0 -> GSI: ");
        Uart::puthex(info.irq0_gsi);
        Uart::puts("\n");
        log(INFO,"MADT","IOAPIC base and IRQ0 GSI logged");
    }

    return info;
}
