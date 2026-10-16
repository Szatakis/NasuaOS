#include "../driver.hpp"
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "drivers/gpu/driver.hpp"
#include "libs/asm/asm.hpp"

extern "C" volatile struct limine_rsdp_request rsdp_request;
extern "C" volatile struct limine_hhdm_request hhdm_request;

static uint32_t SMI_CMD = 0;
static uint8_t ACPI_ENABLE = 0;

static uint8_t SLP_TYPa = 0;
static uint8_t SLP_TYPb = 0;

static const uint16_t SCI_EN = 1 << 0;
static const uint16_t SLP_EN = 1 << 13;
static const uint16_t SLP_TYP_MASK = 7 << 10;

struct __attribute__((packed)) ACPISDTHeader
{
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
};

struct __attribute__((packed)) RSDP
{
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct __attribute__((packed)) GAS
{
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
};

struct __attribute__((packed)) FADT
{
    ACPISDTHeader header;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;
    uint8_t reserved;
    uint8_t PreferredPMProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t AcpiEnable;
    uint8_t AcpiDisable;
    uint8_t S4BIOS_REQ;
    uint8_t PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t PM1EventLength;
    uint8_t PM1ControlLength;
    uint8_t PM2ControlLength;
    uint8_t PMTimerLength;
    uint8_t GPE0Length;
    uint8_t GPE1Length;
    uint8_t GPE1Base;
    uint8_t CSTControl;
    uint16_t C2Latency;
    uint16_t C3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t DutyOffset;
    uint8_t DutyWidth;
    uint8_t DayAlarm;
    uint8_t MonthAlarm;
    uint8_t Century;
    uint16_t BootArchitectureFlags;
    uint8_t reserved2;
    uint32_t Flags;
    GAS ResetReg;
    uint8_t ResetValue;
    uint8_t reserved3[3];
    uint64_t X_FirmwareCtrl;
    uint64_t X_Dsdt;
    GAS X_PM1aEventBlock;
    GAS X_PM1bEventBlock;
    GAS X_PM1aControlBlock;
    GAS X_PM1bControlBlock;
};

struct PMControlRegister
{
    uint64_t address;
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
};

static PMControlRegister PM1a_CNT = {};
static PMControlRegister PM1b_CNT = {};

static void* phys_to_virt(uint64_t addr)
{
    if(!addr || !hhdm_request.response)
    {
        return nullptr;
    }

    uint64_t offset = hhdm_request.response->offset;
    if(addr > UINT64_MAX - offset)
    {
        return nullptr;
    }

    return (void*)(uintptr_t)(addr + offset);
}

static bool acpi_sig(const char* a, const char* b)
{
    for(int i = 0; i < 4; i++)
    {
        if(a[i] != b[i])
        {
            return false;
        }
    }

    return true;
}

static bool rsdp_sig(const char* signature)
{
    static const char expected[] = "RSD PTR ";
    for(uint8_t i = 0; i < sizeof(expected) - 1; i++)
    {
        if(signature[i] != expected[i])
        {
            return false;
        }
    }
    return true;
}

static bool checksum_valid(const void* data, uint32_t length)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint8_t sum = 0;

    for(uint32_t i = 0; i < length; i++)
    {
        sum = (uint8_t)(sum + bytes[i]);
    }

    return sum == 0;
}

static bool valid_sdt(ACPISDTHeader* table)
{
    return table && table->Length >= sizeof(ACPISDTHeader) && table->Length <= 1024 * 1024 && checksum_valid(table, table->Length);
}

static uint32_t root_entry_count(ACPISDTHeader* root, bool xsdt)
{
    uint32_t entry_size = xsdt ? 8 : 4;
    if(!valid_sdt(root) || (!acpi_sig(root->Signature, xsdt ? "XSDT" : "RSDT")) || ((root->Length - sizeof(ACPISDTHeader)) % entry_size) != 0)
    {
        return 0;
    }

    return (root->Length - sizeof(ACPISDTHeader)) / entry_size;
}

static uint64_t root_entry(ACPISDTHeader* root, bool xsdt, uint32_t index)
{
    uint8_t* entries = (uint8_t*)root + sizeof(ACPISDTHeader);
    if(xsdt)
    {
        const uint8_t* entry = entries + index * 8;
        uint64_t address = 0;
        for(uint8_t i = 0; i < 8; i++)
        {
            address |= (uint64_t)entry[i] << (i * 8);
        }
        return address;
    }

    const uint8_t* entry = entries + index * 4;
    uint32_t address = 0;
    for(uint8_t i = 0; i < 4; i++)
    {
        address |= (uint32_t)entry[i] << (i * 8);
    }
    return address;
}

static ACPISDTHeader* find_table(ACPISDTHeader* root, bool xsdt, const char* name)
{
    uint32_t entries = root_entry_count(root, xsdt);
    for(uint32_t i = 0; i < entries; i++)
    {
        ACPISDTHeader* table = (ACPISDTHeader*)phys_to_virt(root_entry(root, xsdt, i));
        if(valid_sdt(table) && acpi_sig(table->Signature, name))
        {
            return table;
        }
    }

    return nullptr;
}

static bool parse_aml_integer(const uint8_t** cursor, const uint8_t* end, uint64_t* value)
{
    if(*cursor >= end)
    {
        return false;
    }

    uint8_t op = *(*cursor)++;
    switch(op)
    {
        case 0x00:
            *value = 0;
            return true;
        case 0x01:
            *value = 1;
            return true;
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0E:
        {
            uint8_t bytes = op == 0x0A ? 1 : op == 0x0B ? 2 : op == 0x0C ? 4 : 8;
            if((size_t)(end - *cursor) < bytes)
            {
                return false;
            }

            *value = 0;
            for(uint8_t i = 0; i < bytes; i++)
            {
                *value |= (uint64_t)(*cursor)[i] << (i * 8);
            }
            *cursor += bytes;
            return true;
        }
        default:
            return false;
    }
}

static bool parse_s5_from_aml(const uint8_t* aml, uint32_t length)
{
    if(length < 7)
    {
        return false;
    }

    const uint8_t* end = aml + length;
    for(const uint8_t* name = aml; (size_t)(end - name) >= 6; name++)
    {
        if(name[0] != '_' || name[1] != 'S' || name[2] != '5' || name[3] != '_' || name[4] != 0x12)
        {
            continue;
        }

        const uint8_t* cursor = name + 5;
        if(cursor >= end)
        {
            continue;
        }

        uint8_t lead = *cursor++;
        uint8_t follow_bytes = lead >> 6;
        uint32_t package_length_bytes = 1 + follow_bytes;
        uint32_t package_length;
        if(follow_bytes == 0)
        {
            package_length = lead & 0x3F;
        }
        else
        {
            if((size_t)(end - cursor) < follow_bytes)
            {
                continue;
            }

            package_length = lead & 0x0F;
            for(uint8_t i = 0; i < follow_bytes; i++)
            {
                package_length |= (uint32_t)(*cursor++) << (4 + 8 * i);
            }
        }

        if(cursor >= end)
        {
            continue;
        }

        if(package_length < package_length_bytes + 1)
        {
            continue;
        }

        const uint8_t* package_end = cursor + package_length - package_length_bytes;
        if(package_end < cursor || package_end > end || cursor >= package_end)
        {
            continue;
        }

        uint8_t elements = *cursor++;
        uint64_t type_a;
        uint64_t type_b;
        if(elements >= 2 && parse_aml_integer(&cursor, package_end, &type_a) && parse_aml_integer(&cursor, package_end, &type_b) && type_a <= 7 && type_b <= 7)
        {
            SLP_TYPa = (uint8_t)type_a;
            SLP_TYPb = (uint8_t)type_b;
            return true;
        }
    }

    return false;
}

static bool parse_s5_from_table(ACPISDTHeader* table)
{
    if(!valid_sdt(table))
    {
        return false;
    }

    uint32_t aml_length = table->Length - sizeof(ACPISDTHeader);
    return parse_s5_from_aml((uint8_t*)table + sizeof(ACPISDTHeader), aml_length);
}

static PMControlRegister make_legacy_control(uint32_t address, uint8_t length)
{
    PMControlRegister reg = {};
    if(address && length >= 2)
    {
        reg.address = address;
        reg.address_space = 1;
        reg.bit_width = length * 8;
        reg.access_size = 2;
    }
    return reg;
}

static PMControlRegister make_extended_control(const GAS& gas)
{
    PMControlRegister reg = {};
    if(gas.Address && (gas.AddressSpace == 0 || gas.AddressSpace == 1) && gas.BitOffset == 0 && gas.BitWidth >= 16 && (gas.AccessSize == 0 || gas.AccessSize == 2 || gas.AccessSize == 3))
    {
        reg.address = gas.Address;
        reg.address_space = gas.AddressSpace;
        reg.bit_width = gas.BitWidth;
        reg.bit_offset = gas.BitOffset;
        reg.access_size = gas.AccessSize;
    }
    return reg;
}

static bool valid_control(const PMControlRegister& reg)
{
    if(!reg.address || reg.bit_width < 16 || reg.bit_offset != 0)
    {
        return false;
    }

    uint8_t access_size = reg.access_size;
    if(access_size == 0)
    {
        access_size = 2;
    }
    if(access_size != 2 && access_size != 3)
    {
        return false;
    }

    uint8_t access_bytes = access_size == 2 ? 2 : 4;
    if(reg.address_space == 1)
    {
        return reg.address <= 0xFFFF && reg.address + access_bytes <= 0x10000;
    }
    return reg.address_space == 0 && phys_to_virt(reg.address);
}

static bool read_control(const PMControlRegister& reg, uint32_t* value)
{
    if(!valid_control(reg) || !value)
    {
        return false;
    }

    uint8_t access_size = reg.access_size == 0 ? 2 : reg.access_size;
    if(reg.address_space == 1)
    {
        *value = access_size == 3 ? inl((uint16_t)reg.address) : inw((uint16_t)reg.address);
    }
    else
    {
        volatile void* address = (volatile void*)phys_to_virt(reg.address);
        if(access_size == 3)
        {
            *value = *(volatile uint32_t*)address;
        }
        else
        {
            *value = *(volatile uint16_t*)address;
        }
    }

    return true;
}

static bool write_control(const PMControlRegister& reg, uint32_t value)
{
    if(!valid_control(reg))
    {
        return false;
    }

    uint8_t access_size = reg.access_size == 0 ? 2 : reg.access_size;
    if(reg.address_space == 1)
    {
        if(access_size == 3)
        {
            outl((uint16_t)reg.address, value);
        }
        else
        {
            outw((uint16_t)reg.address, (uint16_t)value);
        }
    }
    else
    {
        volatile void* address = (volatile void*)phys_to_virt(reg.address);
        if(access_size == 3)
        {
            *(volatile uint32_t*)address = value;
        }
        else
        {
            *(volatile uint16_t*)address = (uint16_t)value;
        }
    }

    return true;
}

bool Acpi::init()
{
    PM1a_CNT = {};
    PM1b_CNT = {};
    SMI_CMD = 0;
    ACPI_ENABLE = 0;

    if(!rsdp_request.response || !rsdp_request.response->address)
    {
        Gpu::print_error("ACPI: No RSDP\n");
        return false;
    }

    RSDP* rsdp = (RSDP*)rsdp_request.response->address;
    if(!rsdp_sig(rsdp->signature) || !checksum_valid(rsdp, 20))
    {
        Gpu::print_error("ACPI: Invalid RSDP\n");
        return false;
    }

    ACPISDTHeader* root = nullptr;
    bool xsdt = false;
    if(rsdp->revision >= 2)
    {
        if(rsdp->length < sizeof(RSDP) || rsdp->length > 4096 ||
           !checksum_valid(rsdp, rsdp->length))
        {
            Gpu::print_error("ACPI: Invalid extended RSDP\n");
            return false;
        }

        if(rsdp->xsdt_address)
        {
            root = (ACPISDTHeader*)phys_to_virt(rsdp->xsdt_address);
            xsdt = true;
        }
    }

    if((!root || root_entry_count(root, xsdt) == 0) && rsdp->rsdt_address)
    {
        root = (ACPISDTHeader*)phys_to_virt(rsdp->rsdt_address);
        xsdt = false;
    }

    if(!root || root_entry_count(root, xsdt) == 0)
    {
        Gpu::print_error("ACPI: Invalid or missing root table\n");
        return false;
    }

    Gpu::print("ACPI: Using ");
    Gpu::print(xsdt ? "XSDT\n" : "RSDT\n");

    FADT* facp = (FADT*)find_table(root, xsdt, "FACP");
    if(!facp || facp->header.Length < offsetof(FADT, PM1ControlLength) + sizeof(facp->PM1ControlLength))
    {
        Gpu::print_error("ACPI: Invalid or missing FADT\n");
        return false;
    }

    PM1a_CNT = make_legacy_control(facp->PM1aControlBlock, facp->PM1ControlLength);
    PM1b_CNT = make_legacy_control(facp->PM1bControlBlock, facp->PM1ControlLength);

    if(facp->header.Length >= offsetof(FADT, X_PM1aControlBlock) + sizeof(GAS))
    {
        PMControlRegister extended = make_extended_control(facp->X_PM1aControlBlock);
        if(valid_control(extended))
        {
            PM1a_CNT = extended;
        }
    }

    if(facp->header.Length >= offsetof(FADT, X_PM1bControlBlock) + sizeof(GAS))
    {
        PMControlRegister extended = make_extended_control(facp->X_PM1bControlBlock);
        if(valid_control(extended))
        {
            PM1b_CNT = extended;
        }
    }

    SMI_CMD = facp->SMI_CommandPort;
    ACPI_ENABLE = facp->AcpiEnable;

    if(!valid_control(PM1a_CNT))
    {
        Gpu::print_error("ACPI: Unsupported PM1a control register\n");
        return false;
    }

    uint64_t dsdt_address = facp->Dsdt;
    if(facp->header.Length >= offsetof(FADT, X_Dsdt) + sizeof(facp->X_Dsdt) &&
       facp->X_Dsdt)
    {
        dsdt_address = facp->X_Dsdt;
    }

    ACPISDTHeader* dsdt = (ACPISDTHeader*)phys_to_virt(dsdt_address);
    if(parse_s5_from_table(dsdt))
    {
        Gpu::print("ACPI: _S5 found in DSDT\n");
        return true;
    }

    uint32_t entries = root_entry_count(root, xsdt);
    for(uint32_t i = 0; i < entries; i++)
    {
        ACPISDTHeader* table = (ACPISDTHeader*)phys_to_virt(root_entry(root, xsdt, i));
        if(table && acpi_sig(table->Signature, "SSDT") && parse_s5_from_table(table))
        {
            Gpu::print("ACPI: _S5 found in SSDT\n");
            return true;
        }
    }

    Gpu::print_error("ACPI: _S5 not found in valid AML tables\n");
    return false;
}

static bool acpi_enable()
{
    uint32_t control;
    if(!read_control(PM1a_CNT, &control))
    {
        Gpu::print_error("ACPI: Unable to read PM1a control register\n");
        return false;
    }

    if(control & SCI_EN)
    {
        return true;
    }

    if(!SMI_CMD || !ACPI_ENABLE || SMI_CMD > 0xFFFF)
    {
        Gpu::print_error("ACPI: Firmware cannot enable ACPI mode\n");
        return false;
    }

    Gpu::print("ACPI: Enabling ACPI mode\n");
    outb((uint16_t)SMI_CMD, ACPI_ENABLE);

    for(uint32_t i = 0; i < 300000; i++)
    {
        if(read_control(PM1a_CNT, &control) && (control & SCI_EN))
        {
            Gpu::print("ACPI: Enabled\n");
            return true;
        }
        io_wait();
    }

    Gpu::print_error("ACPI: Enable timeout\n");
    return false;
}

void Acpi::shutdown()
{
    Gpu::print_warn("NasuaOS: Shutting down\n");

    if(!Acpi::init())
    {
        Gpu::print_error("ACPI: Shutdown cancelled: initialization failed\n");
        return;
    }

    if(!acpi_enable())
    {
        Gpu::print_error("ACPI: Shutdown cancelled: ACPI mode unavailable\n");
        return;
    }

    uint32_t current;
    if(!read_control(PM1a_CNT, &current))
    {
        Gpu::print_error("ACPI: Shutdown cancelled: PM1a read failed\n");
        return;
    }

    uint32_t sleep_value = (current & ~(SLP_TYP_MASK | SLP_EN)) |
        ((uint32_t)(SLP_TYPa & 7) << 10) | SLP_EN;
    uint32_t current_b = 0;
    uint32_t sleep_b_value = 0;
    bool write_pm1b = false;
    if(PM1b_CNT.address && read_control(PM1b_CNT, &current_b))
    {
        sleep_b_value = (current_b & ~(SLP_TYP_MASK | SLP_EN)) |
            ((uint32_t)(SLP_TYPb & 7) << 10) | SLP_EN;
        write_pm1b = true;
    }
    else if(PM1b_CNT.address)
    {
        Gpu::print_error("ACPI: Unable to read PM1b control register\n");
    }

    Gpu::print("ACPI: Sending S5 power-off request\n");
    asm volatile("cli");

    if(write_pm1b && !write_control(PM1b_CNT, sleep_b_value))
    {
        Gpu::print_error("ACPI: PM1b write failed\n");
    }

    if(!write_control(PM1a_CNT, sleep_value))
    {
        Gpu::print_error("ACPI: PM1a write failed\n");
    }

    for(;;)
    {
        asm volatile("hlt");
    }
}

void Acpi::reboot()
{
    Gpu::print_warn("NasuaOS: Restarting\n");

    asm volatile("cli");

    while(inb(0x64) & 0x02)
    {
        io_wait();
    }

    outb(0x64, 0xFE);

    for(;;)
    {
        asm volatile("hlt");
    }
}
