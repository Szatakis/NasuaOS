#include "background.hpp"

#include "system/gui/gui.hpp"
#include "system/gui/vars/colors.hpp"
#include "system/drivers/video/driver.hpp"
#include "system/drivers/memory/driver.hpp"


static uint32_t* background_buffer = nullptr;

static size_t background_pitch = 0;
static size_t background_size = 0;

size_t bar_h = 36;
size_t bar_h_scaled = 0; 


void image_init()
{
    if(!fb)
    {
        return;
    }

    background_pitch = get_backbuffer_pitch();
    background_size = background_pitch * fb->height * sizeof(uint32_t);
    background_buffer = (uint32_t*)kmalloc(background_size);

    if(!background_buffer)
    {
        return;
    }

    bar_h_scaled = (bar_h * fb->height) / 720;
    if(bar_h > bar_h_scaled)
    {
        bar_h = bar_h + bar_h_scaled;
        bar_h_scaled = bar_h - bar_h_scaled;
        bar_h = bar_h - bar_h_scaled;
    }

    for(size_t y = 0; y < fb->height; y++)
    {
        for(size_t x = 0; x < fb->width; x++)
        {
            background_buffer[y * background_pitch + x] = COLOR_NASUA_BG;
        }
    }

    for(size_t y = 0; y < fb->height; y++)
    {
        size_t src_y = (y * background_height) / fb->height;

        if(src_y >= background_height)
        {
            src_y = background_height - 1;
        }

        for(size_t x = 0; x < fb->width; x++)
        {
            size_t src_x = (x * background_width) / fb->width;

            if(src_x >= background_width)
            {
                src_x = background_width - 1;
            }

            background_buffer[y * background_pitch + x] = background_data[src_y * background_width + src_x];
        }
    }
}

void draw_background()
{
    if(!fb || !background_buffer)
    {
        return;
    }

    uint32_t* bb = get_backbuffer();

    if(!bb) 
    {
        return;
    }

    for(size_t y = 0; y < fb->height; y++)
    {
        memcpy(
            bb + y * background_pitch,
            background_buffer + y * background_pitch,
            fb->width * sizeof(uint32_t)
        );
    }
}