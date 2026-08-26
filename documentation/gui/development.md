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

| Function     | Signature                                          | Description            |
|--------------|----------------------------------------------------|------------------------|
| `put_pixel`  | `(int x, int y, uint32_t color)`                   | Draw a single pixel    |
| `fill_block` | `(int x, int y, int w, int h, uint32_t color)`     | Fill a rectangle       |
| `draw_rect`  | `(int x, int y, int w, int h, uint32_t color)`     | Draw rectangle outline |
| `draw_line`  | `(int x1, int y1, int x2, int y2, uint32_t color)` | Draw a line            |
| `draw_text`  | `(int x, int y, const char* text, uint32_t color)` | Render text            |

### Color Format

Colors are 32-bit ARGB values:

| Format       | Example    | Description                         |
|--------------|------------|-------------------------------------|
| `0xAARRGGBB` | `0xFF5500` | Alpha=FF, Red=55, Green=00, Blue=00 |

Common colors:
- `0xFFFFFFFF` — White
- `0xFF000000` — Black
- `0xFFFF0000` — Red
- `0xFF00FF00` — Green
- `0xFF0000FF` — Blue

## Window Management

Source: `kernel_64bit/src/system/drivers/gpu/functions/windows_manager/window.cpp`

### Window Structure

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

### Window Limits

| Constant      | Value | Description                  |
|---------------|-------|------------------------------|
| `MAX_WINDOWS` | 13    | Maximum simultaneous windows |

### Window Functions

| Function                          | Description                         |
|-----------------------------------|-------------------------------------|
| `register_window(window_struct*)` | Register a new window               |
| `unregister_window(int id)`       | Unregister a window by ID           |
| `update_windows_positions()`      | Update window Z-order and positions |
| `draw_windows()`                  | Render all windows to backbuffer    |
| `is_mouse_over_any_window()`      | Check cursor over windows           |

### Window Registration

When a NAPP application creates a GUI window:

```cpp
api->window.create_window(800, 600, "My App");
```

The window manager:
1. Allocates a `window_struct` from the window pool (max 13)
2. Sets visibility, position, and dimensions
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

NAPP GUI applications interact with the GUI via the `napp_api` interface:

```cpp
// Create a window
api->window.create_window(800, 600, "My App");

// Register callbacks
api->window.on_draw = my_draw_callback;
api->window.on_key = my_key_callback;
api->window.on_mouse = my_mouse_callback;

// Render immediately
api->window.draw_window();
```

### Callback Signatures

```cpp
typedef void (*napp_window_draw)(const napp_api*);
typedef void (*napp_window_key)(const napp_api*, unsigned char);
typedef void (*napp_window_mouse)(const napp_api*, int x, int y, int button);
```

### Draw Callback Example

```cpp
void my_draw_callback(const napp_api* api)
{
    // Fill window background
    api->gui.fill_block(0, 0, 800, 600, 0xFF202020);
    
    // Draw a button
    api->gui.draw_rect(100, 100, 200, 50, 0xFFFFFFFF);
    
    // Draw text
    api->gui.draw_text(150, 120, "Click Me", 0xFF000000);
}
```

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

1. Each frame, the kernel checks for window updates
2. If a window's `needs_redraw` flag is set, its draw callback is invoked
3. Mouse and keyboard events are dispatched to the active window
4. Applications should set `needs_redraw = true` when they want to redraw

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
