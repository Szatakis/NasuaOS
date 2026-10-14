#include "../../driver.hpp"

#ifdef __i386__

#include "system/drivers/multiboot/multiboot.hpp"

namespace Gpu
{
    bool graphics_init(uint32_t mbi_addr)
    {
        auto* tag = reinterpret_cast<multiboot_tag*>(mbi_addr + 8);
        while (tag->type != 0)
        {
            if (tag->type == 8 && tag->size >= sizeof(multiboot_tag_framebuffer))
            {
                auto* frame = reinterpret_cast<multiboot_tag_framebuffer*>(tag);
                if (frame->framebuffer_addr > UINTPTR_MAX || frame->framebuffer_bpp != 32)
                {
                    return false;
                }

                static framebuffer_info framebuffer;
                framebuffer.address = frame->framebuffer_addr;
                framebuffer.pitch = frame->framebuffer_pitch;
                framebuffer.width = frame->framebuffer_width;
                framebuffer.height = frame->framebuffer_height;
                framebuffer.bpp = frame->framebuffer_bpp;

                fb = &framebuffer;
                init_backbuffer(framebuffer.width, framebuffer.height, framebuffer.pitch);
                return get_backbuffer_pitch() != 0;
            }

            tag = reinterpret_cast<multiboot_tag*>(reinterpret_cast<uint8_t*>(tag) + ((tag->size + 7) & ~7U));
        }

        return false;
    }

    void draw_cursor()
    {
        for (size_t x = 0; x < CHAR_WIDTH; ++x)
        {
            put_pixel(cursor_x + x, cursor_y + CHAR_HEIGHT - 1, current_text_color);
        }
    }

    void clear_cursor()
    {
        for (size_t x = 0; x < CHAR_WIDTH; ++x)
        {
            put_pixel(cursor_x + x, cursor_y + CHAR_HEIGHT - 1, background_color);
        }
    }

    void clear_char(uint32_t x, uint32_t y)
    {
        for (size_t row = 0; row < CHAR_HEIGHT; ++row)
        {
            for (size_t column = 0; column < CHAR_WIDTH; ++column)
            {
                put_pixel(x + column, y + row, background_color);
            }
        }
    }

    void putchar(char character)
    {
        if (!fb)
        {
            return;
        }

        const size_t previous_x = cursor_x;
        const size_t previous_y = cursor_y;
        clear_cursor();

        if (character == '\n')
        {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
        }
        else
        {
            draw_char8(static_cast<unsigned char>(character), cursor_x, cursor_y, current_text_color);
            cursor_x += CHAR_WIDTH;
        }

        if (cursor_x + CHAR_WIDTH > fb->width)
        {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
        }

        const bool screen_cleared = cursor_y + CHAR_HEIGHT > fb->height;
        if (screen_cleared)
        {
            cursor_x = 0;
            cursor_y = 0;
            clear_screen(background_color);
        }

        draw_cursor();
        if (screen_cleared)
        {
            render_frame();
        }
        else
        {
            render_frame_region(previous_x, previous_y, CHAR_WIDTH, CHAR_HEIGHT);
            render_frame_region(cursor_x, cursor_y + CHAR_HEIGHT - 1, CHAR_WIDTH, 1);
        }
    }

    void print_char8(char character)
    {
        putchar(character);
    }
}

#endif
