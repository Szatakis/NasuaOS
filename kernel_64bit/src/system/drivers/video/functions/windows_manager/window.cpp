#include "window.hpp"

#include "system/drivers/video/driver.hpp"

#include "system/gui/vars/colors.hpp"

#define MAX_WINDOWS 13

window_struct* active_window = nullptr;

static window_struct* window_list[MAX_WINDOWS];
static int window_count = 0;

void register_window(window_struct* window) 
{
    if (window_count >= MAX_WINDOWS) return;
    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i] == window) 
        {
            return;
        }
    }
    window->is_dragging = false; // Init safty flag
    window_list[window_count++] = window;

    active_window = window;
    window->focused = true;
}

void unregister_window(window_struct* window) 
{
    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i] == window) 
        {
            for (int j = i; j < window_count - 1; j++) 
            {
                window_list[j] = window_list[j + 1];
            }
            window_count--;
            return;
        }
    }
}

// Helpers
void bring_window_to_front(int index)
{
    if(index < 0 || index >= window_count)
        return;

    window_struct* target = window_list[index];

    for(int i = index; i < window_count - 1; i++)
    {
        window_list[i] = window_list[i+1];
    }

    window_list[window_count-1] = target;

    active_window = target;

    for(int i = 0; i < window_count; i++)
    {
        window_list[i]->focused = false;
    }

    target->focused = true;
}

void send_key_to_window(char key)
{
    if(active_window == nullptr)
        return;

    if(active_window->key_press)
    {
        active_window->key_press(active_window, key);
    }
}

// Updates windows positions
void update_windows_positions(int current_mouse_x, int current_mouse_y) 
{
    for (int i = 0; i < window_count; i++) 
    {
        window_struct* win = window_list[i];
        if (win->visible && win->is_dragging) 
        {
            win->pos_x = current_mouse_x - win->drag_offset_x;
            win->pos_y = current_mouse_y - win->drag_offset_y;
            
            // Taskbar protection
            if (fb) 
            {
                if (win->pos_y > (int)fb->height - 50) 
                {
                    win->pos_y = (int)fb->height - 50;
                }
            }
        }
    }
}

// Windows clicks
void handle_window_mouse_click(int mouse_x, int mouse_y) 
{
    for (int i = window_count - 1; i >= 0; i--) 
    {
        if (window_list[i]->visible && window_list[i]->is_dragging) 
        {
            window_list[i]->is_dragging = false;
            return;
        }
    }

    for (int i = window_count - 1; i >= 0; i--) 
    {
        window_struct* win = window_list[i];
        if (!win->visible) 
        {
            continue;
        }

        window_button btn = get_window_button(win, mouse_x, mouse_y);

        if (btn == BUTTON_CLOSE) 
        {
            win->visible = false;
            unregister_window(win);
            return;
        }
        else if (btn == BUTTON_MINIMIZE) 
        {
            win->visible = false;
            return;
        }
        else if (btn == BUTTON_MAXIMIZE) 
        {
            return;
        }
        
        if (is_mouse_over_window_title(win, mouse_x, mouse_y)) 
        {
            bring_window_to_front(i);
            win->is_dragging = true;
            win->drag_offset_x = mouse_x - win->pos_x;
            win->drag_offset_y = mouse_y - win->pos_y;
            return;
        }

        if (is_mouse_over_window(win, mouse_x, mouse_y)) 
        {

            bring_window_to_front(i);


            if(win->mouse_click)
            {
                win->mouse_click(
                    win,
                    mouse_x,
                    mouse_y
                );
            }

            return;
        }
    }
}

void draw_windows() 
{
    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i]->visible) 
        {
            draw_window(window_list[i]);
        }
    }
}

void draw_window(window_struct* window) 
{
    if (!window || !window->visible || !fb) return;
    if (window->pos_x >= (int)fb->width || window->pos_y >= (int)fb->height) return;
    if (window->pos_x + window->width <= 0 || window->pos_y + window->height <= 0) return;

    int title_height = window->height / 10;
    if (title_height < 18) title_height = 18; 

    if (window->pos_x >= 0 && window->pos_y >= 0) 
    {
        fill_block(window->pos_x, window->pos_y, COLOR_WINDOW, window->width, window->height);
    } 
    else 
    {
        int draw_x = window->pos_x < 0 ? 0 : window->pos_x;
        int draw_y = window->pos_y < 0 ? 0 : window->pos_y;
        int draw_w = window->pos_x < 0 ? window->width + window->pos_x : window->width;
        int draw_h = window->pos_y < 0 ? window->height + window->pos_y : window->height;
        if (draw_w > 0 && draw_h > 0) fill_block(draw_x, draw_y, COLOR_WINDOW, draw_w, draw_h);
    }

    int title_draw_x = window->pos_x < 0 ? 0 : window->pos_x;
    int title_draw_y = window->pos_y < 0 ? 0 : window->pos_y;
    int title_draw_w = window->pos_x < 0 ? window->width + window->pos_x : window->width;
    int title_draw_h = window->pos_y < 0 ? title_height + window->pos_y : title_height;
    if (title_draw_w > 0 && title_draw_h > 0) fill_block(title_draw_x, title_draw_y, COLOR_TITLEBAR, title_draw_w, title_draw_h);

    if (window->pos_x + 10 < (int)fb->width && window->pos_y + (title_height - 8) / 2 < (int)fb->height) 
    {
        print_at8(window->name, window->pos_x + 10, window->pos_y + (title_height - 8) / 2, COLOR_WHITE);
    }

    int button_x = window->pos_x + window->width - 55;
    int button_y = window->pos_y + (title_height - 8) / 2;

    if (button_x < (int)fb->width && button_y < (int)fb->height) 
    {
        print_at8("-", button_x, button_y, COLOR_WHITE);
        print_at8("[]", button_x + 16, button_y, COLOR_WHITE);
        print_at8("X", button_x + 42, button_y, COLOR_WHITE);
    }

    // Draw window
    if (window->draw_content) 
    {
        window->draw_content(window);
    }
}

bool is_mouse_over_window(window_struct* window, int mouse_x, int mouse_y) 
{
    if (!window || !window->visible) 
    {
        return false;
    }

    return (mouse_x >= window->pos_x && mouse_x < window->pos_x + window->width && mouse_y >= window->pos_y && mouse_y < window->pos_y + window->height);
}

bool is_mouse_over_window_title(window_struct* window, int mouse_x, int mouse_y) 
{
    if (!window || !window->visible) return false;
    int title_height = window->height / 10;
    if (title_height < 18) title_height = 18;
    return (mouse_x >= window->pos_x && mouse_x < window->pos_x + window->width &&
            mouse_y >= window->pos_y && mouse_y < window->pos_y + title_height);
}

window_button get_window_button(window_struct* window, int mouse_x, int mouse_y) 
{
    if (!window || !window->visible) return BUTTON_NONE;
    int title_height = window->height / 10;
    if (title_height < 18) title_height = 18;

    if (mouse_y >= window->pos_y && mouse_y < window->pos_y + title_height) 
    {
        int minimize_start = window->pos_x + window->width - 58; 
        int rel_x = mouse_x - minimize_start;

        if (rel_x >= 0 && rel_x < 14) 
        {
            return BUTTON_MINIMIZE;
        }
        if (rel_x >= 14 && rel_x < 36) 
        {
            return BUTTON_MAXIMIZE;
        }
        if (rel_x >= 36 && rel_x < 56) 
        {
            return BUTTON_CLOSE;
        }
    }
    return BUTTON_NONE;
}

// Returns true if mouse is over any window
bool is_mouse_over_any_window(int mouse_x, int mouse_y) 
{
    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i]->visible && is_mouse_over_window(window_list[i], mouse_x, mouse_y)) 
        {
            return true;
        }
    }
    return false;
}

