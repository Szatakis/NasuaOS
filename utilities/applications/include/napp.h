#pragma once

#include <stdint.h>

// NasuaOS application (.napp) binary format.
//
// A .napp file is a flat binary image: a napp_header at offset 0, directly
// followed by the application code, data and read only data. The kernel loads
// the whole image into memory and calls the entry point with a pointer to the
// napp_api structure.

#define NAPP_MAGIC 0x5050414E // 'N' 'A' 'P' 'P'
#define NAPP_ABI_VERSION 3
#define NAPP_NAME_LENGTH 32
#define NAPP_DESCRIPTION_LENGTH 48
#define NAPP_HEADER_SIZE 96

struct napp_header
{
    uint32_t magic;
    uint32_t abi_version;
    uint32_t header_size;
    uint32_t entry_offset;

    char name[NAPP_NAME_LENGTH];
    char description[NAPP_DESCRIPTION_LENGTH];

    // When false, the application does not appear in the Start Menu.
    // Present only when header_size > NAPP_HEADER_SIZE; older binaries
    // default to true.
    bool show_in_start_menu;
} __attribute__((packed));

// Colors of the system theme, usable by graphical applications.
#define NAPP_COLOR_BLACK      0xFF000000
#define NAPP_COLOR_WHITE      0xFFFFFFFF
#define NAPP_COLOR_GREEN      0xFF59E81B
#define NAPP_COLOR_GRAY       0xFFAAAAAA
#define NAPP_COLOR_WINDOW     0xFF1A1F2C
#define NAPP_COLOR_TITLEBAR   0xFF2B3140

// A window owned by an application. The kernel keeps the geometry up to date
// before every callback; the application only reads it.
struct napp_window
{
    int pos_x;
    int pos_y;
    int width;
    int height;
    int title_height;

    void* userdata;
};

typedef void (*napp_window_draw)(struct napp_window* window);
typedef void (*napp_window_key)(struct napp_window* window, char key);
typedef void (*napp_window_mouse)(struct napp_window* window, int mouse_x, int mouse_y);

// Button-enabled mouse callback. The button parameter is 0 for left click,
// 1 for right click, and 2 for middle click. Called for all mouse buttons
// when mouse_button is set; otherwise mouse (left-click only) is used.
typedef void (*napp_window_mouse_button)(struct napp_window* window, int mouse_x, int mouse_y, int button);

// Periodic callback invoked by the kernel at tick_interval_ms intervals.
// Only called while the window is visible and the game/application is active.
typedef void (*napp_window_tick)(struct napp_window* window);

struct napp_window_config
{
    const char* title;

    int width;
    int height;

    bool resizable;
    bool can_maximize;

    void* userdata;

    napp_window_draw draw;
    napp_window_key key;
    napp_window_mouse mouse;
    napp_window_mouse_button mouse_button;

    // Optional periodic callback, invoked every tick_interval_ms milliseconds.
    // When tick is nullptr or tick_interval_ms <= 0, no periodic callback is
    // registered and the field is simply ignored.
    napp_window_tick tick;
    int tick_interval_ms;
};

// Drawing and window services, only usable while a window callback runs or to
// open a window from the entry point.
struct napp_gui
{
    bool (*open_window)(const struct napp_window_config* config);
    void (*fill_block)(int x, int y, uint32_t color, int width, int height);
    void (*draw_text)(const char* text, int x, int y, uint32_t color);
    void (*resize_window)(int width, int height);
};

// Services the kernel exposes to a running application.
struct napp_api
{
    uint32_t abi_version;

    void (*print)(const char* text);
    void (*print_info)(const char* text);
    void (*print_warn)(const char* text);
    void (*print_error)(const char* text);
    void (*print_line)(const char* text);
    void (*print_dec)(uint32_t value);
    void (*print_hex)(uint32_t value);
    void (*sleep_ms)(uint32_t milliseconds);
    void (*serial_log)(const char* text);

    const struct napp_gui* gui;

    // Arguments passed by the shell
    int argc;
    const char* const* argv;

    // Filesystem additions for system commands
    const char* current_path;
    void (*set_cwd)(const char* path);
    uint32_t (*clawfs_get_sector)(const char* path);
    bool (*clawfs_read_sector)(uint32_t sector, void* buffer);
    bool (*clawfs_write_sector)(uint32_t sector, const void* buffer);
    void (*clawfs_mkdir)(const char* parent_path, const char* dir_name);
    void (*clawfs_rm)(const char* parent_path, const char* name, uint32_t type);
    void (*clawfs_create_file_in)(const char* path, const char* name);
    void (*clawfs_dir)(const char* path);
    int (*clawfs_get_entry_type)(const char* parent_path, const char* name);
    uint32_t (*clawfs_resolve_path)(const char* cur_path, const char* path);

    // Monotonic tick counter (PIT ticks, 100 Hz = 10 ms per tick).
    // Returns the number of timer ticks since boot.
    uint64_t (*get_ticks)(void);

    // Execute a shell command (for terminal applications)
    void (*execute_command)(const char* cmd);

    // Redirect Gpu::print output to an in-app callback. Pass nullptr to
    // restore normal output to the screen. Only one redirect can be active
    // at a time; the last call wins.
    void (*set_print_redirect)(void (*callback)(char));
};

typedef int (*napp_entry)(const struct napp_api* api);

// Emits the header and pins the entry point right behind it.
// The third parameter controls whether the application appears in the
// Start Menu. Use false for utility/console apps and games that should
// only be launched on demand.
#define NAPP_APPLICATION(app_name, app_description, app_show_in_menu)       \
    extern "C" __attribute__((section(".napp_entry"), used))                \
    int _start(const napp_api* api);                                        \
                                                                            \
    __attribute__((section(".napp_header"), used))                          \
    const napp_header napp_application_header =                             \
    {                                                                       \
        NAPP_MAGIC,                                                         \
        NAPP_ABI_VERSION,                                                   \
        sizeof(napp_header),                                                \
        sizeof(napp_header),                                                \
        app_name,                                                           \
        app_description,                                                    \
        app_show_in_menu                                                    \
    }
