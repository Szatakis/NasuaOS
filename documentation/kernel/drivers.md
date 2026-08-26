# Kernel - Drivers

The 64-bit kernel uses a driver architecture with GPU, input, timer, and storage drivers. This document covers the GPU driver and drawing primitives.

## GPU Driver

Source: `kernel_64bit/src/system/drivers/gpu/driver.hpp`

The GPU driver is the primary output mechanism for the kernel. It provides text rendering, graphics primitives, and backbuffer management.

### Backbuffer System

The kernel uses **double buffering** to prevent screen tearing:

1. All rendering goes to a **backbuffer** (an in-memory framebuffer)
2. After rendering is complete, `render_frame()` copies the backbuffer to the actual VGA/VRAM

```cpp
static uint32_t backbuffer[4096 * 2160];  // VRAM-sized backbuffer
```

The backbuffer dimensions depend on the detected resolution. The `init_backbuffer()` function allocates and clears it, and `render_frame()` performs the copy.

### Text Rendering

The text subsystem (`text.cpp`) manages an 80×25 character text buffer overlaid on the graphics output.

| Function                         | Description                             |
|----------------------------------|-----------------------------------------|
| `print_char8(char, x, y, color)` | Draw a single 8×16 character            |
| `print_at8(const char*, x, y)`   | Print string at coordinates (8px font)  |
| `print_at10(const char*, x, y)`  | Print string at coordinates (10px font) |
| `print_at12(const char*, x, y)`  | Print string at coordinates (12px font) |
| `print_at16(const char*, x, y)`  | Print string at coordinates (16px font) |
| `print(const char*)`             | Print to the kernel's text position     |
| `print_int(int)`                 | Print an integer                        |
| `print_hex(unsigned int)`        | Print hex value                         |
| `delete_last_char()`             | Remove last character                   |
| `print_info(const char*)`        | Print INFO-colored message (yellow)     |
| `print_warn(const char*)`        | Print WARN-colored message (orange)     |
| `print_error(const char*)`       | Print ERROR-colored message (red)       |
| `fetch()`                        | Print ASCII art logo and system info    |

### Text Escape Color Codes

The kernel uses ANSI-like escape codes for colored terminal output:

| Code     | Color      | Usage          |
|----------|------------|----------------|
| `\x1B[w` | White      | Normal text    |
| `\x1B[e` | Yellow     | Info messages  |
| `\x1B[a` | Orange     | Warning        |
| `\x1B[1` | Light Gray | Headers        |
| `\x1B[2` | Gray       | Secondary text |
| `\x1B[3` | Red        | Errors         |
| `\x1B[4` | Green      | Success        |
| `\x1B[5` | Blue       | Links          |
| `\x1B[6` | Dark Gray  | Footers        |

### Color Definitions

```cpp
enum colors {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GRAY = 7,
    DARK_GRAY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    PINK = 13,
    YELLOW = 14,
    WHITE = 15,
};
```

### Drawing Primitives

| Function | Description |
|----------|-------------|
| `put_pixel(x, y, color)` | Set a single pixel |
| `clear_screen(color)` | Fill entire screen with a color |
| `fill_block(x, y, w, h, color)` | Fill a rectangle |
| `draw_rect(x, y, w, h, color)` | Draw a rectangle outline |
| `draw_line(x1, y1, x2, y2, color)` | Draw a line |

### Shell Command Printing

`print_cmd()` renders text at specific screen coordinates:

```cpp
void print_cmd(const char* str, int x, int y, uint32_t color);
```

Used for shell output messages and UI elements.

### Window Manager

Source: `kernel_64bit/src/system/drivers/gpu/functions/windows_manager/window.cpp`

The window manager handles overlapping GUI windows:

| Constant      | Value |
|---------------|-------|
| `MAX_WINDOWS` | 13    |

| Function                          | Description                      |
|-----------------------------------|----------------------------------|
| `register_window(window_struct*)` | Register a new window            |
| `unregister_window(int)`          | Remove a window by ID            |
| `update_windows_positions()`      | Update all window positions      |
| `draw_windows()`                  | Render all windows to backbuffer |
| `is_mouse_over_any_window()`      | Check if cursor is over a window |

```cpp
struct window_struct
{
    int x, y;
    int width, height;
    uint32_t color;
    bool visible;
    char title[64];
    void (*draw_func)(int, int, int, int, uint32_t);
};
```

## GUI State Management

Source: `kernel_64bit/src/system/gui/gui.cpp`

The GUI subsystem manages the desktop state:

| Function               | Description                         |
|------------------------|-------------------------------------|
| `open_start_menu()`    | Open the start menu                 |
| `close_start_menu()`   | Close the start menu                |
| `draw_start_menu()`    | Render the start menu to backbuffer |
| `update_gui_state()`   | Process GUI state changes           |
| `update_windows_gui()` | Update all GUI windows              |
| `image_init()`         | Initialize the background image     |
| `draw_background()`    | Render the desktop background       |

Global state:

```cpp
bool start_menu_open = false;
```

## Input Drivers

### Keyboard

Source: `kernel_64bit/src/system/drivers/keyboard/`

PS/2 keyboard driver with scancode-to-ASCII conversion:

```cpp
char command_buffer[64];  // Shell input buffer
bool shell_input_enabled = true;
```

Key behavior:
- `Enter` (scancode `0x1C`) — triggers `execute_command()`
- `Backspace` (scancode `0x0E`) — deletes last character from buffer
- `Shift` + `↑` — previous command in history
- `Shift` + `↓` — next command in history

### Mouse

Source: `kernel_64bit/src/system/drivers/mouse/`

PS/2 mouse driver:

```cpp
int mouse_x = 200;  // Default cursor X position
int mouse_y = 145;  // Default cursor Y position
```

The mouse handler processes:
- Movement packets → cursor position update
- Left button click → click handler (window drag, button press)
- Right button click → context menu / close window

Arrow keys (without Shift) also move the cursor:
- `←↑↓→` move cursor by 10 pixels
- With GUI active, input goes to the active window

## Timer (PIT)

Source: `kernel_64bit/src/system/drivers/timer/driver.hpp`

Programmable Interval Timer running at 100 Hz:

```cpp
void pit_init();
uint64_t pit_get_ticks();  // Returns tick count (10ms per tick)
void sleep(uint32_t ms);   // Millisecond delay
```

Used for: uptime calculation, sleep/delay, time-based operations
