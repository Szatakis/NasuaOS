#include "../../driver.hpp"

#ifdef __i386__

#include "system/drivers/multiboot/multiboot.hpp"
#include "fonts/font8x8.h"

namespace Gpu
{
    framebuffer_info* fb = nullptr;

    size_t cursor_x = 0;
    size_t cursor_y = 0;

    uint32_t current_text_color = 0xFFFFFF;

    const uint32_t FONT_SPACING_W = 1;
    const uint32_t FONT_SPACING_H = 4;

    uint32_t background_color = 0x000000;

    uint32_t* buffer = nullptr;

    bool graphics_init(uint32_t mbi_addr)
    {
        multiboot_tag* tag;

        for (tag = (multiboot_tag*)(mbi_addr + 8); tag->type != 0; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7)))
        {
            if (tag->type == 8)
            {
                auto frame = (multiboot_tag_framebuffer*)tag;

                static framebuffer_info fb_data;

                fb_data.address = frame->framebuffer_addr;
                fb_data.pitch = frame->framebuffer_pitch;
                fb_data.width = frame->framebuffer_width;
                fb_data.height = frame->framebuffer_height;
                fb_data.bpp = frame->framebuffer_bpp;

                fb = &fb_data;
                buffer = (uint32_t*)(uintptr_t)fb_data.address;

                return true;
            }
        }

        return false;
    }

    void put_pixel(size_t x, size_t y, uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        if (x >= fb->width || y >= fb->height)
        {
            return;
        }

        uint32_t* buf = buffer;
        size_t pitch = fb->pitch / 4;

        buf[y * pitch + x] = color;
    }

    void clear_screen(uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        for (uint32_t y = 0; y < fb->height; y++)
        {
            for (uint32_t x = 0; x < fb->width; x++)
            {
                put_pixel(x, y, color);
            }
        }
    }

    void putchar(char c)
    {
        clear_cursor();

        if (c == '\n')
        {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
        }
        else
        {
            draw_char8(c, cursor_x, cursor_y, current_text_color);
            cursor_x += CHAR_WIDTH;
        }

        if (cursor_x >= fb->width)
        {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
        }

        if (cursor_y >= fb->height)
        {
            cursor_x = 0;
            cursor_y = 0;

            clear_screen(background_color);
        }

        draw_cursor();
    }

    void draw_cursor()
    {
        for (uint32_t x = 0; x < 8; x++)
        {
            put_pixel(cursor_x + x, cursor_y + 7, current_text_color);
        }
    }

    void clear_cursor()
    {
        for (uint32_t x = 0; x < CHAR_WIDTH; x++)
        {
            put_pixel(cursor_x + x, cursor_y + CHAR_HEIGHT - 1, background_color);
        }
    }

    void clear_char(uint32_t x, uint32_t y)
    {
        for (uint32_t yy = 0; yy < CHAR_HEIGHT; yy++)
        {
            for (uint32_t xx = 0; xx < CHAR_WIDTH; xx++)
            {
                put_pixel(x + xx, y + yy, background_color);
            }
        }
    }

    void draw_char8(unsigned char c, size_t x, size_t y, uint32_t color)
    {
        const uint8_t* glyph = font8x8[(uint8_t)c];

        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                if (glyph[row] & (1 << (7 - col)))
                {
                    put_pixel(x + col, y + row, color
                    );
                }
            }
        }
    }

    void print(const char* str)
    {
        while (*str)
        {
            putchar(*str);
            str++;
        }
    }

    void print_char8(char c)
    {
        putchar(c);
    }

    void print_num8(uint32_t num)
    {
        char buf[12];
        int i = 0;

        if (num == 0)
        {
            print("0");
            return;
        }

        while (num > 0)
        {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }

        while (i > 0)
        {
            char c[2];

            c[0] = buf[--i];
            c[1] = '\0';

            print(c);
        }
    }

    void fetch()
    {
        print("\n $$\\   $$\\                                          $$$$$$\\   $$$$$$\\  \n");
        print(" $$$\\  $$ |                                        $$  __$$\\ $$  __$$\\ \n");
        print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
        print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
        print(" $$ $$\\$$ | \\____$$\\ $$  _____|$$ |  $$ | \\____$$\\ $$ |  $$ |\\$$$$$$\\  \n");
        print(" $$ \\$$$$ | $$$$$$$ |\\$$$$$$\\  $$ |  $$ | $$$$$$$ |$$ |  $$ | \\____$$\\ \n");
        print(" $$ |\\$$$ |$$  __$$ | \\____$$\\ $$ |  $$ |$$  __$$ |$$ |  $$ |$$\\   $$ |\n");
        print(" $$ | \\$$ |\\$$$$$$$ |$$$$$$$  |\\$$$$$$  |\\$$$$$$$ | $$$$$$  |\\$$$$$$  |\n");
        print(" \\__|  \\__| \\_______|\\_______/  \\______/  \\_______| \\______/  \\______/ \n\n");
    }
} // namespace Gpu

#endif // __i386__


#ifdef __x86_64__

#include "system/gui/vars/colors.hpp"
#include "drivers/memory/driver.hpp"
#include "applications/shell/commands.hpp"

namespace Gpu
{
    framebuffer_info* fb = nullptr;

    size_t cursor_x = 0;
    size_t cursor_y = 0;

    uint32_t current_text_color = 0xFFFFFF;

    const uint32_t FONT_SPACING_W = 1;
    const uint32_t FONT_SPACING_H = 4;

    uint32_t background_color = 0x000000;

    static uint32_t backbuffer[4096 * 2160];

    static size_t backbuffer_width = 0;
    static size_t backbuffer_height = 0;
    static size_t backbuffer_pitch = 0;

    void init_backbuffer(size_t width, size_t height, size_t pitch)
    {
        backbuffer_width = width;
        backbuffer_height = height;
        backbuffer_pitch = pitch / 4;
    }

    uint32_t* get_backbuffer()
    {
        return backbuffer;
    }

    size_t get_backbuffer_pitch()
    {
        return backbuffer_pitch;
    }

    void put_pixel(size_t x, size_t y, uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        if (x >= fb->width || y >= fb->height)
        {
            return;
        }

        uint32_t* buf = get_backbuffer();
        size_t pitch = get_backbuffer_pitch();

        buf[y * pitch + x] = color;
    }

    void clear_screen(uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        uint32_t* bb_ptr = get_backbuffer();

        for (size_t y = 0; y < backbuffer_height; y++)
        {
            for (size_t x = 0; x < backbuffer_width; x++)
            {
                bb_ptr[y * backbuffer_pitch + x] = color;
            }
        }
    }

    void render_frame()
    {
        if (!fb || !fb->address)
        {
            return;
        }

        uint32_t* fb_ptr = (uint32_t*)fb->address;

        size_t pitch = fb->pitch / 4;

        for (size_t y = 0; y < backbuffer_height; y++)
        {
            ::Memory::memcpy(&fb_ptr[y * pitch], &backbuffer[y * backbuffer_pitch], backbuffer_width * sizeof(uint32_t));
        }
    }
} // namespace Gpu

#endif // __x86_64__