#include "window.hpp"

#include "system/drivers/gpu/driver.hpp"
#include "system/gui/gui.hpp"

#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"

#include "libs/libc/libc.hpp"

#define MAX_WINDOWS 13
#define MAX_TASKBAR_WINDOWS 5

window_struct* active_window = nullptr;

static window_struct* window_list[MAX_WINDOWS];
static int window_count = 0;

static int find_window_index(window_struct* window)
{
    for (int i = 0; i < window_count; i++)
    {
        if (window_list[i] == window)
        {
            return i;
        }
    }

    return -1;
}

void register_window(window_struct* window) 
{
    if (!window || window_count >= MAX_WINDOWS) return;
    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i] == window) 
        {
            return;
        }
    }

    window->is_dragging = false;

    window->is_resizing = false;
    window->resize_right = false;
    window->resize_bottom = false;

    window->minimized = false;

    window_list[window_count++] = window;

    active_window = window;
    window->focused = true;
}

void unregister_window(window_struct* window) 
{
    if (!window) 
    {
        return;
    }

    for (int i = 0; i < window_count; i++) 
    {
        if (window_list[i] == window) 
        {
            for (int j = i; j < window_count - 1; j++) 
            {
                window_list[j] = window_list[j + 1];
            }
            window_count--;
            if (window_count > 0)
            {
                active_window = window_list[window_count - 1];
            }
            else
            {
                active_window = nullptr;
            }
            return;
        }
    }
}

void minimize_window(window_struct* window)
{
    if (!window || find_window_index(window) < 0) return;

    window->visible = false;
    window->minimized = true;
    window->focused = false;

    if (active_window == window)
    {
        active_window = nullptr;
    }
}

// Helpers
void bring_window_to_front(int index)
{
    if(index < 0 || index >= window_count)
    {
        return;
    }

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

void restore_window(window_struct* window)
{
    if (!window)
    {
        return;
    }

    int index = find_window_index(window);
    if (index < 0) return;

    window->visible = true;
    window->minimized = false;
    window->focused = true;
    bring_window_to_front(index);
}

void maximize_window(window_struct* window)
{
    if (!window || !window->can_maximize || find_window_index(window) < 0)
    {
        return;
    }

    if (window->maximized)
    {
        unmaximize_window(window);
        return;
    }

    if (!fb)
    {
        return;
    }

    window->restore_pos_x = window->pos_x;
    window->restore_pos_y = window->pos_y;
    window->restore_width = window->width;
    window->restore_height = window->height;

    window->pos_x = 0;
    window->pos_y = 0;
    window->width = (int)fb->width;
    window->height = (int)fb->height - (int)bar_h_scaled;
    window->maximized = true;
    window->visible = true;
    window->minimized = false;
    window->focused = true;
}

void unmaximize_window(window_struct* window)
{
    if (!window || !window->maximized || !window->can_maximize)
    {
        return;
    }

    window->pos_x = window->restore_pos_x;
    window->pos_y = window->restore_pos_y;
    window->width = window->restore_width;
    window->height = window->restore_height;
    window->maximized = false;
    window->visible = true;
    window->minimized = false;
    window->focused = true;
}

static bool get_resize_area(window_struct* window, int mouse_x, int mouse_y, bool* resize_right, bool* resize_bottom) 
{
    if (!window || !window->visible || !window->resizable || window->maximized) 
    {
        return false;
    }

    int right = window->pos_x + window->width;
    int bottom = window->pos_y + window->height;

    bool on_right = mouse_x >= right - WINDOW_RESIZE_BORDER && mouse_x < right && mouse_y >= window->pos_y && mouse_y < bottom;
    bool on_bottom = mouse_y >= bottom - WINDOW_RESIZE_BORDER && mouse_y < bottom && mouse_x >= window->pos_x && mouse_x < right;

    // BOTTOM RIGHT ORNER
    bool on_corner = mouse_x >= right - WINDOW_RESIZE_BORDER && mouse_x < right && mouse_y >= bottom - WINDOW_RESIZE_BORDER && mouse_y < bottom;

    if (on_corner)
    {
        *resize_right = true;
        *resize_bottom = true;

        return true;
    }


    // RIGHT EDGE
    if (on_right)
    {
        *resize_right = true;
        *resize_bottom = false;

        return true;
    }


    // BOTTOM EDGE
    if (on_bottom)
    {
        *resize_right = false;
        *resize_bottom = true;

        return true;
    }

    return false;
}

void send_key_to_window(char key)
{
    if(active_window == nullptr)
    {
        return;
    }

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


        if (!win || !win->visible || win->maximized) 
        {
            continue;
        }


        // WINDOW DRAGGING
        if (win->is_dragging) 
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

            continue;
        }


        // WINDOW RESIZING
        if (win->is_resizing) 
        {
            int delta_x = current_mouse_x - win->resize_start_mouse_x;
            int delta_y = current_mouse_y - win->resize_start_mouse_y;


            // Resize right
            if (win->resize_right) 
            {
                int new_width = win->resize_start_width + delta_x;

                if (new_width < WINDOW_MIN_WIDTH) 
                {
                    new_width = WINDOW_MIN_WIDTH;
                }

                win->width = new_width;
            }

            // Resize bottom
            if (win->resize_bottom) 
            {
                int new_height = win->resize_start_height + delta_y;

                if (new_height < WINDOW_MIN_HEIGHT) 
                {
                    new_height = WINDOW_MIN_HEIGHT;
                }

                win->height = new_height;
            }

            // Don't resize outside screen
            if (fb) 
            {
                if (win->pos_x + win->width > (int)fb->width) 
                {
                    win->width = (int)fb->width - win->pos_x;
                }

                if (win->pos_y + win->height > (int)fb->height - (int)bar_h_scaled) 
                {
                    win->height = (int)fb->height - (int)bar_h_scaled - win->pos_y;
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
        window_struct* win = window_list[i];

        if (!win || !win->visible) 
        {
            continue;
        }


        if (win->is_dragging) 
        {
            win->is_dragging = false;

            return;
        }


        if (win->is_resizing) 
        {
            win->is_resizing = false;

            win->resize_right = false;
            win->resize_bottom = false;

            return;
        }
    }

    window_struct* taskbar = get_taskbar_window_at(mouse_x, mouse_y);
    if (taskbar != nullptr)
    {
        if (taskbar->minimized)
        {
            restore_window(taskbar);
        }
        else if (taskbar->visible)
        {
            if (active_window == taskbar)
            {
                minimize_window(taskbar);
            }
            else
            {
                bring_window_to_front(find_window_index(taskbar));
            }
        }
        return;
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
            win->minimized = false;
            unregister_window(win);
            return;
        }
        else if (btn == BUTTON_MINIMIZE) 
        {
            minimize_window(win);
            return;
        }
        else if (btn == BUTTON_MAXIMIZE) 
        {
            if (win->can_maximize)
            {
                bring_window_to_front(i);
                if (win->maximized)
                {
                    unmaximize_window(win);
                }
                else
                {
                    maximize_window(win);
                }
            }
            return;
        }

        // RESIZE
        if (win->resizable && !win->maximized)
        {
            bool resize_right = false;
            bool resize_bottom = false;


            if (get_resize_area(win, mouse_x, mouse_y, &resize_right, &resize_bottom))
            {
                bring_window_to_front(i);

                win->is_resizing = true;
                win->resize_right = resize_right;
                win->resize_bottom = resize_bottom;
                win->resize_start_mouse_x = mouse_x;
                win->resize_start_mouse_y = mouse_y;
                win->resize_start_width = win->width;
                win->resize_start_height = win->height;

                return;
            }
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
                win->mouse_click(win, mouse_x, mouse_y);
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

static void draw_app_icon_16(const char* name, int x, int y)
{
    if (!name || !fb) return;

    const uint32_t* icon_data = nullptr;
    if (strcmp(name, "Terminal") == 0)
    {
        icon_data = terminal_icon;
    }
    else if (strcmp(name, "Settings") == 0)
    {
        icon_data = settings_icon;
    }
    else if (strcmp(name, "SuaEdit") == 0)
    {
        icon_data = suaedit_icon;
    }
    else if (strcmp(name, "Calculator") == 0)
    {
        icon_data = calculator_icon;
    }

    if (!icon_data)
    {
        return;
    }

    uint32_t* bb_ptr = get_backbuffer();
    if (!bb_ptr) return;
    size_t pitch = get_backbuffer_pitch();

    for (size_t iy = 0; iy < 16; iy++)
    {
        for (size_t ix = 0; ix < 16; ix++)
        {
            size_t px = x + ix;
            size_t py = y + iy;

            if (px >= fb->width || py >= fb->height)
                continue;

            uint32_t color = icon_data[(iy * 2) * 32 + (ix * 2)];

            if (color == 0x000000)
                continue;

            bb_ptr[py * pitch + px] = color;
        }
    }
}

void draw_taskbar_entries()
{
    if (!fb || window_count <= 0)
    {
        return;
    }

    const size_t taskbar_y = fb->height - bar_h_scaled;
    const size_t button_h = bar_h_scaled - 8;
    const int button_y = (int)taskbar_y + ((int)bar_h_scaled - (int)button_h) / 2;
    const size_t button_w = 120;
    const size_t button_spacing = 6;
    size_t x = 124;

    int draw_count = window_count;
    if (draw_count > MAX_TASKBAR_WINDOWS)
    {
        draw_count = MAX_TASKBAR_WINDOWS;
    }

    for (int i = 0; i < draw_count; i++)
    {
        window_struct* win = window_list[i];
        if (!win || !win->name) {
            continue;
        }

        if (!win->visible && !win->minimized)
        {
            continue;
        }

        if (x + button_w > fb->width - 170)
        {
            break;
        }

        uint32_t bg;
        uint32_t border;
        uint32_t fg;

        bool is_minimized = win->minimized || !win->visible;
        bool is_active = (active_window == win) && !is_minimized;

        if (is_minimized)
        {
            bg = 0x000000; // Pitch black background when minimized
            border = 0x111620;
            fg = 0x4A5568; // Muted dark text
        }
        else if (is_active)
        {
            bg = 0x1E2838; // Active window background
            border = 0x4A5568;
            fg = COLOR_WHITE;
        }
        else
        {
            bg = 0x0F141D; // Normal visible window background
            border = 0x222A38;
            fg = 0xCBD5E0;
        }

        fill_block(x, button_y, bg, button_w, button_h);
        draw_rect(x, button_y, x + button_w, button_y + button_h, border);

        if (is_active)
        {
            // Bottom indicator line in gray
            fill_block(x + 2, button_y + button_h - 2, 0x808C9C, button_w - 4, 2);
        }

        int icon_x = (int)x + 8;
        int icon_y = button_y + ((int)button_h - 16) / 2;
        draw_app_icon_16(win->name, icon_x, icon_y);

        int text_x = (int)x + 28;
        int text_y = button_y + ((int)button_h - 8) / 2;
        print_at8(win->name, text_x, text_y, fg);

        x += button_w + button_spacing;
    }
}

bool is_mouse_over_taskbar(int mouse_x, int mouse_y)
{
    (void)mouse_x;

    if (!fb || !window_count)
    {
        return false;
    }

    size_t taskbar_y = fb->height - bar_h_scaled;
    size_t taskbar_h = bar_h_scaled;

    if (mouse_y < (int)taskbar_y || mouse_y >= (int)fb->height)
    {
        return false;
    }

    return mouse_y >= (int)taskbar_y && mouse_y < (int)(taskbar_y + taskbar_h);
}

window_struct* get_taskbar_window_at(int mouse_x, int mouse_y)
{
    if (!fb || !is_mouse_over_taskbar(mouse_x, mouse_y))
    {
        return nullptr;
    }

    const size_t taskbar_y = fb->height - bar_h_scaled;
    const size_t button_h = bar_h_scaled - 8;
    const int button_y = (int)taskbar_y + ((int)bar_h_scaled - (int)button_h) / 2;
    const size_t button_w = 120;
    const size_t button_spacing = 6;
    size_t x = 124;

    int taskbar_slots = window_count;
    if (taskbar_slots > MAX_TASKBAR_WINDOWS)
    {
        taskbar_slots = MAX_TASKBAR_WINDOWS;
    }

    for (int i = 0; i < taskbar_slots; i++)
    {
        window_struct* win = window_list[i];
        if (!win || !win->name || (!win->visible && !win->minimized))
        {
            continue;
        }

        if (mouse_x >= (int)x && mouse_x < (int)(x + button_w) && mouse_y >= button_y && mouse_y < (button_y + (int)button_h))
        {
            return win;
        }
        x += button_w + button_spacing;
    }

    return nullptr;
}

void draw_window(window_struct* window) 
{
    if (!window || !window->visible || !fb)
    {
        return;
    }

    if (window->pos_x >= (int)fb->width || window->pos_y >= (int)fb->height)
    {
        return;
    }

    if (window->pos_x + window->width <= 0 || window->pos_y + window->height <= 0)
    {
        return;
    }

    int title_height = WINDOW_TITLEBAR_HEIGHT;

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

        if (draw_w > 0 && draw_h > 0)
        {
            fill_block(draw_x, draw_y, COLOR_WINDOW, draw_w, draw_h);
        }
    }

    int title_draw_x = window->pos_x < 0 ? 0 : window->pos_x;
    int title_draw_y = window->pos_y < 0 ? 0 : window->pos_y;
    int title_draw_w = window->pos_x < 0 ? window->width + window->pos_x : window->width;
    int title_draw_h = window->pos_y < 0 ? title_height + window->pos_y : title_height;

    if (title_draw_w > 0 && title_draw_h > 0) 
    {
        fill_block(title_draw_x, title_draw_y, COLOR_TITLEBAR, title_draw_w, title_draw_h);
    }

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
    if (!window || !window->visible)
    {
        return false;
    }

    int title_height = WINDOW_TITLEBAR_HEIGHT;

    return (mouse_x >= window->pos_x && mouse_x < window->pos_x + window->width && mouse_y >= window->pos_y && mouse_y < window->pos_y + title_height);
}

window_button get_window_button(window_struct* window, int mouse_x, int mouse_y) 
{
    if (!window || !window->visible)
    {
        return BUTTON_NONE;
    }

    int title_height = WINDOW_TITLEBAR_HEIGHT;

    if (mouse_y >= window->pos_y && mouse_y < window->pos_y + title_height) 
    {
        int minimize_start = window->pos_x + window->width - 58; 
        int rel_x = mouse_x - minimize_start;

        if (rel_x >= 0 && rel_x < 14) 
        {
            return BUTTON_MINIMIZE;
        }
        if (window->can_maximize && rel_x >= 14 && rel_x < 36) 
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
