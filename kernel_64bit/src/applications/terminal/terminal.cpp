#include "terminal.hpp"

#include "../shell/commands.hpp"
#include "libs/libc/libc.hpp"

#define TERM_MAX_ROWS 64
#define TERM_MAX_COLS 256

bool active_terminal_redirect = false;

struct terminal_state 
{
    char input[256];
    int cursor;

    char output[TERM_MAX_ROWS][TERM_MAX_COLS];
    int output_count;
    int col_cursor;
};

terminal_state terminal_data = {
    .input = {0},
    .cursor = 0,
    .output = {{0}},
    .output_count = 0,
    .col_cursor = 0
};

void terminal_clear_output() 
{
    for (int i = 0; i < TERM_MAX_ROWS; i++) 
    {
        terminal_data.output[i][0] = '\0';
    }
    terminal_data.output_count = 1;
    terminal_data.col_cursor = 0;

    active_terminal_redirect = true;
    print_cmd();
    active_terminal_redirect = false;
}

void terminal_write_char(char c) 
{
    if (terminal_data.output_count == 0) 
    {
        terminal_data.output_count = 1;
        terminal_data.col_cursor = 0;
        terminal_data.output[0][0] = '\0';
    }

    if (c == '\n') 
    {
        if (terminal_data.output_count < TERM_MAX_ROWS) 
        {
            terminal_data.output[terminal_data.output_count][0] = '\0';
            terminal_data.output_count++;
            terminal_data.col_cursor = 0;
        } 
        else 
        {
            for (int i = 0; i < TERM_MAX_ROWS - 1; i++) 
            {
                strcpy(terminal_data.output[i], terminal_data.output[i + 1]);
            }
            terminal_data.output[TERM_MAX_ROWS - 1][0] = '\0';
            terminal_data.col_cursor = 0;
        }
        return;
    }

    if (c == '\r') 
    {
        terminal_data.col_cursor = 0;
        return;
    }

    int line = terminal_data.output_count - 1;
    if (terminal_data.col_cursor < TERM_MAX_COLS - 1) 
    {
        terminal_data.output[line][terminal_data.col_cursor++] = c;
        terminal_data.output[line][terminal_data.col_cursor] = '\0';
    } 
    else 
    {
        terminal_write_char('\n');
        line = terminal_data.output_count - 1;
        terminal_data.output[line][terminal_data.col_cursor++] = c;
        terminal_data.output[line][terminal_data.col_cursor] = '\0';
    }
}

void terminal_write_string(const char* str) 
{
    while (*str) 
    {
        terminal_write_char(*str++);
    }
}

void terminal_key(window_struct* win, char key) 
{
    terminal_state* term = (terminal_state*)win->userdata;

    if (key == '\n') 
    {
        if (!is_empty_or_whitespace(term->input)) 
        {
            terminal_write_string(term->input);
            terminal_write_char('\n');

            active_terminal_redirect = true;
            execute_command(term->input);
            active_terminal_redirect = false;
        } 
        else 
        {
            terminal_write_char('\n');
            active_terminal_redirect = true;
            print_cmd();
            active_terminal_redirect = false;
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

    if (term->cursor < 255) 
    {
        term->input[term->cursor++] = key;
        term->input[term->cursor] = '\0';
    }
}

void draw_terminal(window_struct* win) 
{
    terminal_state* term = (terminal_state*)win->userdata;

    if (term->output_count == 0 || term->output[0][0] == '\0') 
    {
        terminal_clear_output();
    }

    int title = win->height / 10;
    if (title < 18) 
    {
        title = 18;
    }

    int win_inner_w = win->width - 4;
    int win_inner_h = win->height - title - 4;

    int start_x = win->pos_x + 8;
    int start_y = win->pos_y + 30;
    print_at8("NasuaOS Terminal", start_x, start_y, COLOR_TERM_INFO);

    int char_w = 8 + FONT_SPACING_W;
    int line_height = 12;
    int content_start_y = start_y + 16;
    int available_height = win_inner_h - 22;
    int max_visible_lines = available_height / line_height;
    if (max_visible_lines < 1) 
    {
        max_visible_lines = 1;
    }

    int max_cols = (win_inner_w - 12) / char_w;
    if (max_cols < 10) 
    {
        max_cols = 10;
    }

    // Step 1: Count total visual wrapped rows
    int total_visual_rows = 0;
    for (int i = 0; i < term->output_count; i++) 
    {
        char temp_line[512];
        if (i == term->output_count - 1) 
        {
            strcpy(temp_line, term->output[i]);
            strcat(temp_line, term->input);
        } 
        else 
        {
            strcpy(temp_line, term->output[i]);
        }

        int len = strlen(temp_line);
        if (len == 0) 
        {
            total_visual_rows += 1;
        } 
        else 
        {
            total_visual_rows += (len + max_cols - 1) / max_cols;
        }
    }

    // Step 2: Determine starting visual row index to render
    int start_visual_row = 0;
    if (total_visual_rows > max_visible_lines) 
    {
        start_visual_row = total_visual_rows - max_visible_lines;
    }

    // Step 3: Render visual rows
    int current_visual_row = 0;
    int curr_y = content_start_y;

    for (int i = 0; i < term->output_count; i++) 
    {
        char temp_line[512];
        bool is_last_line = (i == term->output_count - 1);
        if (is_last_line) 
        {
            strcpy(temp_line, term->output[i]);
            strcat(temp_line, term->input);
        } 
        else 
        {
            strcpy(temp_line, term->output[i]);
        }

        int len = strlen(temp_line);
        if (len == 0) 
        {
            if (current_visual_row >= start_visual_row) 
            {
                curr_y += line_height;
            }
            current_visual_row++;
        } 
        else 
        {
            int offset = 0;
            while (offset < len) 
            {
                int chunk_len = len - offset;
                if (chunk_len > max_cols) 
                {
                    chunk_len = max_cols;
                }

                if (current_visual_row >= start_visual_row) 
                {
                    char chunk[256];
                    for (int j = 0; j < chunk_len; j++) 
                    {
                        chunk[j] = temp_line[offset + j];
                    }
                    chunk[chunk_len] = '\0';

                    print_at8(chunk, start_x, curr_y, COLOR_WHITE);

                    if (is_last_line && (offset + chunk_len >= len)) 
                    {
                        int cursor_x_pos = start_x + chunk_len * char_w;
                        print_at8("_", cursor_x_pos, curr_y, COLOR_GRAY);
                    }

                    curr_y += line_height;
                }

                offset += chunk_len;
                current_visual_row++;
            }
        }
    }
}

window_struct terminal = 
{
    .name = "Terminal",
    .id = 0,

    .pos_x = 10,
    .pos_y = 10,
    .width = 500,
    .height = 350,

    .visible = false,
    .minimized = false,
    .focused = false,

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

    .userdata = &terminal_data,
    .draw_content = draw_terminal,
    .key_press = terminal_key,
    .mouse_click = nullptr
};