#include "../driver.hpp"

#include "system/drivers/timer/driver.hpp"

#include "libs/asm/asm.h"

#define PIT_FREQ 1193182
#define SPEAKER_PORT 0x61


void pc_speaker_on(uint32_t frequency)
{
    if (frequency == 0)
    {
        return;
    }
    
    uint32_t divisor = PIT_FREQ / frequency;

    // ustaw kanał 2 PIT
    outb(0x43, 0xB6);

    // low byte
    outb(0x42, divisor & 0xFF);

    // high byte
    outb(0x42, (divisor >> 8) & 0xFF);

    // turn on speaker
    uint8_t tmp = inb(SPEAKER_PORT);

    outb(SPEAKER_PORT, tmp | 3);
}


void pc_speaker_off()
{
    uint8_t tmp = inb(SPEAKER_PORT);

    outb(SPEAKER_PORT, tmp & ~3);
}

void beep(uint32_t freq, uint32_t duration)
{
    pc_speaker_on(freq);
    sleep(duration);
    pc_speaker_off();
}