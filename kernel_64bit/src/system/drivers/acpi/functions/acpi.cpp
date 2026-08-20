#include "../driver.hpp"
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "system/drivers/gpu/driver.hpp"

#include "applications/shell/commands.hpp"

#include "libs/asm/asm.hpp"


extern "C" volatile struct limine_rsdp_request rsdp_request;
extern "C" volatile struct limine_hhdm_request hhdm_request;


// GLOBAL ACPI VARIABLES

static uint32_t SMI_CMD = 0;
static uint8_t ACPI_ENABLE = 0;

static uint32_t PM1a_CNT = 0;
static uint32_t PM1b_CNT = 0;
static uint32_t PM1a_EVT = 0;

static uint16_t SLP_TYPa = 0;
static uint16_t SLP_TYPb = 0;

static uint16_t SLP_EN = (1 << 13);
static uint16_t SCI_EN = (1 << 0);


// ACPI STRUCTURES

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

    // ACPI 2.0+

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


// FADT
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


    uint8_t reserved2[10];


    uint64_t X_FirmwareCtrl;
    uint64_t X_Dsdt;


    GAS X_PM1aControlBlock;
    GAS X_PM1bControlBlock;


    uint8_t rest[64];
};


// HELPERS
static void* phys_to_virt(uint64_t addr)
{
    if (!addr)
    {
        return nullptr;
    }


    if (!hhdm_request.response)
    {
        return (void*)addr;
    }


    return (void*)(addr + hhdm_request.response->offset);
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


// AML PARSER
static uint16_t parse_aml_element(uint8_t** ptr)
{
    uint8_t op = **ptr;
    (*ptr)++;

    switch(op)
    {
        // ZeroOp
        case 0x00:
        {
            return 0;
        }


        // OneOp
        case 0x01:
        {
            return 1;
        }


        // BytePrefix
        case 0x0A:
        {
            uint8_t value = **ptr;

            (*ptr)++;

            return value;
        }

        // WordPrefix
        case 0x0B:
        {
            uint16_t value = (*ptr)[0] | ((*ptr)[1] << 8);

            (*ptr)+=2;

            return value;
        }


        // OnesOp
        case 0xFF:
        {
            return 0xFFFF;
        }

        default:
        {
            return 0;
        }
    }
}


// FIND _S5_
static bool parse_s5_from_aml(uint8_t* aml, uint32_t length)
{
    for(uint32_t i = 0; i < length - 5; i++)
    {
        if(aml[i] == '_' && aml[i+1] == 'S' && aml[i+2] == '5' && aml[i+3] == '_')
        {
            uint8_t* ptr = &aml[i+4];


            // Must be NameOp
            if(*ptr != 0x12)
            {
                continue;
            }


            ptr++;


            // PkgLength
            uint8_t lead = *ptr++;
            uint8_t extra = (lead >> 6);


            ptr += extra;


            // NumElements
            ptr++;


            SLP_TYPa = parse_aml_element(&ptr);
            SLP_TYPb = parse_aml_element(&ptr);


            return true;
        }
    }


    return false;
}


// TABLE SEARCH

static void* find_table(ACPISDTHeader* root, const char* name)
{
    bool xsdt = acpi_sig(root->Signature, "XSDT");
    bool rsdt = acpi_sig(root->Signature, "RSDT");


    if(!xsdt && !rsdt)
    {
        return nullptr;
    }

    uint32_t entries;


    if(xsdt)
    {
        entries = (root->Length - sizeof(ACPISDTHeader)) / 8;
    }
    else
    {
        entries = (root->Length - sizeof(ACPISDTHeader)) / 4;
    }



    uint8_t* ptr = (uint8_t*)root + sizeof(ACPISDTHeader);


    for(uint32_t i = 0; i < entries; i++)
    {
        uint64_t addr;

        if(xsdt)
        {
            addr = ((uint64_t*)ptr)[i];
        }
        else
        {
            addr = ((uint32_t*)ptr)[i];
        }

        if(!addr)
        {
            continue;
        }

        ACPISDTHeader* table = (ACPISDTHeader*)phys_to_virt(addr);

        if(!table)
        {
            continue;
        }

        if(acpi_sig(table->Signature, name))
        {
            return table;
        }
    }


    return nullptr;
}


// ACPI INIT

bool acpi_init()
{
    if(!rsdp_request.response)
    {
        print_error("ACPI: No RSDP\n");
        return false;
    }


    RSDP* rsdp = (RSDP*)rsdp_request.response->address;


    if(!rsdp)
    {
        return false;
    }


    ACPISDTHeader* root = nullptr;


    bool xsdt = false;


    if(rsdp->revision >= 2 && rsdp->xsdt_address)
    {
        root = (ACPISDTHeader*)phys_to_virt(rsdp->xsdt_address);

        xsdt = true;
    }
    else
    {
        root =(ACPISDTHeader*)phys_to_virt(rsdp->rsdt_address);
    }


    if(!root)
    {
        print_error("ACPI: No root table\n");
        return false;
    }

    print("ACPI: ");

    if(xsdt)
    {
        print("Using XSDT\n");
    }
    else
    {
        print("Using RSDT\n");
    }


    // FIND FACP
    FADT* facp = (FADT*)find_table(root,"FACP");

    if(!facp)
    {
        print_error("ACPI: FACP not found\n");
        return false;
    }

    print("ACPI: FACP found\n");

    // PM1 CONTROL BLOCK
    if(facp->header.Length >= 148 && facp->X_PM1aControlBlock.Address != 0 && facp->X_PM1aControlBlock.AddressSpace == 0)
    {
        PM1a_CNT = (uint32_t)facp->X_PM1aControlBlock.Address;
    }
    else
    {
        PM1a_CNT = facp->PM1aControlBlock;
    }


    if(facp->header.Length >= 160 && facp->X_PM1bControlBlock.Address != 0 && facp->X_PM1bControlBlock.AddressSpace == 0)
    {
        PM1b_CNT = (uint32_t)facp->X_PM1bControlBlock.Address;
    }
    else
    {
        PM1b_CNT = facp->PM1bControlBlock;
    }


    PM1a_EVT = facp->PM1aEventBlock;
    SMI_CMD = facp->SMI_CommandPort;
    ACPI_ENABLE = facp->AcpiEnable;


    print("ACPI: PM1a=");
    print_num8(PM1a_CNT);
    print("\n");


    // DSDT
    uint64_t dsdt_addr;

    if(facp->header.Length >= 148 && facp->X_Dsdt != 0)
    {
        dsdt_addr = facp->X_Dsdt;
    }
    else
    {
        dsdt_addr = facp->Dsdt;
    }

    ACPISDTHeader* dsdt = (ACPISDTHeader*)phys_to_virt(dsdt_addr);



    if(dsdt)
    {
        uint32_t len = dsdt->Length - sizeof(ACPISDTHeader);

        if(parse_s5_from_aml((uint8_t*)dsdt + sizeof(ACPISDTHeader), len))
        {
            print("ACPI: _S5 found in DSDT\n");
            return true;
        }
    }


    // SSDT SEARCH
    uint32_t entries;


    if(xsdt)
    {
        entries = (root->Length - sizeof(ACPISDTHeader)) / 8;
    }
    else
    {
        entries = (root->Length - sizeof(ACPISDTHeader)) / 4;
    }


    uint8_t* ptr = (uint8_t*)root + sizeof(ACPISDTHeader);


    for(uint32_t i = 0; i < entries; i++)
    {
        uint64_t addr;

        if(xsdt)
        {
            addr = ((uint64_t*)ptr)[i];
        }
        else
        {
            addr = ((uint32_t*)ptr)[i];
        }


        ACPISDTHeader* table = (ACPISDTHeader*)phys_to_virt(addr);

        if(!table)
        {
            continue;
        }

        if(acpi_sig(table->Signature,"SSDT"))
        {
            uint32_t len = table->Length - sizeof(ACPISDTHeader);

            if(parse_s5_from_aml((uint8_t*)table + sizeof(ACPISDTHeader), len))
            {
                print("ACPI: _S5 found in SSDT\n");
                return true;
            }
        }
    }


    print_error("ACPI: _S5 not found\n");

    return false;
}


// ACPI ENABLE

static bool acpi_enable()
{
    if(PM1a_CNT == 0)
    {
        print_error("ACPI: PM1a_CNT missing\n");
        return false;
    }

    // ACPI is alredy active

    if((inw((uint16_t)PM1a_CNT) & SCI_EN) != 0)
    {
        return true;
    }


    if(SMI_CMD == 0 || ACPI_ENABLE == 0)
    {
        print_error("ACPI: Cannot enable ACPI\n");
        return false;
    }

    print("ACPI: Enabling ACPI mode\n");


    outb((uint16_t)SMI_CMD, ACPI_ENABLE);


    // Wait for BIOS to handle ACPI

    for(int i = 0; i < 300; i++)
    {
        if((inw((uint16_t)PM1a_CNT) & SCI_EN) != 0)
        {
            print("ACPI: Enabled\n");
            return true;
        }

        for(int j = 0; j < 1000; j++)
        {
            io_wait();
        }
    }

    print_error("ACPI: Enable timeout\n");

    return false;
}


// SHUTDOWN

void acpi_shutdown()
{
    print_warn("NasuaOS: Shutting down\n");


    if(!acpi_init())
    {
        print_error("ACPI: initialization failed\n");

        asm volatile("cli");

        while(true)
        {
            asm volatile("hlt");
        }
    }


    if(!acpi_enable())
    {
        print_error("ACPI: cannot enable\n");
        return;
    }

    asm volatile("cli");

    // clean events

    if(PM1a_EVT)
    {
        outw((uint16_t)PM1a_EVT, 0xFFFF);
    }

    print("ACPI: Sending sleep command\n");


    uint16_t slp_a = (SLP_TYPa << 10) | SLP_EN;
    uint16_t slp_b = (SLP_TYPb << 10) | SLP_EN;


    outw((uint16_t)PM1a_CNT, slp_a);

    if(PM1b_CNT)
    {
        outw((uint16_t)PM1b_CNT, slp_b);
    }


    while(true)
    {
        asm volatile("hlt");
    }
}


// REBOOT

void acpi_reboot()
{
    print_warn("NasuaOS: Restarting\n");

    asm volatile("cli");


    while(inb(0x64) & 0x02)
    {
        io_wait();
    }


    outb(0x64, 0xFE);

    // fallback
    for(;;)
    {
        asm volatile("hlt");
    }
}