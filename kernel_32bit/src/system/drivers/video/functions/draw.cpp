#include "../driver.hpp"

#include "../../../fonts/font8x8.h"

extern uint32_t* buffer;
extern framebuffer fb;
extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern bool cursor_state;

void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{

    if(x >= fb.width)
    {
        return;
    }


    if(y >= fb.height)
        return;



    uint32_t offset =
        y * (fb.pitch / 4) + x;



    buffer[offset] = color;

}

void draw_cursor()
{
    if(!cursor_state)
        return;


    for(uint32_t x=0;x<8;x++)
    {
        put_pixel(
            cursor_x+x,
            cursor_y+7,
            text_color
        );
    }
}

void draw_char(char c,uint32_t x, uint32_t y, uint32_t color)
{
    const uint8_t* glyph = font8x8[(uint8_t)c];


    for(int row=0; row<8; row++)
    {

        for(int col=0; col<8; col++)
        {

            if(glyph[row] & (1 << (7-col)))
            {

                put_pixel(
                    x+col,
                    y+row,
                    color
                );

            }

        }

    }
}

void clear_cursor()
{
    for(uint32_t x = 0; x < CHAR_WIDTH; x++)
    {
        put_pixel(
            cursor_x + x,
            cursor_y + CHAR_HEIGHT - 1,
            background_color
        );
    }
}

void clear_char(uint32_t x,uint32_t y)
{
    for(uint32_t yy=0; yy<CHAR_HEIGHT; yy++)
    {
        for(uint32_t xx=0; xx<CHAR_WIDTH; xx++)
        {
            put_pixel(
                x+xx,
                y+yy,
                background_color
            );
        }
    }
}
