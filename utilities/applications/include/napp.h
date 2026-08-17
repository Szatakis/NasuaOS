#pragma once

#include <stdint.h>

// NasuaOS application (.napp) binary format.
//
// A .napp file is a flat binary image: a napp_header at offset 0, directly
// followed by the application code, data and read only data. The kernel loads
// the whole image into memory and calls the entry point with a pointer to the
// napp_api structure.

#define NAPP_MAGIC 0x5050414E // 'N' 'A' 'P' 'P'
#define NAPP_ABI_VERSION 2
#define NAPP_HEADER_SIZE 64
#define NAPP_NAME_LENGTH 32

struct napp_header
{
    uint32_t magic;
    uint32_t abi_version;
    uint32_t header_size;
    uint32_t entry_offset;

    char name[NAPP_NAME_LENGTH];

    uint32_t reserved[4];
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
};

// Drawing and window services, only usable while a window callback runs or to
// open a window from the entry point.
struct napp_gui
{
    bool (*open_window)(const struct napp_window_config* config);
    void (*fill_block)(int x, int y, uint32_t color, int width, int height);
    void (*draw_text)(const char* text, int x, int y, uint32_t color);
};

// Services the kernel exposes to a running application.
struct napp_api
{
    uint32_t abi_version;

    void (*print)(const char* text);
    void (*print_line)(const char* text);
    void (*print_dec)(uint32_t value);
    void (*print_hex)(uint32_t value);
    void (*sleep_ms)(uint32_t milliseconds);
    void (*serial_log)(const char* text);

    const struct napp_gui* gui;
};

typedef int (*napp_entry)(const struct napp_api* api);

// Emits the header and pins the entry point right behind it.
#define NAPP_APPLICATION(app_name)                                             \
    extern "C" __attribute__((section(".napp_entry"), used))                   \
    int _start(const napp_api* api);                                           \
                                                                               \
    __attribute__((section(".napp_header"), used))                             \
    const napp_header napp_application_header =                                \
    {                                                                          \
        NAPP_MAGIC,                                                            \
        NAPP_ABI_VERSION,                                                      \
        NAPP_HEADER_SIZE,                                                      \
        NAPP_HEADER_SIZE,                                                      \
        app_name,                                                              \
        { 0, 0, 0, 0 }                                                         \
    }
