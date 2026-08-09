#include "terminal.hpp"

#include "../shell/commands.hpp"
#include "libs/libc/libc.h"

struct terminal_state 
{
    char input[256];
    int cursor;

    char output[64][128];
    int output_count;
};

terminal_state terminal_data;

void terminal_key(window_struct* win, char key) 
{
    terminal_state* term = (terminal_state*)win->userdata;

    if (key == '\n') 
    {
        if(!is_empty_or_whitespace(term->input)) {
            execute_command(term->input);
        }

        term->cursor = 0;
        term->input[0] = '\0';
        return;
    }

    if (key == '\b') 
    {
        if (term->cursor > 0) 
        {
            term->cursor--;
            term->input[term->cursor] = '\0';
        }
        return;
    }

    term->input[term->cursor++] = key;
    term->input[term->cursor] = '\0';
}

void draw_terminal(window_struct* win) 
{
    terminal_state* term = (terminal_state*)win->userdata;

    int title = win->height / 10;
    if (title < 18) 
    {
        title = 18;
    }

    fill_block(win->pos_x + 2, win->pos_y + title + 2, COLOR_BLACK, win->width - 4, win->height - title - 4);

    print_at8("NasuaOS Terminal", win->pos_x + 8, win->pos_y + title + 8, COLOR_GREEN);
    print_at8(term->input, win->pos_x + 8, win->pos_y + title + 24, COLOR_WHITE);
}

window_struct terminal = 
{
    .name = "Terminal",
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

    .userdata = &terminal_data,
    .draw_content = draw_terminal,
    .key_press = terminal_key,
    .mouse_click = nullptr
};