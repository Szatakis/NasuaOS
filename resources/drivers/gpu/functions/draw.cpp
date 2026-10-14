#include "../driver.hpp"
#include "fonts/fonts.hpp"

namespace Gpu
{
    void draw_char8(unsigned char character, size_t x, size_t y, uint32_t color)
    {
        for (size_t row = 0; row < 8; ++row)
        {
            const uint8_t bits = font8x8[character][row];
            for (size_t column = 0; column < 8; ++column)
            {
                if (bits & (1U << (7 - column)))
                {
                    put_pixel(x + column, y + row, color);
                }
            }
        }
    }

    void draw_char8_clipped(unsigned char character, size_t x, size_t y,
                            uint32_t color, size_t clip_x1, size_t clip_x2)
    {
        for (size_t row = 0; row < 8; ++row)
        {
            const uint8_t bits = font8x8[character][row];
            for (size_t column = 0; column < 8; ++column)
            {
                const size_t pixel_x = x + column;
                if ((bits & (1U << (7 - column))) &&
                    pixel_x >= clip_x1 && pixel_x < clip_x2)
                {
                    put_pixel(pixel_x, y + row, color);
                }
            }
        }
    }

    void draw_char10(unsigned char character, size_t x, size_t y, uint32_t color)
    {
        for (size_t row = 0; row < 10; ++row)
        {
            const uint16_t bits = font10x10[character][row];
            for (size_t column = 0; column < 10; ++column)
            {
                if (bits & (1U << (9 - column)))
                {
                    put_pixel(x + column, y + row, color);
                }
            }
        }
    }

    void draw_char12(unsigned char character, size_t x, size_t y, uint32_t color)
    {
        for (size_t row = 0; row < 12; ++row)
        {
            const uint16_t bits = font12x12[character][row];
            for (size_t column = 0; column < 12; ++column)
            {
                if (bits & (1U << (11 - column)))
                {
                    put_pixel(x + column, y + row, color);
                }
            }
        }
    }

    void draw_char16(unsigned char character, size_t x, size_t y, uint32_t color)
    {
        for (size_t row = 0; row < 16; ++row)
        {
            const uint16_t bits = font16x16[character][row];
            for (size_t column = 0; column < 16; ++column)
            {
                if (bits & (1U << (15 - column)))
                {
                    put_pixel(x + column, y + row, color);
                }
            }
        }
    }

    void fill_block(size_t x, size_t y, uint32_t color, size_t size_x, size_t size_y)
    {
        if (!fb || x >= fb->width || y >= fb->height)
        {
            return;
        }

        if (size_x > fb->width - x)
        {
            size_x = fb->width - x;
        }
        if (size_y > fb->height - y)
        {
            size_y = fb->height - y;
        }

        for (size_t row = 0; row < size_y; ++row)
        {
            for (size_t column = 0; column < size_x; ++column)
            {
                put_pixel(x + column, y + row, color);
            }
        }
    }

    void draw_rect(int x1, int y1, int x2, int y2, uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        if (x1 > x2)
        {
            const int temp = x1;
            x1 = x2;
            x2 = temp;
        }
        if (y1 > y2)
        {
            const int temp = y1;
            y1 = y2;
            y2 = temp;
        }

        if (x2 <= 0 || y2 <= 0 || x1 >= static_cast<int>(fb->width) || y1 >= static_cast<int>(fb->height))
        {
            return;
        }

        if (x1 < 0) 
        {
            x1 = 0;
        }
        if (y1 < 0) 
        {
            y1 = 0;
        }
        if (x2 > static_cast<int>(fb->width)) 
        {
            x2 = static_cast<int>(fb->width);
        }
        if (y2 > static_cast<int>(fb->height)) 
        {
            y2 = static_cast<int>(fb->height);
        }

        for (int y = y1; y < y2; ++y)
        {
            for (int x = x1; x < x2; ++x)
            {
                put_pixel(static_cast<size_t>(x), static_cast<size_t>(y), color);
            }
        }
    }
}
