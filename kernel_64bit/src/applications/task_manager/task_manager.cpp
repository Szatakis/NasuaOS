#include "task_manager.hpp"

void draw_task_manager(window_struct* win)
{
    (void)win;
}

window_struct task_manager = 
{
    .name = "Task Manager",
    .id = 0,

    .pos_x = 10,
    .pos_y = 10,
    .width = 500,
    .height = 350,

    .visible = true,
    .minimized = false,
    .focused = true,

    .resizable = true,
    .can_maximize = true,
    .maximized = false,

    .restore_pos_x = 0,
    .restore_pos_y = 0,
    .restore_width = 0,
    .restore_height = 0,

    .is_dragging = false,
    .drag_offset_x = 0,
    .drag_offset_y = 0,

    .is_resizing = false,
    .resize_right = false,
    .resize_bottom = false,
    .resize_start_mouse_x = 500,
    .resize_start_mouse_y = 350,
    .resize_start_width = 10,
    .resize_start_height = 20,
    .max_width = 0,
    .max_height = 0,

    .userdata = nullptr,
    .draw_content = draw_task_manager,
    .key_press = nullptr,
    .mouse_click = nullptr
};