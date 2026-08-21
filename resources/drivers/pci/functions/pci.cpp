#include "../driver.hpp"

#include <stdint.h>
#include <stddef.h>

#include "system/sysfunc/logger/logger.hpp"
#include "drivers/uart/driver.hpp"

namespace Pci
{

bool found_uhci = false;
bool found_ohci = false;
bool found_ehci = false;
bool found_xhci = false;
bool found_unknown_usb = false;


static void print_hex8(uint8_t value)
{
    const char* hex = "0123456789ABCDEF";

    char buffer[3];

    buffer[0] = hex[(value >> 4) & 0xF];
    buffer[1] = hex[value & 0xF];
    buffer[2] = '\0';

    Uart::puts(buffer);
}

static void print_hex16(uint16_t value)
{
    const char* hex = "0123456789ABCDEF";

    char buffer[5];

    buffer[0] = hex[(value >> 12) & 0xF];
    buffer[1] = hex[(value >> 8) & 0xF];
    buffer[2] = hex[(value >> 4) & 0xF];
    buffer[3] = hex[value & 0xF];
    buffer[4] = '\0';

    Uart::puts(buffer);
}

static const char* pci_class_name(uint8_t class_code, uint8_t subclass)
{
    if (class_code == 0x01)
    {
        return "Mass Storage";
    }

    if (class_code == 0x02)
    {
        return "Network";
    }
        
    if (class_code == 0x03)
    {
        return "Display";
    }
        
    if (class_code == 0x04)
    {
        return "Multimedia";
    }
        
    if (class_code == 0x06 && subclass == 0x00)
    {
        return "Host Bridge";
    }

    if (class_code == 0x06 && subclass == 0x01)
    {
        return "ISA Bridge";
    }

    if (class_code == 0x06 && subclass == 0x04)
    {
        return "PCI Bridge";
    }

    if (class_code == 0x06 && subclass == 0x80)
    {
        return "System Bridge";
    }
        
    if (class_code == 0x08)
    {
        return "System Peripheral";
    }
        
    if (class_code == 0x0C && subclass == 0x03)
    {
        return "USB Controller";
    }

    return "Unknown";
}

uint32_t config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)function << 8) | (offset & 0xFC);

    outl(0xCF8, address);

    return inl(0xCFC);
}

uint16_t config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t value = config_read32(bus, slot, function, offset);

    if (offset & 2)
    {
        return (uint16_t)(value >> 16);
    }

    return (uint16_t)(value & 0xFFFF);
}

uint8_t config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t value = config_read32(bus, slot, function, offset);
    uint8_t shift = (uint8_t)((offset & 3) * 8);

    return (uint8_t)((value >> shift) & 0xFF);
}

static void print_pci_device(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor, uint16_t device, uint8_t class_code, uint8_t subclass, uint8_t prog_if)
{
    Uart::puts("[PCI] ");

    print_hex8(bus);
    Uart::puts(":");

    print_hex8(slot);
    Uart::puts(".");

    print_hex8(function);

    Uart::puts(" Vendor=0x");
    print_hex16(vendor);

    Uart::puts(" Device=0x");
    print_hex16(device);

    Uart::puts(" Class=0x");
    print_hex8(class_code);

    Uart::puts(" Subclass=0x");
    print_hex8(subclass);

    Uart::puts(" ProgIF=0x");
    print_hex8(prog_if);

    Uart::puts(" [");
    Uart::puts(pci_class_name(class_code, subclass));
    Uart::puts("]\n");
}

void scan()
{
    Uart::puts("[PCI] Scanning PCI bus\n");

    bool found_device = false;

    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t slot = 0; slot < 32; slot++)
        {
            uint16_t vendor = config_read16((uint8_t)bus, slot, 0, 0x00);

            if (vendor == 0xFFFF)
            {
                continue;
            }


            uint8_t header_type = config_read8((uint8_t)bus, slot, 0, 0x0E);
            uint8_t function_count = (header_type & 0x80) ? 8 : 1;

            for (uint8_t function = 0; function < function_count; function++)
            {
                vendor = config_read16((uint8_t)bus, slot, function, 0x00);

                if (vendor == 0xFFFF)
                {
                    continue;
                }


                found_device = true;


                uint16_t device = config_read16((uint8_t)bus, slot, function,0x02);
                uint8_t class_code = config_read8((uint8_t)bus, slot, function, 0x0B);
                uint8_t subclass = config_read8((uint8_t)bus, slot, function, 0x0A);
                uint8_t prog_if = config_read8((uint8_t)bus, slot, function, 0x09);


                print_pci_device((uint8_t)bus, slot, function, vendor, device, class_code, subclass, prog_if);


                /*
                    USB controllers:

                    Class    = 0x0C
                    Subclass = 0x03
                
                    ProgIF:
                
                    0x00 = UHCI
                    0x10 = OHCI
                    0x20 = EHCI
                    0x30 = xHCI
                 */

                if (class_code == 0x0C && subclass == 0x03)
                {
                    if (prog_if == 0x00)
                    {
                        found_uhci = true;

                        Uart::puts("[PCI] UHCI USB 1.1 controller detected\n");
                        log(INFO,"PCI","UHCI USB 1.1 controller detected");
                    }
                    else if (prog_if == 0x10)
                    {
                        found_ohci = true;

                        Uart::puts("[PCI] OHCI USB 1.1 controller detected\n");
                        log(INFO,"PCI","OHCI USB 1.1 controller detected");
                    }
                    else if (prog_if == 0x20)
                    {
                        found_ehci = true;

                        Uart::puts("[PCI] EHCI USB 2.0 controller detected\n");
                        log(INFO,"PCI","EHCI USB 2.0 controller detected");
                    }
                    else if (prog_if == 0x30)
                    {
                        found_xhci = true;

                        Uart::puts("[PCI] xHCI USB 3.x controller detected\n");
                        log(INFO,"PCI","xHCI USB 3.x controller detected");
                    }
                    else
                    {
                        found_unknown_usb = true;

                        Uart::puts("[PCI] Unknown USB controller detected\n");
                        log(INFO,"PCI","Unknown USB controller detected");
                    }
                }
            }
        }
    }


    Uart::puts("\n[PCI] Scan complete\n");

    if (!found_device)
    {
        Uart::puts("[PCI] No PCI devices found\n");
        log(INFO,"PCI","No PCI devices found");
    }


    //USB summary

    Uart::puts("[PCI] USB controller summary:\n");

    if (found_uhci)
    {
        Uart::puts("[PCI]   UHCI  - USB 1.1\n");
    }
    if (found_ohci)
    {
        Uart::puts("[PCI]   OHCI  - USB 1.1\n");
    }
    if (found_ehci)
    {
        Uart::puts("[PCI]   EHCI  - USB 2.0\n");
    }
    if (found_xhci)
    {
        Uart::puts("[PCI]   xHCI  - USB 3.x\n");
    }
    if (found_unknown_usb)
    {
        Uart::puts("[PCI]   Unknown USB controller\n");
    }

    if (!found_uhci && !found_ohci && !found_ehci && !found_xhci && !found_unknown_usb)
    {
        Uart::puts("[PCI]   No USB controllers found\n");
    }


    //Pick the best available USB controller.
    //We want USB 2.0 first. Next usb 1.1. Next 3.x

    if (found_ehci)
    {
        Uart::puts("[PCI] Selected controller: EHCI (USB 2.0)\n");
        log(INFO,"PCI","Selected controller: EHCI (USB 2.0)");
    }
    else if (found_ohci)
    {
        Uart::puts("[PCI] Selected controller: OHCI (USB 1.1)\n");
        log(INFO,"PCI","Selected controller: OHCI (USB 1.1)");
    }
    else if (found_uhci)
    {
        Uart::puts("[PCI] Selected controller: UHCI (USB 1.1)\n");
        log(INFO,"PCI","Selected controller: UHCI (USB 1.1)");
    }
    else if (found_xhci)
    {
        Uart::puts("[PCI] Selected controller: xHCI (USB 3.x)\n");
        log(INFO,"PCI","Selected controller: xHCI (USB 3.x)");
    }
    else
    {
        Uart::puts("[PCI] No supported USB controller available\n");
        log(INFO,"PCI","No supported USB controller available");
    }
}

void init()
{
    Uart::puts("[PCI] Initializing PCI\n");

    scan();

    Uart::puts("[PCI] PCI initialization complete\n");
}

}