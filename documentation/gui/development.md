# GUI Development

The NasuaOS GUI is a simple 2D graphical environment managed by the kernel. GUI windows are registered with the window manager and rendered to a backbuffer each frame.

## GUI Architecture

```
┌─────────────────────────────────────────────┐
│              GUI System                     │
├─────────────────────────────────────────────┤
│  Start Menu                                 │
│  (gui.cpp)                                  │
├─────────────────────────────────────────────┤
│  Window Manager                             │
│  (window.cpp)                               │
├─────────────────────────────────────────────┤
│  Window 1    Window 2    Window 3           │
│  (App callbacks)                            │
├─────────────────────────────────────────────┤
│  Desktop Background                         │
│  (image_init, draw_background)              │
├─────────────────────────────────────────────┤
│  Backbuffer                                 │
│  → render_frame() → VRAM                    │
└─────────────────────────────────────────────┘
```

## Drawing System

All GUI rendering writes to a backbuffer array, then `render_frame()` copies it to video memory:

```cpp
static uint32_t backbuffer[4096 * 2160];
```

### Primary Drawing Functions

| Function     | Signature                                                  | Description            |
|--------------|------------------------------------------------------------|------------------------|
| `put_pixel`  | `(size_t x, size_t y, uint32_t color)`                     | Set a single pixel     |
| `fill_block` | `(size_t x, size_t y, uint32_t color, size_t w, size_t h)` | Fill a rectangle       |
| `draw_rect`  | `(int x1, int y1, int x2, int y2, uint32_t color)`         | Draw rectangle outline |
| `print_at8`  | `(const char* text, size_t x, size_t y, uint32_t color)`   | Render 8px-width text  |
| `print_at10` | `(const char* text, size_t x, size_t y, uint32_t color)`   | Render 10px-width text |
| `print_at12` | `(const char* text, size_t x, size_t y, uint32_t color)`   | Render 12px-width text |
| `print_at16` | `(const char* text, size_t x, size_t y, uint32_t color)`   | Render 16px-width text |

### Color Format

Colors are 32-bit ARGB values:

| Format       | Example    | Description                         |
|--------------|------------|-------------------------------------|
| `0xAARRGGBB` | `0xFF5500` | Alpha=FF, Red=55, Green=00, Blue=00 |

Common colors:
- `0xFFFFFFFF` — White
- `0xFF000000` — Black
- `0xFF59E81B` — Green (theme accent)
- `0xFFAAAAAA` — Gray
- `0xFF1A1F2C` — Window background
- `0xFF2B3140` — Title bar

## Window Management

Source: `kernel_64bit/src/system/drivers/gpu/functions/windows_manager/window.cpp`

### Window Structure

The kernel's `Gpu::Window_Manager::window_struct` (defined in `drivers/gpu/driver.hpp`):

```cpp
typedef struct window_struct
{
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

    bool is_resizing;
    bool resize_right;
    bool resize_bottom;

    int resize_start_mouse_x;
    int resize_start_mouse_y;
    int resize_start_width;
    int resize_start_height;

    int max_width;
    int max_height;

    void* userdata;

    window_draw_callback draw_content;
    window_key_callback key_press;
    window_mouse_callback mouse_click;
    window_mouse_button_callback mouse_button;
} window_struct;

### Window Limits

| Constant      | Value | Description                  |
|---------------|-------|------------------------------|
| `MAX_WINDOWS` | 13    | Maximum simultaneous windows |

### Window Functions

| Function                          | Description                         |
|-----------------------------------|-------------------------------------|
| `register_window(window_struct*)` | Register a new window               |
| `unregister_window(window_struct*)` | Remove a window                   |
| `update_windows_positions(int, int)` | Update all window positions      |
| `draw_windows()`                  | Render all windows to backbuffer    |
| `is_mouse_over_any_window(int, int)` | Check if cursor is over a window |

### Window Registration

When a NAPP application creates a GUI window:

```cpp
napp_window_config config = {};
config.title = "My App";
config.width = 800;
config.height = 600;
config.draw = my_draw_callback;
config.key = my_key_callback;

if (!api->gui->open_window(&config))
{
    return 1;
}
```

The window manager:
1. Allocates a `window_struct` from the window pool (max 13)
2. Sets visibility, position, and dimensions from the config
3. Registers the window's draw callback
4. The window is rendered in each frame via `draw_windows()`

## Start Menu

Source: `kernel_64bit/src/system/gui/gui.cpp`

The start menu is a fixed UI element drawn by the kernel:

```cpp
bool start_menu_open = false;
```

### Start Menu Functions

| Function             | Description                     |
|----------------------|---------------------------------|
| `open_start_menu()`  | Show the start menu             |
| `close_start_menu()` | Hide the start menu             |
| `draw_start_menu()`  | Render start menu to backbuffer |

The start menu is toggled via the GUI state manager and responds to mouse clicks.

## Desktop Background

```cpp
void image_init();    // Load background image data
void draw_background(); // Render to backbuffer
```

The background image is loaded during `iqu_init() → image_init()` and rendered each frame behind windows.

## Rendering Pipeline

Each frame:

1. `clear_screen()` — Clear backbuffer
2. `draw_background()` — Draw desktop background
3. `draw_windows()` — Draw all registered windows in Z-order
4. `draw_start_menu()` — Draw start menu (if open)
5. `render_frame()` — Copy backbuffer to VRAM

## NAPP GUI Applications

NAPP GUI applications interact with the GUI via the `napp_api` interface. The `gui`
field is a pointer to a `napp_gui` virtual function table:

```cpp
// Create a window
napp_window_config config = {};
config.title = "My App";
config.width = 800;
config.height = 600;
config.resizable = true;
config.can_maximize = true;
config.userdata = nullptr;
config.draw = my_draw_callback;
config.key = my_key_callback;
config.mouse = my_mouse_callback;

if (!api->gui->open_window(&config))
{
    return 1;
}
```

### Callback Signatures

```cpp
typedef void (*napp_window_draw)(struct napp_window* window);
typedef void (*napp_window_key)(struct napp_window* window, char key);
typedef void (*napp_window_mouse)(struct napp_window* window, int mouse_x, int mouse_y);
typedef void (*napp_window_mouse_button)(struct napp_window* window, int mouse_x, int mouse_y, int button);
typedef void (*napp_window_tick)(struct napp_window* window);
```

### Draw Callback Example

```cpp
static const napp_gui* gui = nullptr;

void my_draw_callback(napp_window* win)
{
    // Fill window background
    gui->fill_block(win->pos_x, win->pos_y, NAPP_COLOR_WINDOW, win->width, win->height);
    
    // Draw text
    gui->draw_text("Hello, World!", win->pos_x + 10, win->pos_y + 10, NAPP_COLOR_WHITE);
}
```

Note: The `napp_window` struct passed to callbacks is a read-only view of the
window's geometry, updated by the kernel before each callback. It contains `pos_x`,
`pos_y`, `width`, `height`, `title_height`, and `userdata` fields.

## Text Rendering (GUI Mode)

For GUI applications, text is rendered via:

```cpp
void draw_text(int x, int y, const char* text, uint32_t color);
```

This function:
- Renders each character using the 8×16 pixel font bitmap
- Supports ASCII characters 32–127
- Colors are applied per-character
- No Unicode or multi-byte character support

## Event Model

The GUI uses a **polling** model:

1. Each frame, the kernel iterates all registered windows
2. Each window's `draw_content` callback is invoked (which dispatches to the NAPP app's draw callback)
3. Mouse and keyboard events are dispatched to the active window
4. The draw callback is called every frame, so applications should only redraw when needed

## Z-Order

Windows are stacked using a simple front-to-back Z-order:

- The most recently created window is drawn on top
- The start menu always renders above all application windows
- `MAX_WINDOWS = 13` limits the total number of windows

## Limitations

| Limitation      | Details                                                     |
|-----------------|-------------------------------------------------------------|
| No compositing  | Windows are drawn directly to backbuffer, no alpha blending |
| Fixed font      | Only 8×16 bitmap font supported                             |
| No drag/resize  | Windows cannot be moved or resized after creation           |
| ASCII only      | No Unicode support                                          |
| Single-threaded | No window threading — draw callbacks block the frame        |

## Double Buffering

To prevent tearing:

```cpp
void render_frame()
{
    memcpy(vram, backbuffer, framebuffer_size);
}
```

The backbuffer is a `uint32_t` array sized to match the framebuffer resolution. After all rendering is complete, the array is copied to VGA memory in a single operation.
