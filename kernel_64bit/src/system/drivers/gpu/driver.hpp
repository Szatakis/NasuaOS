#pragma once
#include <limine.h>
#include <cstddef>
#include <cstdint>

#define WINDOW_TITLEBAR_HEIGHT 24
#define WINDOW_RESIZE_BORDER 6
#define WINDOW_MIN_WIDTH 150
#define WINDOW_MIN_HEIGHT 100

extern limine_framebuffer* fb;

extern size_t cursor_x;
extern size_t cursor_y;

extern uint32_t current_text_color;

extern const uint32_t FONT_SPACING_W;
extern const uint32_t FONT_SPACING_H;

// Backbuffer management
void init_backbuffer(size_t width, size_t height, size_t pitch);
uint32_t* get_backbuffer();
size_t get_backbuffer_pitch();
void render_frame();

void init_text_buffer();
void draw_text_buffer();

void put_pixel(size_t x, size_t y, uint32_t color);

void clear_screen();
void clear_line();

void print(const char* str);
void print_int(int value);
void print_hex(uint32_t value);
void print_char8(char c);

void print_at8(const char* str, size_t x, size_t y, uint32_t color);
void print_at10(const char* str, size_t x, size_t y, uint32_t color);
void print_at12(const char* str, size_t x, size_t y, uint32_t color);
void print_at16(const char* str, size_t x, size_t y, uint32_t color);

void delete_last_char(size_t font_size);

void handle_mouse();

void update_bottom_bar();

// Font Draw
void draw_char8(unsigned char c, size_t x, size_t y, uint32_t color);
void draw_char10(unsigned char c, size_t x, size_t y, uint32_t color);
void draw_char12(unsigned char c, size_t x, size_t y, uint32_t color);
void draw_char16(unsigned char c, size_t x, size_t y, uint32_t color);

void print_num8(uint32_t num);

void update_gui();
void fill_block(size_t x, size_t y, uint32_t color, size_t size_x, size_t size_y);
void draw_rect(int x1, int y1, int x2, int y2, uint32_t color);

void print_info(const char* msg);
void print_warn(const char* msg);
void print_error(const char* msg);
void print_cmd();

void print_num_padded(uint32_t num);
void print_year(uint32_t year);

void print_resolution();

void fetch();



typedef void (*window_draw_callback)(struct window_struct*);
typedef void (*window_key_callback)(window_struct*, char);
typedef void (*window_mouse_callback)(window_struct*, int, int);

//Window manager
typedef struct window_struct {
    const char* name;
    uint32_t id;

    int pos_x;
    int pos_y;
    int width;
    int height;

    bool visible;
    bool minimized;
    bool focused;

    bool resizable;
    bool can_maximize;
    bool maximized;

    int restore_pos_x;
    int restore_pos_y;
    int restore_width;
    int restore_height;

    bool is_dragging;
    int drag_offset_x;
    int drag_offset_y;

    // Resize
    bool is_resizing;
    bool resize_right;
    bool resize_bottom;

    int resize_start_mouse_x;
    int resize_start_mouse_y;

    int resize_start_width;
    int resize_start_height;

    void* userdata;

    window_draw_callback draw_content;
    window_key_callback key_press;
    window_mouse_callback mouse_click;
} window_struct;

extern window_struct* active_window;

void send_key_to_window(char key);

enum window_button
{
    BUTTON_NONE,
    BUTTON_MINIMIZE,
    BUTTON_MAXIMIZE,
    BUTTON_CLOSE
};

void draw_window(window_struct* window);

bool is_mouse_over_window(window_struct* window, int mouse_x, int mouse_y);
bool is_mouse_over_window_title(window_struct* window, int mouse_x, int mouse_y);
window_button get_window_button(window_struct* window, int mouse_x, int mouse_y);
void handle_window_mouse_click(int mouse_x, int mouse_y);

void register_window(window_struct* window);
void unregister_window(window_struct* window);
void minimize_window(window_struct* window);
void restore_window(window_struct* window);
void maximize_window(window_struct* window);
void unmaximize_window(window_struct* window);
void draw_windows();
void draw_taskbar_entries();
void update_windows_positions(int current_mouse_x, int current_mouse_y);

bool is_mouse_over_any_window(int mouse_x, int mouse_y);
bool is_mouse_over_taskbar(int mouse_x, int mouse_y);
window_struct* get_taskbar_window_at(int mouse_x, int mouse_y);