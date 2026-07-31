#include "settings.hpp"

void draw_settings(window_struct* win)
{
    (void)win;
}

window_struct settings = 
{
    .name = "Settings",
    .id = 0,

    .pos_x = 10,
    .pos_y = 10,
    .width = 500,
    .height = 350,

    .visible = true,
    .focused = true,

    .is_dragging = false,
    .drag_offset_x = 0,
    .drag_offset_y = 0,

    .userdata = nullptr,
    .draw_content = draw_settings,
    .key_press = nullptr,
    .mouse_click = nullptr
};