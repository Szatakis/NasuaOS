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
    .focused = true,

    .is_dragging = false,
    .drag_offset_x = 0,
    .drag_offset_y = 0,

    .userdata = nullptr,
    .draw_content = draw_task_manager,
    .key_press = nullptr,
    .mouse_click = nullptr
};