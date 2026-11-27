#pragma once

#include <stdint.h>

namespace Pci
{
    extern bool found_uhci;
    extern bool found_ohci;
    extern bool found_ehci;
    extern bool found_xhci;
    extern bool found_unknown_usb;

    uint32_t config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
    uint16_t config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
    uint8_t config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

    void config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
    void config_write16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint16_t value);
    void config_write8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint8_t value);

    void init();
    void scan();
}