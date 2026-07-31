#include "gui.hpp"

#include "./icons/icons.hpp"
#include "system/drivers/video/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/drivers/keyboard/driver.hpp"
#include "system/drivers/mouse/driver.hpp"
#include "./vars/colors.hpp"

extern limine_framebuffer* fb;

bool last_start_hover = false;

const uint32_t menu_w = 300;
const uint32_t menu_h = 400;
uint32_t menu_x = 0;
uint32_t menu_y = 0;
const int panel_width = 45;

extern bool start_hover;
extern bool is_menu_start_open;
extern void draw_rect(int x1, int y1, int x2, int y2, uint32_t color);
extern bool is_mouse_over_start(int mouse_x, int mouse_y);
extern void draw_start_menu_system_icons(int x, int y);

void draw_start_menu_f(int x, int y, int w, int h) 
{
    draw_rect(x, y, x + w, y + h, COLOR_NASUA_START_MENU);
    draw_rect(x, y, x + panel_width, y + h, COLOR_NASUA_START_MENU_P);

    draw_start_menu_system_icons(x, y, panel_width, w, h);
}

void draw_start_menu() 
{
    if (!fb || !is_menu_start_open) 
    {
        return;
    }

    menu_x = 0;
    menu_y = fb->height - bar_h_scaled - menu_h;

    draw_start_menu_f(menu_x, menu_y, menu_w, menu_h);
}

void open_start_menu() 
{
    if (!fb) 
    {
        return;
    }
    is_menu_start_open = true;
}

void close_start_menu() 
{
    is_menu_start_open = false;
}

void update_gui_state(int mouse_x, int mouse_y) 
{
    if (is_mouse_over_start(mouse_x, mouse_y)) 
    {
        start_hover = true;
    } 
    else 
    {
        start_hover = false;
    }
}

void update_windows_gui() 
{
    update_windows_positions(mouse_x, mouse_y);
    draw_windows(); 
}