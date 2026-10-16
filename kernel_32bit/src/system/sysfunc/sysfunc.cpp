#include "sysfunc.hpp"

#include "libs/libc/libc.hpp"
#include "drivers/gpu/driver.hpp"
#include "system/drivers/multiboot/multiboot.hpp"
#include "libs/asm/asm.hpp"

extern bool safe_mode;
extern bool debug_mode;

namespace
{
    struct __attribute__((packed)) AcpiSdtHeader
    {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        uint8_t reserved[26];
    };

    struct __attribute__((packed)) AcpiRsdp
    {
        char signature[8];
        uint8_t checksum;
        uint8_t oem_id[6];
        uint8_t revision;
        uint32_t rsdt_address;
        uint32_t length;
        uint64_t xsdt_address;
        uint8_t extended_checksum;
        uint8_t reserved[3];
    };

    struct AcpiRegister
    {
        uint32_t address;
        uint8_t address_space;
        uint8_t bit_width;
        uint8_t bit_offset;
        uint8_t access_size;
    };

    static uint32_t multiboot_info_address = 0;
    static AcpiRegister pm1a_control = {};
    static AcpiRegister pm1b_control = {};
    static uint32_t smi_command_port = 0;
    static uint8_t acpi_enable_command = 0;
    static uint8_t sleep_type_a = 0;
    static uint8_t sleep_type_b = 0;

    static uint32_t read_u32(const uint8_t* data)
    {
        return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    }

    static uint64_t read_u64(const uint8_t* data)
    {
        return (uint64_t)read_u32(data) | ((uint64_t)read_u32(data + 4) << 32);
    }

    static bool matches(const char* a, const char* b, uint32_t length)
    {
        for(uint32_t i = 0; i < length; i++)
        {
            if(a[i] != b[i])
            {
                return false;
            }
        }
        return true;
    }

    static bool checksum_valid(const uint8_t* data, uint32_t length)
    {
        uint8_t sum = 0;
        for(uint32_t i = 0; i < length; i++)
        {
            sum = (uint8_t)(sum + data[i]);
        }
        return sum == 0;
    }

    static AcpiRsdp* find_rsdp()
    {
        if(!multiboot_info_address)
        {
            return nullptr;
        }

        const uint8_t* info = (const uint8_t*)(uintptr_t)multiboot_info_address;
        uint32_t total_size = read_u32(info);
        if(total_size < 16 || total_size > 16 * 1024 * 1024)
        {
            return nullptr;
        }

        AcpiRsdp* old_rsdp = nullptr;
        uint32_t offset = 8;
        while(offset + sizeof(multiboot_tag) <= total_size)
        {
            const multiboot_tag* tag = (const multiboot_tag*)(info + offset);
            if(tag->size < sizeof(multiboot_tag) || tag->size > total_size - offset)
            {
                return nullptr;
            }

            if((tag->type == 14 || tag->type == 15) && tag->size >= sizeof(multiboot_tag) + 20)
            {
                AcpiRsdp* rsdp = (AcpiRsdp*)(info + offset + sizeof(multiboot_tag));
                if(matches(rsdp->signature, "RSD PTR ", 8) && checksum_valid((const uint8_t*)rsdp, 20))
                {
                    if(tag->type == 15)
                    {
                        if(tag->size >= sizeof(multiboot_tag) + 36 && rsdp->length >= 36 && rsdp->length <= tag->size - sizeof(multiboot_tag) && checksum_valid((const uint8_t*)rsdp, rsdp->length))
                        {
                            return rsdp;
                        }
                    }
                    else
                    {
                        old_rsdp = rsdp;
                    }
                }
            }

            if(tag->type == 0)
            {
                break;
            }
            uint32_t aligned_size = (tag->size + 7) & ~7U;
            if(aligned_size > total_size - offset)
            {
                return nullptr;
            }
            offset += aligned_size;
        }

        return old_rsdp;
    }

    static AcpiSdtHeader* physical_table(uint64_t address)
    {
        if(!address || address > UINT32_MAX)
        {
            return nullptr;
        }
        return (AcpiSdtHeader*)(uintptr_t)(uint32_t)address;
    }

    static bool valid_table(AcpiSdtHeader* table)
    {
        return table && table->length >= sizeof(AcpiSdtHeader) && table->length <= 1024 * 1024 && checksum_valid((const uint8_t*)table, table->length);
    }

    static uint64_t table_entry(AcpiSdtHeader* root, bool xsdt, uint32_t index)
    {
        const uint8_t* entry = (const uint8_t*)root + sizeof(AcpiSdtHeader) + index * (xsdt ? 8 : 4);
        return xsdt ? read_u64(entry) : read_u32(entry);
    }

    static uint32_t table_count(AcpiSdtHeader* root, bool xsdt)
    {
        uint32_t entry_size = xsdt ? 8 : 4;
        if(!valid_table(root) || !matches(root->signature, xsdt ? "XSDT" : "RSDT", 4) || (root->length - sizeof(AcpiSdtHeader)) % entry_size != 0)
        {
            return 0;
        }
        return (root->length - sizeof(AcpiSdtHeader)) / entry_size;
    }

    static AcpiSdtHeader* find_table(AcpiSdtHeader* root, bool xsdt, const char* signature)
    {
        uint32_t count = table_count(root, xsdt);
        for(uint32_t i = 0; i < count; i++)
        {
            AcpiSdtHeader* table = physical_table(table_entry(root, xsdt, i));
            if(valid_table(table) && matches(table->signature, signature, 4))
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

        uint8_t opcode = *(*cursor)++;
        if(opcode == 0x00 || opcode == 0x01)
        {
            *value = opcode;
            return true;
        }

        uint8_t byte_count;
        switch(opcode)
        {
            case 0x0A: 
                byte_count = 1; 
                break;
            case 0x0B: 
                byte_count = 2; 
                break;
            case 0x0C: 
                byte_count = 4; 
                break;
            case 0x0E: 
                byte_count = 8; 
                break;
            default: 
                return false;
        }

        if((size_t)(end - *cursor) < byte_count)
        {
            return false;
        }

        *value = 0;
        for(uint8_t i = 0; i < byte_count; i++)
        {
            *value |= (uint64_t)(*cursor)[i] << (i * 8);
        }
        *cursor += byte_count;
        return true;
    }

    static bool find_sleep_types(AcpiSdtHeader* table)
    {
        if(!valid_table(table))
        {
            return false;
        }

        const uint8_t* aml = (const uint8_t*)table + sizeof(AcpiSdtHeader);
        const uint8_t* end = (const uint8_t*)table + table->length;
        for(const uint8_t* name = aml; (size_t)(end - name) >= 6; name++)
        {
            if(name[0] != '_' || name[1] != 'S' || name[2] != '5' || name[3] != '_' || name[4] != 0x12)
            {
                continue;
            }

            const uint8_t* cursor = name + 5;
            uint8_t type_lead = *cursor++;
            uint8_t extra_bytes = type_lead >> 6;
            uint32_t package_length_bytes = 1 + extra_bytes;
            uint32_t package_length;
            if(extra_bytes == 0)
            {
                package_length = type_lead & 0x3F;
            }
            else
            {
                if((size_t)(end - cursor) < extra_bytes)
                {
                    continue;
                }
                package_length = type_lead & 0x0F;
                for(uint8_t i = 0; i < extra_bytes; i++)
                {
                    package_length |= (uint32_t)(*cursor++) << (4 + i * 8);
                }
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

            uint8_t element_count = *cursor++;
            uint64_t type_a;
            uint64_t type_b;
            if(element_count >= 2 && parse_aml_integer(&cursor, package_end, &type_a) && parse_aml_integer(&cursor, package_end, &type_b) && type_a <= 7 && type_b <= 7)
            {
                sleep_type_a = (uint8_t)type_a;
                sleep_type_b = (uint8_t)type_b;
                return true;
            }
        }

        return false;
    }

    static AcpiRegister make_register(uint64_t address, uint8_t space, uint8_t width, uint8_t offset, uint8_t access_size)
    {
        AcpiRegister reg = {};
        if(address && address <= UINT32_MAX && (space == 0 || space == 1) && width >= 16 && offset == 0 && (access_size == 0 || access_size == 2 || access_size == 3))
        {
            reg.address = (uint32_t)address;
            reg.address_space = space;
            reg.bit_width = width;
            reg.bit_offset = offset;
            reg.access_size = access_size;
        }
        return reg;
    }

    static AcpiRegister extended_register(const uint8_t* gas)
    {
        return make_register(read_u64(gas + 4), gas[0], gas[1], gas[2], gas[3]);
    }

    static AcpiRegister legacy_register(uint32_t address, uint8_t length)
    {
        return make_register(address, 1, length * 8, 0, 2);
    }

    static bool register_valid(const AcpiRegister& reg)
    {
        if(!reg.address || reg.bit_width < 16 || reg.bit_offset != 0)
        {
            return false;
        }

        uint8_t size = reg.access_size == 0 ? 2 : reg.access_size;
        uint32_t bytes = size == 2 ? 2 : size == 3 ? 4 : 0;
        if(!bytes || reg.address > UINT32_MAX - (bytes - 1))
        {
            return false;
        }

        if(reg.address_space == 1)
        {
            return reg.address <= 0xFFFF && reg.address + bytes <= 0x10000;
        }
        return reg.address_space == 0;
    }

    static bool read_register(const AcpiRegister& reg, uint32_t* value)
    {
        if(!register_valid(reg) || !value)
        {
            return false;
        }

        uint8_t size = reg.access_size == 0 ? 2 : reg.access_size;
        if(reg.address_space == 1)
        {
            *value = size == 3 ? inl((uint16_t)reg.address) : inw((uint16_t)reg.address);
        }
        else if(size == 3)
        {
            *value = *(volatile uint32_t*)(uintptr_t)reg.address;
        }
        else
        {
            *value = *(volatile uint16_t*)(uintptr_t)reg.address;
        }
        return true;
    }

    static bool write_register(const AcpiRegister& reg, uint32_t value)
    {
        if(!register_valid(reg))
        {
            return false;
        }

        uint8_t size = reg.access_size == 0 ? 2 : reg.access_size;
        if(reg.address_space == 1)
        {
            if(size == 3)
            {
                outl((uint16_t)reg.address, value);
            }
            else
            {
                outw((uint16_t)reg.address, (uint16_t)value);
            }
        }
        else if(size == 3)
        {
            *(volatile uint32_t*)(uintptr_t)reg.address = value;
        }
        else
        {
            *(volatile uint16_t*)(uintptr_t)reg.address = (uint16_t)value;
        }
        return true;
    }

    static bool initialize_acpi()
    {
        AcpiRsdp* rsdp = find_rsdp();
        if(!rsdp)
        {
            Gpu::print("ACPI shutdown unavailable: bootloader supplied no valid RSDP\n");
            return false;
        }

        AcpiSdtHeader* root = nullptr;
        bool xsdt = false;
        if(rsdp->rsdt_address)
        {
            root = physical_table(rsdp->rsdt_address);
        }
        else if(rsdp->revision >= 2 && rsdp->length >= 36 && rsdp->xsdt_address)
        {
            root = physical_table(rsdp->xsdt_address);
            xsdt = true;
        }

        if(!root || table_count(root, xsdt) == 0)
        {
            if(!xsdt && rsdp->revision >= 2 && rsdp->length >= 36 && rsdp->xsdt_address)
            {
                root = physical_table(rsdp->xsdt_address);
                xsdt = true;
            }
        }
        if(!root || table_count(root, xsdt) == 0)
        {
            Gpu::print("ACPI shutdown unavailable: invalid RSDT/XSDT\n");
            return false;
        }

        AcpiSdtHeader* fadt = find_table(root, xsdt, "FACP");
        if(!fadt && !xsdt && rsdp->revision >= 2 && rsdp->length >= 36 && rsdp->xsdt_address)
        {
            root = physical_table(rsdp->xsdt_address);
            xsdt = true;
            fadt = find_table(root, xsdt, "FACP");
        }
        if(!fadt || fadt->length < 90)
        {
            Gpu::print("ACPI shutdown unavailable: invalid FADT\n");
            return false;
        }

        const uint8_t* fadt_bytes = (const uint8_t*)fadt;
        uint8_t pm1_control_length = fadt_bytes[89];
        pm1a_control = legacy_register(read_u32(fadt_bytes + 64), pm1_control_length);
        pm1b_control = legacy_register(read_u32(fadt_bytes + 68), pm1_control_length);

        if(fadt->length >= 184)
        {
            AcpiRegister extended_a = extended_register(fadt_bytes + 172);
            if(register_valid(extended_a))
            {
                pm1a_control = extended_a;
            }
        }
        if(fadt->length >= 196)
        {
            AcpiRegister extended_b = extended_register(fadt_bytes + 184);
            if(register_valid(extended_b))
            {
                pm1b_control = extended_b;
            }
        }

        if(fadt->length >= 53)
        {
            smi_command_port = read_u32(fadt_bytes + 48);
            acpi_enable_command = fadt_bytes[52];
        }

        if(!register_valid(pm1a_control))
        {
            Gpu::print("ACPI shutdown unavailable: unsupported PM1a register\n");
            return false;
        }

        AcpiSdtHeader* dsdt = nullptr;
        if(fadt->length >= 148)
        {
            uint64_t extended_dsdt = read_u64(fadt_bytes + 140);
            if(extended_dsdt && extended_dsdt <= UINT32_MAX)
            {
                dsdt = physical_table(extended_dsdt);
            }
        }
        if(!dsdt)
        {
            dsdt = physical_table(read_u32(fadt_bytes + 40));
        }

        if(find_sleep_types(dsdt))
        {
            return true;
        }

        uint32_t count = table_count(root, xsdt);
        for(uint32_t i = 0; i < count; i++)
        {
            AcpiSdtHeader* table = physical_table(table_entry(root, xsdt, i));
            if(table && matches(table->signature, "SSDT", 4) && find_sleep_types(table))
            {
                return true;
            }
        }

        Gpu::print("ACPI shutdown unavailable: _S5 not found in AML\n");
        return false;
    }
}

void halt()
{
    while(true)
    {
        asm volatile("hlt");
    }
}

void init_shutdown(uint32_t mbi_addr)
{
    multiboot_info_address = mbi_addr;
}

void shutdown()
{
    Gpu::print("NasuaOS: Shutting down...\n");
    pm1a_control = {};
    pm1b_control = {};
    smi_command_port = 0;
    acpi_enable_command = 0;

    if(!initialize_acpi())
    {
        return;
    }

    uint32_t control;
    if(!read_register(pm1a_control, &control))
    {
        Gpu::print("ACPI shutdown failed: cannot read PM1a\n");
        return;
    }

    if((control & 1) == 0)
    {
        if(!smi_command_port || smi_command_port > 0xFFFF || !acpi_enable_command)
        {
            Gpu::print("ACPI shutdown failed: firmware cannot enable ACPI mode\n");
            return;
        }

        outb((uint16_t)smi_command_port, acpi_enable_command);
        bool enabled = false;
        for(uint32_t i = 0; i < 300000; i++)
        {
            if(read_register(pm1a_control, &control) && (control & 1))
            {
                enabled = true;
                break;
            }
            io_wait();
        }
        if(!enabled)
        {
            Gpu::print("ACPI shutdown failed: ACPI mode enable timed out\n");
            return;
        }
    }

    uint32_t value_a = (control & ~((7U << 10) | (1U << 13))) | ((uint32_t)sleep_type_a << 10) | (1U << 13);
    uint32_t value_b = 0;
    bool write_b = pm1b_control.address && read_register(pm1b_control, &value_b);
    if(write_b)
    {
        value_b = (value_b & ~((7U << 10) | (1U << 13))) | ((uint32_t)sleep_type_b << 10) | (1U << 13);
    }

    Gpu::print("ACPI: Sending S5 power-off request\n");
    asm volatile("cli");
    if(write_b)
    {
        write_register(pm1b_control, value_b);
    }
    write_register(pm1a_control, value_a);
    halt();
}

void reboot()
{
    uint8_t status;

    do
    {
        asm volatile("inb $0x64, %0" : "=a"(status));
    } while(status & 0x02);

    asm volatile("movb $0xFE, %%al\n" "outb %%al, $0x64" : : : "ax");

    while(true)
    {
        asm volatile("hlt");
    }
}

void check_modules(uint32_t mbi_addr)
{
    multiboot_tag* tag;

    for(tag = (multiboot_tag*)(mbi_addr + 8); tag->type != 0; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7)))
    {

        if(tag->type == 3)
        {
            multiboot_tag_module* module = (multiboot_tag_module*)tag;
            const char* data = (const char*)(uintptr_t)module->mod_start;

            if(find_in_string(data,"SAFE_MODE"))
            {
                safe_mode = true;
            }

            if(find_in_string(data,"DEBUG"))
            {
                debug_mode = true;
            }
        }
    }

    if(safe_mode)
    {
        Gpu::print("Safe mode ON\n");
    }

    if(debug_mode)
    {
        Gpu::print("Debug mode ON\n");
    }
}
