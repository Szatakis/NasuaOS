# NAPP API Reference

The `napp_api` structure provides NAPP applications with access to all kernel services. It is passed as a pointer to the application's `_start` function.

## The napp_api Structure

```cpp
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
    const struct napp_gui* gui;          // GUI drawing functions (pointer)
    int argc;                            // Number of command-line arguments
    const char* const* argv;             // Array of argument strings (argv[0] = app name)
    const char* current_path;            // Current working directory path
    void (*set_cwd)(const char* path);   // Set current working directory
    uint32_t (*clawfs_get_sector)(const char* path);
    bool (*clawfs_read_sector)(uint32_t sector, void* buffer);
    bool (*clawfs_write_sector)(uint32_t sector, const void* buffer);
    void (*clawfs_mkdir)(const char* parent_path, const char* dir_name);
    void (*clawfs_rm)(const char* parent_path, const char* name, uint32_t type);
    void (*clawfs_create_file_in)(const char* path, const char* name);
    void (*clawfs_dir)(const char* path);
    int (*clawfs_get_entry_type)(const char* parent_path, const char* name);
    uint32_t (*clawfs_resolve_path)(const char* cur_path, const char* path);
    uint64_t (*get_ticks)(void);         // PIT timer ticks since boot (100 Hz = 10 ms/tick)
    void (*execute_command)(const char* cmd);
    void (*set_print_redirect)(void (*callback)(char));
};
```

The full structure is defined in `utilities/applications/include/napp.h`.

## Function Pointer Fields

### Core I/O Functions

| Field        | Signature                | Description                                      |
|--------------|--------------------------|--------------------------------------------------|
| `abi_version`| `uint32_t`               | NAPP ABI version (currently 3)                   |
| `argc`       | `int`                    | Number of command-line arguments                 |
| `argv`       | `char**`                 | Array of argument strings (`argv[0]` = app name) |
| `print`      | `void (*)(const char*)`  | Print a string to the terminal/log               |
| `print_info` | `void (*)(const char*)`  | Print an INFO-level log message                  |
| `print_warn` | `void (*)(const char*)`  | Print a WARN-level log message                   |
| `print_error`| `void (*)(const char*)`  | Print an ERROR-level log message                 |
| `print_line` | `void (*)(const char*)`  | Print a string followed by a newline             |
| `print_dec`  | `void (*)(uint32_t)`     | Print a decimal integer                          |
| `print_hex`  | `void (*)(uint32_t)`     | Print a hex integer                              |
| `sleep_ms`   | `void (*)(uint32_t)`     | Sleep for the given number of milliseconds       |
| `serial_log` | `void (*)(const char*)`  | Send a string to the UART serial port            |

### GUI Functions

The `gui` field is a pointer to a `const napp_gui` virtual function table:

```cpp
struct napp_gui
{
    bool (*open_window)(const struct napp_window_config* config);
    void (*fill_block)(int x, int y, uint32_t color, int width, int height);
    void (*draw_text)(const char* text, int x, int y, uint32_t color);
    void (*resize_window)(int width, int height);
};
```

| Field         | Signature                                       | Description                                        |
|---------------|-------------------------------------------------|----------------------------------------------------|
| `open_window` | `bool (*)(const napp_window_config*)`           | Create and register a GUI window                   |
| `fill_block`  | `void (*)(int x, int y, uint32_t color, int width, int height)` | Fill a rectangular area with color |
| `draw_text`   | `void (*)(const char* text, int x, int y, uint32_t color)`  | Draw text at coordinates               |
| `resize_window`| `void (*)(int width, int height)`              | Resize the current window                          |

### Filesystem Functions

| Field                    | Signature                           | Description                                        |
|--------------------------|-------------------------------------|----------------------------------------------------|
| `current_path`           | `const char*`                       | Current working directory path                     |
| `set_cwd`                | `void (*)(const char*)`             | Set current working directory                      |
| `clawfs_get_sector`      | `uint32_t (*)(const char*)`         | Get the disk sector for a path                     |
| `clawfs_read_sector`     | `bool (*)(uint32_t, void*)`         | Read a 512-byte sector into a buffer               |
| `clawfs_write_sector`    | `bool (*)(uint32_t, const void*)`   | Write a 512-byte sector from a buffer              |
| `clawfs_mkdir`           | `void (*)(const char*, const char*)`| Create a directory in a parent path                |
| `clawfs_rm`              | `void (*)(const char*, const char*, uint32_t)` | Remove a file/dir (type: 0=file, 1=dir) |
| `clawfs_create_file_in`  | `void (*)(const char*, const char*)`| Create a file in a directory path                  |
| `clawfs_dir`             | `void (*)(const char*)`             | List directory contents                            |
| `clawfs_get_entry_type`  | `int (*)(const char*, const char*)` | Get entry type (0=file, 1=dir, -1=not found)       |
| `clawfs_resolve_path`    | `uint32_t (*)(const char*, const char*)` | Resolve a path to a sector                    |

### Timer Functions

| Field      | Signature                | Description                                       |
|------------|--------------------------|---------------------------------------------------|
| `get_ticks`| `uint64_t (*)(void)`     | Get monotonic PIT timer ticks since boot (100 Hz) |

### Execution Functions

| Field                | Signature                  | Description                                                                                 |
|----------------------|----------------------------|---------------------------------------------------------------------------------------------|
| `sleep_ms`           | `void (*)(uint32_t)`       | Sleep for the specified number of milliseconds                                              |
| `execute_command`    | `void (*)(const char*)`    | Execute a shell command via the kernel                                                      |
| `set_print_redirect` | `void (*)(void (*)(char))` | Redirect kernel print output to an in-app callback; pass `nullptr` to restore normal output |

#### Output Redirect for Integrated Terminals

The `set_print_redirect` function allows a NAPP application (such as an editor
with an integrated terminal) to capture all output produced by `execute_command`
directly into its own terminal buffer.  The callback receives one character at
a time (ANSI colour escape sequences are already consumed by the kernel's
`print` layer before the callback is invoked).  Pass `nullptr` to the same
function to stop redirecting output back to the screen.

## Window Configuration

Applications that want a graphical window call `gui->open_window(&config)` with a
`napp_window_config` struct:

```cpp
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
    napp_window_mouse_button mouse_button;  // Optional
    napp_window_tick tick;                 // Optional periodic callback
    int tick_interval_ms;                  // Interval in ms (kernel is 100 Hz = 10 ms/tick)
};
```

## Callback Typedefs

Callbacks registered in `napp_window_config` receive a pointer to a `napp_window`
struct (a read-only view of the window's geometry, updated by the kernel before
each callback). The `napp_window` struct is defined in `napp.h`:

```cpp
struct napp_window
{
    int pos_x;          // Window X position on screen
    int pos_y;          // Window Y position on screen
    int width;          // Window width
    int height;         // Window height
    int title_height;   // Height of the title bar
    void* userdata;     // User-provided pointer from config
};
```

The callback signatures are:

```cpp
typedef void (*napp_window_draw)(struct napp_window* window);
typedef void (*napp_window_key)(struct napp_window* window, char key);
typedef void (*napp_window_mouse)(struct napp_window* window, int mouse_x, int mouse_y);
typedef void (*napp_window_mouse_button)(struct napp_window* window, int mouse_x, int mouse_y, int button);
typedef void (*napp_window_tick)(struct napp_window* window);
```

| Callback Type             | Parameters                              | Description                                                 |
|---------------------------|-----------------------------------------|-------------------------------------------------------------|
| `napp_window_draw`        | `(napp_window* window)`                 | Drawing callback (called each frame)                        |
| `napp_window_key`         | `(napp_window* window, char key)`       | Keyboard input callback                                     |
| `napp_window_mouse`       | `(napp_window* window, int x, int y)`   | Mouse movement callback                                     |
| `napp_window_mouse_button`| `(napp_window* window, int x, int y, int button)` | Mouse button callback (0=left, 1=right, 2=middle) |
| `napp_window_tick`        | `(napp_window* window)`                 | Periodic callback at `tick_interval_ms`                     |

### Tick Callback

The `tick` callback is invoked by the kernel at `tick_interval_ms` intervals while
the window is visible. When `tick` is `nullptr` or `tick_interval_ms <= 0`, no
periodic callback is registered. This is useful for games and animations.

### Mouse Button Callback

The `mouse_button` callback is called for all mouse buttons (left, right, middle)
when set. If only left-click handling is needed, the simpler `mouse` callback
can be used instead. The `button` parameter is `0` for left click, `1` for right
click, and `2` for middle click.

## Example Usage

```cpp
#include <napp.h>

NAPP_APPLICATION("my_app", "A brief description of the app", true);

static const napp_gui* gui = nullptr;
static const napp_api* app_api = nullptr;

void on_draw(napp_window* win)
{
    gui->fill_block(win->pos_x, win->pos_y, NAPP_COLOR_WINDOW, win->width, win->height);
    gui->draw_text("Hello from NAPP!", 10, 10, NAPP_COLOR_WHITE);
}

void on_key(napp_window* win, char key)
{
    if (key == 'q')
    {
        app_api->serial_log("Application shutting down\n");
    }
}

void on_mouse(napp_window* win, int mouse_x, int mouse_y)
{
    // Handle mouse movement
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr)
    {
        return 1;
    }

    app_api = api;
    gui = api->gui;

    napp_window_config config = {};
    config.title = "My App";
    config.width = 800;
    config.height = 600;
    config.resizable = true;
    config.can_maximize = true;
    config.userdata = nullptr;
    config.draw = on_draw;
    config.key = on_key;
    config.mouse = on_mouse;
    config.mouse_button = nullptr;
    config.tick = nullptr;
    config.tick_interval_ms = 0;

    if (!gui->open_window(&config))
    {
        api->serial_log("[MyApp] Failed to open window\n");
        return 1;
    }

    api->serial_log("[MyApp] Window opened\n");
    return 0;
}
```

## Error Handling

NAPP applications return an `int` from `_start`. A return value of `0` indicates success; non-zero indicates an error. The kernel logs the return code when the application exits.

Applications should always check that `api` is not null and that `api->abi_version` matches `NAPP_ABI_VERSION` before using the API:

```cpp
if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr)
{
    return 1;
}
```