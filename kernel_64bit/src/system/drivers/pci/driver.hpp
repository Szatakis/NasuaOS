#pragma once

#include <stdint.h>

extern bool found_uhci;
extern bool found_ohci;
extern bool found_ehci;
extern bool found_xhci;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

void pci_init();
void pci_scan();