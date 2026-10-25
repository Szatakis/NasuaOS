#include <stdint.h>
#include <stdlib.h>

#include "doomgeneric.h"
#include "doomkeys.h"

extern uint64_t doom_nasua_get_ticks(void);

#define DOOM_KEY_QUEUE_SIZE 64

static struct
{
    unsigned char key;
    int pressed;
} key_queue[DOOM_KEY_QUEUE_SIZE];

static unsigned int key_read_index;
static unsigned int key_write_index;

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
}

void DG_SleepMs(uint32_t milliseconds)
{
    (void)milliseconds;
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)(doom_nasua_get_ticks() * 10);
}

int DG_GetKey(int* pressed, unsigned char* key)
{
    if (key_read_index == key_write_index || pressed == NULL || key == NULL)
    {
        return 0;
    }

    *key = key_queue[key_read_index].key;
    *pressed = key_queue[key_read_index].pressed;
    key_read_index = (key_read_index + 1) % DOOM_KEY_QUEUE_SIZE;
    return 1;
}

void DG_SetWindowTitle(const char* title)
{
    (void)title;
}

void doom_nasua_key_event(unsigned char key, int pressed)
{
    unsigned int next = (key_write_index + 1) % DOOM_KEY_QUEUE_SIZE;
    if (next == key_read_index)
    {
        key_read_index = (key_read_index + 1) % DOOM_KEY_QUEUE_SIZE;
    }

    key_queue[key_write_index].key = key;
    key_queue[key_write_index].pressed = pressed;
    key_write_index = next;
}
