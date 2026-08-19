#pragma once
#include <cstddef>
#include <cstdint>

#include "system/applications/napp/napp.hpp"
#include "system/drivers/gpu/driver.hpp"
#include "../vars/colors.hpp"

// Applications built into the kernel, shown at the top of the start menu.
inline constexpr int kernel_app_count = 3;

// Taskbar Icons
extern const uint32_t start_icon[1024];
extern const uint32_t start_icon_hover[1024];
extern const uint32_t notification_icon[1024];
extern const uint32_t notification_icon_h[1024];
extern const uint32_t speaker_icon[1024];

// Start Menu Icons
extern const uint32_t shutdown_icon[1024];
extern const uint32_t reboot_icon[1024];

// Apps
extern const uint32_t terminal_icon[1024];
extern const uint32_t calculator_icon[1024];
extern const uint32_t settings_icon[1024];
extern const uint32_t suaedit_icon[1024];


extern int icons_start_x;
extern int icons_start_y;
extern int icons_offset;

extern bool menu_start_open;
extern bool start_hover;

void draw_start_button(size_t x, size_t y);
bool is_mouse_over_start(int mouse_x, int mouse_y);

void draw_start_menu_system_icons(int x, int y, int p, int w, int h);
void draw_start_menu_napp_icons();

extern char start_menu_napps[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
extern int start_menu_napp_count;

bool is_mouse_over_icon(int mx, int my, int x, int y, int w, int h);

template <size_t width, size_t height>
void draw_icon(const uint32_t* icon, size_t start_x, size_t start_y)
{
    if (!fb) return;

    uint32_t* bb_ptr = get_backbuffer();
    size_t pitch = get_backbuffer_pitch();

    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            size_t px = start_x + x;
            size_t py = start_y + y;

            if (px >= fb->width || py >= fb->height)
                continue;

            uint32_t color = icon[y * width + x];

            if (color == 0x000000)
                continue;

            bb_ptr[py * pitch + px] = 0xFF000000 | color;
        }
    }
}