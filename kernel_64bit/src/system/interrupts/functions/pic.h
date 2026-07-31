#pragma once

#include <stdint.h>

void pic_remap();
void pic_disable();
void pic_send_eoi(uint8_t irq);

// Sends EOI to proper interrupts contrroler.
// If LAPIC is active sends to it else sends to 8259 PIC.
void irq_send_eoi(uint8_t irq);
