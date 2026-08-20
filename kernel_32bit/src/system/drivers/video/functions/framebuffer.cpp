#include "../driver.hpp"

#include "../../multiboot/multiboot.hpp"

extern framebuffer fb;
extern uint32_t* buffer;
extern uint32_t max_cols;
extern uint32_t max_rows;

void clear_screen(uint32_t color)
{
    for(uint32_t y=0;y<fb.height;y++)
    {
        for(uint32_t x=0;x<fb.width;x++)
        {
            put_pixel(x, y, color);
        }
    }
}

bool graphics_init(uint32_t mbi_addr)
{
    multiboot_tag* tag;


    for(tag = (multiboot_tag*)(mbi_addr + 8); tag->type != 0; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7)))
    {
        if(tag->type == 8)
        {
            auto frame = (multiboot_tag_framebuffer*)tag;
            fb.addr = frame->framebuffer_addr;
            fb.pitch = frame->framebuffer_pitch;
            fb.width = frame->framebuffer_width;
            fb.height = frame->framebuffer_height;
            fb.bpp = frame->framebuffer_bpp;

            buffer = (uint32_t*)(uintptr_t)fb.addr;

            max_cols = fb.width / CHAR_WIDTH;
            max_rows = fb.height / CHAR_HEIGHT;

            return true;
        }
    }

    return false;
}

