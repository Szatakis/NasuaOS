#include "../driver.hpp"

extern framebuffer fb;

void clear_screen(uint32_t color)
{

    for(uint32_t y=0;y<fb.height;y++)
    {

        for(uint32_t x=0;x<fb.width;x++)
        {

            put_pixel(
                x,
                y,
                color
            );

        }

    }

}