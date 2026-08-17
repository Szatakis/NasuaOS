#include "icons.hpp"
#include "system/gui/gui.hpp"

#include "libs/libc/libc.hpp"

bool start_hover = false;

int icons_start_x = 0;
int icons_start_y = 0;
int icons_offset = 45;

typedef struct 
{
    int x1, y1;
    int x2, y2;
} ButtonArea;

int start_y = static_cast<int>(fb->height) - bar_h_scaled;  
bool is_menu_start_open = false;

ButtonArea start_button = { 0, 0, 0, 0 }; 

void draw_start_button(size_t x, size_t taskbar_y)
{
    constexpr int button_w = 100;
    const int button_h = 28 + (bar_h_scaled - bar_h);
    constexpr int margin = 2;

    const int button_y = taskbar_y + (bar_h_scaled - button_h) / 2;

    start_button.x1 = static_cast<int>(x);
    start_button.y1 = button_y;
    start_button.x2 = static_cast<int>(x + button_w);
    start_button.y2 = button_y + button_h;

    draw_rect(start_button.x1 - margin, start_button.y1, start_button.x2 + margin, start_button.y2, COLOR_SHADOW);

    draw_rect(start_button.x1, start_button.y1 + margin, start_button.x2, start_button.y2 - margin, COLOR_HARD_SHADOW);

    if(start_hover)
    {
        draw_icon<32, 32>(start_icon_hover, x, button_y + ((bar_h_scaled - bar_h) / 2));
    }
    else
    {
        draw_icon<32, 32>(start_icon, x, button_y + ((bar_h_scaled - bar_h) / 2));
    }

    print_at10("START", x + 38, button_y + ((button_h - 8) / 2), COLOR_WHITE);
}

void draw_start_menu_system_icons(int x, int y, int p, int w, int h) 
{
    (void)w;
    draw_icon<32, 32>(shutdown_icon, (x + (p - 32) / 2), (y + h - (32 * 2) - 16));
    draw_icon<32, 32>(reboot_icon, (x + (p - 32) / 2), (y + h - (32 * 2) + 24));

    icons_start_x = x;
    icons_start_y = y + 5;

    draw_icon<32, 32>(terminal_icon, icons_start_x + 50, icons_start_y);
    print_at8("Terminal", icons_start_x + 85, icons_start_y + 12, COLOR_WHITE);

    draw_icon<32, 32>(settings_icon, icons_start_x + 50, icons_start_y + icons_offset);
    print_at8("Settings", icons_start_x + 85, icons_start_y + 12 + icons_offset, COLOR_WHITE); // not work yet

    draw_icon<32, 32>(suaedit_icon, icons_start_x + 50, icons_start_y + 2 * icons_offset);
    print_at8("SuaEdit", icons_start_x + 85, icons_start_y + 12 + 2 * icons_offset, COLOR_WHITE); // not work yet

    draw_start_menu_napp_icons();
}

// Applications installed as .napp packages in the rootfs, listed below the
// kernel applications and refreshed every time the start menu is drawn.
char start_menu_napps[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
int start_menu_napp_count = 0;

void draw_start_menu_napp_icons()
{
    start_menu_napp_count = (int)napp_list(start_menu_napps, NAPP_MAX_APPLICATIONS);

    for (int i = 0; i < start_menu_napp_count; i++)
    {
        const int row = kernel_app_count + i;
        const int y = icons_start_y + row * icons_offset;

        const uint32_t* icon = strcmp(start_menu_napps[i], "calculator") == 0 ? calculator_icon : suaedit_icon;

        draw_icon<32, 32>(icon, icons_start_x + 50, y);
        print_at8(start_menu_napps[i], icons_start_x + 85, y + 12, COLOR_WHITE);
    }
}

bool is_mouse_over_start(int mouse_x, int mouse_y) 
{
    if (mouse_x >= start_button.x1 && mouse_x <= start_button.x2 && mouse_y >= start_button.y1 && mouse_y <= start_button.y2) 
    {
        return true;
    }
    return false;
}

bool is_mouse_over_icon(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x &&
           mx < x + w &&
           my >= y &&
           my < y + h;
}