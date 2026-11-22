#include "boot_splash.hpp"

#include <stddef.h>

#include "drivers/gpu/driver.hpp"
#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"
#include "libs/libc/libc.hpp"
#include "fonts/fonts.hpp"

namespace
{
    constexpr uint32_t LOG_MAX_LINES = 18;
    constexpr size_t LOG_MESSAGE_SIZE = 96;
    constexpr size_t TITLE_FONT_SCALE = 3;
    constexpr size_t LOG_LINE_H = 12;

    bool active = false;
    uint32_t progress = 0;

    char log_lines[LOG_MAX_LINES][LOG_MESSAGE_SIZE];
    uint32_t log_count = 0;
    uint32_t log_next = 0;

    // Layout (computed from framebuffer size)
    size_t logo_size = 0;
    size_t logo_x = 0;
    size_t logo_y = 0;
    size_t title_y = 0;
    size_t bar_x = 0;
    size_t bar_y = 0;
    size_t bar_w = 0;
    size_t bar_h = 0;
    size_t log_x = 0;
    size_t log_y = 0;
    size_t log_w = 0;
    size_t log_h = 0;
    size_t log_line_h = LOG_LINE_H;
    uint32_t log_visible_max = LOG_MAX_LINES;

    void compute_layout()
    {
        const size_t w = Gpu::fb->width;
        const size_t h = Gpu::fb->height;

        logo_size = w / 8;
        if (logo_size < 64)
        {
            logo_size = 64;
        }
        if (logo_size > 192)
        {
            logo_size = 192;
        }

        logo_x = (w - logo_size) / 2;
        logo_y = h / 8;

        title_y = logo_y + logo_size + 24;

        bar_w = w / 2;
        if (bar_w > 640)
        {
            bar_w = 640;
        }
        if (bar_w < 200)
        {
            bar_w = 200;
        }
        bar_h = 14;
        bar_x = (w - bar_w) / 2;
        bar_y = title_y + 64;

        log_x = w / 16;
        log_w = w - 2 * log_x;
        log_h = h / 3;
        if (log_h > 260)
        {
            log_h = 260;
        }
        if (log_h < 140)
        {
            log_h = 140;
        }
        log_y = h - log_h - h / 16;
        log_line_h = LOG_LINE_H;

        // Header ("boot log" + padding) + one line height must fit the panel
        const size_t usable = log_h > 30 ? log_h - 30 : log_h;
        log_visible_max = (uint32_t)(usable / log_line_h);
        if (log_visible_max < 1)
        {
            log_visible_max = 1;
        }
        if (log_visible_max > LOG_MAX_LINES)
        {
            log_visible_max = LOG_MAX_LINES;
        }
    }

    void draw_logo()
    {
        constexpr size_t icon_w = 32;
        constexpr size_t icon_h = 32;

        const size_t scale = logo_size / icon_w;
        const size_t scaled_w = icon_w * scale;
        const size_t scaled_h = icon_h * scale;
        const size_t ox = logo_x + (logo_size - scaled_w) / 2;
        const size_t oy = logo_y + (logo_size - scaled_h) / 2;

        for (size_t y = 0; y < icon_h; ++y)
        {
            for (size_t x = 0; x < icon_w; ++x)
            {
                const uint32_t color = start_icon[y * icon_w + x];
                if (color == 0x000000)
                {
                    continue;
                }

                const uint32_t argb = 0xFF000000 | color;
                for (size_t dy = 0; dy < scale; ++dy)
                {
                    for (size_t dx = 0; dx < scale; ++dx)
                    {
                        Gpu::put_pixel(ox + x * scale + dx, oy + y * scale + dy, argb);
                    }
                }
            }
        }
    }

    // Draws text using font8x8 scaled up (larger fonts lack most glyphs)
    size_t draw_text_scaled(const char* str, size_t x, size_t y, size_t scale, uint32_t color)
    {
        const size_t advance = (8 + Gpu::FONT_SPACING_W) * scale;

        for (const char* p = str; *p; ++p)
        {
            const unsigned char c = (unsigned char)*p;
            if (c < 32 || c > 126)
            {
                x += advance;
                continue;
            }

            for (size_t row = 0; row < 8; ++row)
            {
                const uint8_t bits = font8x8[c][row];
                for (size_t col = 0; col < 8; ++col)
                {
                    if (!(bits & (1U << (7 - col))))
                    {
                        continue;
                    }

                    const size_t px = x + col * scale;
                    const size_t py = y + row * scale;
                    for (size_t dy = 0; dy < scale; ++dy)
                    {
                        for (size_t dx = 0; dx < scale; ++dx)
                        {
                            Gpu::put_pixel(px + dx, py + dy, color);
                        }
                    }
                }
            }

            x += advance;
        }

        return x;
    }

    size_t text_width_scaled(const char* str, size_t scale)
    {
        size_t count = 0;
        for (const char* p = str; *p; ++p)
        {
            if (*p >= 32 && *p <= 126)
            {
                count++;
            }
        }
        return count * (8 + Gpu::FONT_SPACING_W) * scale;
    }

    void draw_progress_bar()
    {
        // Outer border
        Gpu::fill_block(bar_x, bar_y, COLOR_BUTTON_SHADOW, bar_w, bar_h);
        Gpu::draw_rect((int)bar_x, (int)bar_y,
                       (int)(bar_x + bar_w), (int)(bar_y + bar_h), COLOR_BORDER);

        // Inner track
        Gpu::fill_block(bar_x + 2, bar_y + 2, COLOR_NASUA_DARKER,
                        bar_w - 4, bar_h - 4);

        // Fill
        size_t fill_w = (bar_w - 4) * progress / 100;
        if (fill_w > 0)
        {
            Gpu::fill_block(bar_x + 2, bar_y + 2, COLOR_BLUE, fill_w, bar_h - 4);
        }

        // Percentage text
        char pct[8];
        itoa((int64_t)progress, pct);
        char label[16];
        strcpy(label, pct);
        strcat(label, "%");

        Gpu::print_at8(label, bar_x + bar_w + 16, bar_y + 3, COLOR_NASUA_LIGHT);
    }

    void draw_log_panel()
    {
        Gpu::fill_block(log_x, log_y, COLOR_NASUA_START_MENU_P, log_w, log_h);
        Gpu::draw_rect((int)log_x, (int)log_y,
                       (int)(log_x + log_w), (int)(log_y + log_h), COLOR_BORDER);

        Gpu::print_at8("boot log", log_x + 8, log_y + 6, COLOR_TERM_INFO);

        const size_t text_x = log_x + 8;
        const size_t text_top = log_y + 8 + log_line_h;
        size_t max_chars = (log_w - 16) / (8 + Gpu::FONT_SPACING_W);
        if (max_chars >= LOG_MESSAGE_SIZE)
        {
            max_chars = LOG_MESSAGE_SIZE - 1;
        }

        uint32_t visible = log_count < log_visible_max ? log_count : log_visible_max;
        uint32_t oldest = (log_next + LOG_MAX_LINES - visible) % LOG_MAX_LINES;

        for (uint32_t i = 0; i < visible; ++i)
        {
            uint32_t idx = (oldest + i) % LOG_MAX_LINES;
            char* line = log_lines[idx];

            if ((size_t)strlen(line) > max_chars)
            {
                line[max_chars] = '\0';
            }

            Gpu::print_at8(line, text_x, text_top + i * log_line_h, COLOR_NASUA_LIGHT);
        }
    }

    void render()
    {
        if (!active || !Gpu::fb)
        {
            return;
        }

        Gpu::clear_screen(COLOR_NASUA_BG);

        draw_logo();

        const char* title = "NasuaOS";
        const size_t title_w = text_width_scaled(title, TITLE_FONT_SCALE);
        draw_text_scaled(title, (Gpu::fb->width - title_w) / 2, title_y,
                         TITLE_FONT_SCALE, COLOR_NASUA_LIGHT);

        const char* subtitle = "loading system...";
        const size_t subtitle_w = strlen(subtitle) * (8 + Gpu::FONT_SPACING_W);
        Gpu::print_at8(subtitle, (Gpu::fb->width - subtitle_w) / 2,
                       title_y + 32, COLOR_GRAY);

        draw_progress_bar();
        draw_log_panel();

        Gpu::render_frame();
    }
}

namespace BootSplash
{
    bool is_active()
    {
        return active;
    }

    void init()
    {
        if (!Gpu::fb)
        {
            return;
        }

        active = true;
        progress = 0;
        log_count = 0;
        log_next = 0;

        for (uint32_t i = 0; i < LOG_MAX_LINES; ++i)
        {
            log_lines[i][0] = '\0';
        }

        compute_layout();
        render();
    }

    void set_progress(uint32_t percent)
    {
        if (!active)
        {
            return;
        }

        if (percent > 100)
        {
            percent = 100;
        }

        progress = percent;
        render();
    }

    void add_log(const char* subsystem, const char* message)
    {
        if (!active)
        {
            return;
        }

        char* line = log_lines[log_next];
        line[0] = '\0';

        if (subsystem && subsystem[0])
        {
            strcat(line, "[");
            strcat(line, subsystem);
            strcat(line, "] ");
        }

        if (message)
        {
            strcat(line, message);
        }

        if ((size_t)strlen(line) >= LOG_MESSAGE_SIZE)
        {
            line[LOG_MESSAGE_SIZE - 1] = '\0';
        }

        log_next = (log_next + 1) % LOG_MAX_LINES;
        if (log_count < LOG_MAX_LINES)
        {
            log_count++;
        }

        render();
    }

    void finish()
    {
        if (!active)
        {
            return;
        }

        set_progress(100);
        active = false;
    }
}
