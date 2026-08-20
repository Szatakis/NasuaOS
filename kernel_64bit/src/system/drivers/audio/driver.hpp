#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

class Audio
{
public:
    static void pc_speaker_on(uint32_t frequency);
    static void pc_speaker_off();
    static void beep(uint32_t freq, uint32_t duration); // Beeps pc speaker for given amount of time
};

#endif