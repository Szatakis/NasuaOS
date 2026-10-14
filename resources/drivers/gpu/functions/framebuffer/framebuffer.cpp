#include "../../driver.hpp"

namespace Gpu
{
    namespace
    {
#ifndef GPU_BACKBUFFER_WIDTH
        constexpr size_t BACKBUFFER_WIDTH = 4096;
#else
        constexpr size_t BACKBUFFER_WIDTH = GPU_BACKBUFFER_WIDTH;
#endif
#ifndef GPU_BACKBUFFER_HEIGHT
        constexpr size_t BACKBUFFER_HEIGHT = 2160;
#else
        constexpr size_t BACKBUFFER_HEIGHT = GPU_BACKBUFFER_HEIGHT;
#endif

        uint32_t backbuffer[BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT];
        size_t backbuffer_width = 0;
        size_t backbuffer_height = 0;
        size_t backbuffer_pitch = 0;
    }

    framebuffer_info* fb = nullptr;
    size_t cursor_x = 0;
    size_t cursor_y = 0;
    const uint32_t FONT_SPACING_W = 1;
    const uint32_t FONT_SPACING_H = 4;
    uint32_t background_color = 0x000000;

    void init_backbuffer(size_t width, size_t height, size_t pitch)
    {
        size_t pitch_pixels = pitch / sizeof(uint32_t);
        if (width == 0 || height == 0 || width > BACKBUFFER_WIDTH || height > BACKBUFFER_HEIGHT || pitch_pixels < width)
        {
            backbuffer_width = 0;
            backbuffer_height = 0;
            backbuffer_pitch = 0;
            return;
        }

        backbuffer_width = width;
        backbuffer_height = height;
        backbuffer_pitch = BACKBUFFER_WIDTH;
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
        if (!fb || x >= fb->width || y >= fb->height || x >= backbuffer_width || y >= backbuffer_height)
        {
            return;
        }

        backbuffer[y * backbuffer_pitch + x] = color;
    }

    void clear_screen(uint32_t color)
    {
        if (!fb)
        {
            return;
        }

        for (size_t y = 0; y < backbuffer_height; ++y)
        {
            for (size_t x = 0; x < backbuffer_width; ++x)
            {
                backbuffer[y * backbuffer_pitch + x] = color;
            }
        }
    }

    void render_frame_region(size_t x, size_t y, size_t width, size_t height)
    {
        if (!fb || !fb->address || fb->bpp != 32 || backbuffer_width == 0 || backbuffer_height == 0)
        {
            return;
        }

        auto* framebuffer = reinterpret_cast<volatile uint32_t*>(
            static_cast<uintptr_t>(fb->address));
        const size_t framebuffer_pitch = fb->pitch / sizeof(uint32_t);

        if (framebuffer_pitch < backbuffer_width)
        {
            return;
        }

        const size_t copy_width = fb->width < backbuffer_width ? fb->width : backbuffer_width;
        const size_t copy_height = fb->height < backbuffer_height ? fb->height : backbuffer_height;
        
        if (x >= copy_width || y >= copy_height)
        {
            return;
        }

        if (width > copy_width - x)
        {
            width = copy_width - x;
        }
        if (height > copy_height - y)
        {
            height = copy_height - y;
        }

        for (size_t row = y; row < y + height; ++row)
        {
            const size_t row_offset = row * framebuffer_pitch;
            const size_t backbuffer_offset = row * backbuffer_pitch;
            for (size_t column = x; column < x + width; ++column)
            {
                framebuffer[row_offset + column] = backbuffer[backbuffer_offset + column];
            }
        }
    }

    void render_frame()
    {
        render_frame_region(0, 0, backbuffer_width, backbuffer_height);
    }
}
