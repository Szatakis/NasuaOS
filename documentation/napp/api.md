# NAPP API Reference

The `napp_api` structure provides NAPP applications with access to all kernel services. It is passed as a pointer to the application's `_start` function.

## The napp_api Structure

```cpp
struct napp_api
{
    napp_window window;                // Window management functions
    napp_gui gui;                      // GUI drawing functions
    // ... additional fields below
};
```

The full structure is defined in `utilities/applications/include/napp.h`.

## Function Pointer Fields

### Core I/O Functions

| Field | Signature | Description |
|-------|-----------|-------------|
| `argc` | `int` | Number of command-line arguments |
| `argv` | `char**` | Array of argument strings (`argv[0]` = app name) |
| `print` | `void (*)(const char*)` | Print a string to the terminal/log |
| `print_info` | `void (*)(const char*)` | Print an INFO-level log message |
| `print_line` | `void (*)(const char*)` | Print a string followed by a newline |
| `print_dec` | `void (*)(int)` | Print a decimal integer |
| `print_hex` | `void (*)(unsigned int)` | Print a hex integer |
| `serial_log` | `void (*)(const char*)` | Send a string to the UART serial port |

### Filesystem Functions

| Field | Signature | Description |
|-------|-----------|-------------|
| `current_path` | `const char*` | Current working directory path |
| `clawfs_create_file` | `int (*)(const char*)` | Create a file at the given path |
| `clawfs_write_to_file` | `int (*)(const char*, const char*)` | Write content to a file |
| `clawfs_mkdir` | `int (*)(const char*)` | Create a directory |
| `clawfs_rm` | `int (*)(const char*)` | Remove a file |
| `clawfs_read_file` | `int (*)(const char*, char*)` | Read a file into a buffer |
| `clawfs_list_dir` | `void (*)(const char*)` | List directory contents |

### Time / RTC Functions

| Field | Signature | Description |
|-------|-----------|-------------|
| `get_rtc_time` | `void (*)()` | Read and print current RTC time |
| `set_rtc_time` | `void (*)(int, int, int, int, int, int)` | Set RTC (y, m, d, h, m, s) |
| `get_uptime` | `int (*)()` | Get system uptime in seconds |

### Storage Functions

| Field | Signature | Description |
|-------|-----------|-------------|
| `storage_uses_ata` | `bool (*)()` | Check if ATA storage is available |
| `storage_read` | `int (*)(uint64_t, void*, uint32_t)` | Read sectors from storage |

### Execution Functions

| Field | Signature | Description |
|-------|-----------|-------------|
| `napp_run_path` | `int (*)(const char*)` | Load and run a NAPP binary from a path |
| `system_file_exists` | `bool (*)(const char*)` | Check if a system file exists |

### Window Management (`napp_window`)

| Field | Signature | Description |
|-------|-----------|-------------|
| `create_window` | `int (*)(int, int, const char*)` | Create a window (width, height, title) |
| `close_window` | `void (*)()` | Close the current window |
| `draw_window` | `void (*)()` | Render the window |
| `on_draw` | callback | Drawing callback (called each frame) |
| `on_key` | callback | Keyboard input callback |
| `on_mouse` | callback | Mouse input callback |

### GUI Drawing (`napp_gui`)

| Field | Signature | Description |
|-------|-----------|-------------|
| `fill_block` | `void (*)(int, int, int, int, unsigned int)` | Fill a rectangular area with color |
| `draw_text` | `void (*)(int, int, const char*, unsigned int)` | Draw text at coordinates |
| `draw_rect` | `void (*)(int, int, int, int, unsigned int)` | Draw a rectangle outline |
| `draw_rect_fill` | `void (*)(...)` | Fill a rectangle |
| `put_pixel` | `void (*)(int, int, unsigned int)` | Set a single pixel |

## GUI Callback Structure

NAPP GUI applications register callbacks for drawing and input events:

```cpp
napp_window_draw draw_callback;  // Called when the window needs redrawing
napp_window_key key_callback;    // Called on keyboard input
napp_window_mouse mouse_callback;// Called on mouse input
```

These are typedef'd in `napp.h`:

```cpp
typedef void (*napp_window_draw)(const napp_api*);
typedef void (*napp_window_key)(const napp_api*, unsigned char);
typedef void (*napp_window_mouse)(const napp_api*, int, int, int);
```

## Example Usage

```cpp
#include <napp.h>

NAPP_APPLICATION("my_app", "A brief description of the app");

void on_draw(const napp_api* api)
{
    api->gui.fill_block(0, 0, 800, 600, NAPP_COLOR_BLUE);
    api->gui.draw_text(10, 10, "Hello from NAPP!", NAPP_COLOR_WHITE);
}

void on_key(const napp_api* api, unsigned char key)
{
    if (key == 'q')
        api->window.close_window();
}

int _start(const napp_api* api)
{
    api->window.create_window(800, 600, "My App");
    api->window.on_draw = on_draw;
    api->window.on_key = on_key;
    api->window.draw_window();
    return 0;
}
```

## NAPP_WINDOW Structure

For GUI applications, the `napp_window` struct defines window properties:

```cpp
struct napp_window_config
{
    int    x;
    int    y;
    int    width;
    int    height;
    bool   visible;
    bool   resizable;
    char   title[64];
};
```

## NAPP_GUI INTERFACE

The `napp_gui` struct provides a virtual function table for GUI operations:

```cpp
struct napp_gui
{
    void (*fill_block)(int x, int y, int w, int h, uint32_t color);
    void (*draw_text)(int x, int y, const char* text, uint32_t color);
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_rect_fill)(int x, int y, int w, int h, uint32_t color);
    void (*put_pixel)(int x, int y, uint32_t color);
};
```

## Error Handling

NAPP applications return an `int` from `_start`. A return value of `0` indicates success; non-zero indicates an error. The kernel logs the return code when the application exits.
