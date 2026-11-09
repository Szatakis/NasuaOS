#include "gui.hpp"

#include "./icons/icons.hpp"
#include "drivers/gpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/keyboard/driver.hpp"
#include "drivers/mouse/driver.hpp"
#include "./vars/colors.hpp"

bool last_start_hover = false;

const int panel_width = 156;
static constexpr uint32_t START_MENU_MAX_WIDTH = 620;
static constexpr uint32_t START_MENU_MAX_HEIGHT = 400;
uint32_t menu_w = START_MENU_MAX_WIDTH;
uint32_t menu_h = START_MENU_MAX_HEIGHT;
uint32_t menu_x = 0;
uint32_t menu_y = 0;

extern bool start_hover;

static uint32_t start_menu_width()
{
    const uint32_t responsive_width = Gpu::fb->width / 2;
    return responsive_width < START_MENU_MAX_WIDTH
        ? responsive_width
        : START_MENU_MAX_WIDTH;
}

static void update_start_menu_size()
{
    menu_w = start_menu_width();
    const uint32_t available_height = Gpu::fb->height > bar_h_scaled
        ? Gpu::fb->height - bar_h_scaled
        : 0;
    menu_h = available_height < START_MENU_MAX_HEIGHT
        ? available_height
        : START_MENU_MAX_HEIGHT;
    menu_x = 0;
    menu_y = available_height - menu_h;
}

void draw_start_menu_f(int x, int y, int w, int h)
{
    Gpu::draw_rect(x, y, x + w, y + h, COLOR_NASUA_START_MENU);
    Gpu::draw_rect(x, y, x + panel_width, y + h, COLOR_NASUA_START_MENU_P);

    draw_start_menu_contents(x, y, w, h);
}

void draw_start_menu() 
{
    if (!Gpu::fb || !menu_start_open) 
    {
        return;
    }

    update_start_menu_size();

    draw_start_menu_f(menu_x, menu_y, menu_w, menu_h);
}

void open_start_menu() 
{
    if (!Gpu::fb) 
    {
        return;
    }

    update_start_menu_size();
    menu_start_open = true;
    reset_start_menu_navigation();
}

void close_start_menu() 
{
    menu_start_open = false;
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
    Gpu::Window_Manager::update_windows_positions(Mouse::x, Mouse::y);
    Gpu::Window_Manager::draw_windows(); 
}