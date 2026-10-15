# Kernel - Drivers

Driver implementations are kept in the single shared tree at `resources/drivers/`. Both kernels include driver declarations through `resources/drivers/drivers.hpp`; the per-kernel `system/drivers/drivers.hpp` headers add only kernel-specific boot interfaces.

The x86_64 build compiles driver sources under `resources/drivers/`. The legacy x86_32 build compiles the same GPU and keyboard source files, plus shared libraries. GPU framebuffer storage, pixel writes, presentation, glyph drawing, rectangle drawing, and text formatting are single shared implementations. x86_32 retains only a Multiboot framebuffer adapter and a polling console output sink. The x86_64-only GUI text buffer, taskbar, mouse, and window-manager integration is kept under `resources/drivers/gpu/functions/64bit_driver/` and is excluded from the x86_32 source list.

The remaining x86_64 driver sources are not compatible with the x86_32 kernel merely by compiling them with `-m32`. They use kernel services that the x86_32 kernel does not provide, including Limine/HHDM memory management (`resources/drivers/memory/`), the x86_64 interrupt subsystem, UART/logger interfaces, and shell/GUI APIs. For example, APIC maps MMIO through the Limine HHDM and `Memory::vmm`, while ACPI calls the shell and logging APIs. Ethernet and network drivers are intentionally 64-bit-only: their PCI access, DMA memory translation, and kernel networking initialization path are not linked into the 32-bit target.

There are no separate tracked x86_32 copies of driver source files to delete. Keeping one physical implementation tree avoids duplicate driver sources; the x86_32 source list is limited to drivers whose existing shared sources provide a 32-bit code path.

## GPU Driver

Interface and implementation: `resources/drivers/gpu/driver.hpp` and `resources/drivers/gpu/functions/`. `framebuffer/framebuffer.cpp`, `draw.cpp`, and `text_core.cpp` contain the shared GPU implementation; `framebuffer/multiboot.cpp` adapts the x86_32 Multiboot handoff to the common framebuffer.

The GPU driver is the primary output mechanism for the kernel. It provides text rendering, graphics primitives, and backbuffer management.

### Backbuffer System

The kernel uses **double buffering** to prevent screen tearing:

1. All rendering goes to a **backbuffer** (an in-memory framebuffer)
2. After rendering is complete, `render_frame()` copies the backbuffer to the actual VGA/VRAM

The shared implementation defaults to a 4096×2160 backbuffer. The x86_32 build sets its capacity to 1280×720, matching its Multiboot framebuffer request and avoiding an unnecessarily large fixed-address kernel image. If firmware supplies a larger 32-bpp framebuffer, the 32-bit adapter uses the top-left region that fits the backbuffer instead of failing graphics initialization. `init_backbuffer()` validates the pitch and does not allocate memory dynamically. The 32-bit console presents only the updated character cells (or the full frame when clearing the screen), rather than copying the entire framebuffer for every character.

## Keyboard Driver

The PS/2 controller access, Set 1 scancode translation, modifier state, and polling line input are implemented once in `resources/drivers/keyboard/functions/keyboard.cpp`, which contains no architecture checks and is compiled by both kernels. The 64-bit GUI/interrupt integration lives in `kernel_64bit/src/system/interrupts/keyboard.cpp` and uses the shared `translate_scancode()` implementation. The 32-bit kernel uses the shared polling `read_line()` path.

## Ethernet and Network Drivers (x86_64)

The generic Ethernet interface is declared in `resources/drivers/ethernet/driver.hpp`; the VirtIO-specific PCI transport and virtqueue implementation are isolated in `resources/drivers/ethernet/controllers/virtio-net/`. The network layer in `resources/drivers/network/` owns Ethernet frame dispatch, ARP neighbor resolution, IPv4 and ICMP handling, UDP checksums and ports, TCP client connections, DNS, and DHCP. The bounded TCP client currently backs the NAPP `http_get` service and supports plain HTTP only. DHCP starts after the VirtIO interface initializes and is advanced from the kernel's regular network polling loop. The 32-bit kernel does not compile or initialize these drivers.

The current transport supports the transitional/legacy PCI VirtIO network device (PCI ID `1af4:1000`), which is what QEMU's default `virtio-net-pci` device exposes on the usual `pc` machine. It requires a valid device-supplied MAC and a DMA-contiguous queue/buffer mapping, and reports initialization failure rather than substituting a MAC, network configuration, or simulated DHCP response. The modern-only VirtIO PCI transport (`1af4:1041`) is not implemented.

Build the 64-bit kernel and run it with QEMU user-mode DHCP networking:

```sh
make -C kernel_64bit ARCH=x86_64
make run-x86_64 QEMU_NETDEV="-netdev user,id=net0 -device virtio-net-pci,netdev=net0"
```

The serial log reports device discovery, MAC, queue/link status, DHCP state changes, and the configuration applied from DHCPACK. A successful build alone does not verify that QEMU delivered a DHCP lease; verify the serial `DHCP state BOUND` message and assigned addresses during a VM run.

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

| Function | Signature | Description |
|----------|-----------|-------------|
| `put_pixel(x, y, color)` | `(size_t, size_t, uint32_t)` | Set a single pixel |
| `clear_screen(color)` | `(uint32_t)` | Fill entire screen with a color |
| `fill_block(x, y, color, w, h)` | `(size_t, size_t, uint32_t, size_t, size_t)` | Fill a rectangle |
| `draw_rect(x1, y1, x2, y2, color)` | `(int, int, int, int, uint32_t)` | Draw a rectangle outline (coordinates, not width/height) |

### Shell Command Printing

`print_cmd()` renders text at specific screen coordinates:

```cpp
void print_cmd(const char* str, int x, int y, uint32_t color);
```

Used for shell output messages and UI elements.

### Window Manager

Implementation: `resources/drivers/gpu/functions/64bit_driver/window.cpp`

The window manager handles overlapping GUI windows:

| Constant      | Value |
|---------------|-------|
| `MAX_WINDOWS` | 13    |

| Function                          | Description                         |
|-----------------------------------|-------------------------------------|
| `register_window(window_struct*)` | Register a new window               |
| `unregister_window(window_struct*)` | Remove a window                 |
| `update_windows_positions(int, int)` | Update all window positions      |
| `draw_windows()`                  | Render all windows to backbuffer    |
| `is_mouse_over_any_window(int, int)` | Check if cursor is over a window |

```cpp
typedef struct window_struct
{
    const char* name;
    uint32_t id;

    int pos_x, pos_y;
    int width, height;

    bool visible, minimized, focused;
    bool resizable, can_maximize, maximized;

    int restore_pos_x, restore_pos_y, restore_width, restore_height;

    bool is_dragging;
    int drag_offset_x, drag_offset_y;

    bool is_resizing;
    bool resize_right, resize_bottom;
    int resize_start_mouse_x, resize_start_mouse_y;
    int resize_start_width, resize_start_height;

    int max_width, max_height;

    void* userdata;

    void (*draw_content)(window_struct*);
    void (*key_press)(window_struct*, char);
    void (*mouse_click)(window_struct*, int, int);
    void (*mouse_button)(window_struct*, int, int, int);
} window_struct;
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

Implementation: `resources/drivers/keyboard/`

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

Implementation: `resources/drivers/mouse/`

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

Interface: `resources/drivers/timer/driver.hpp`

Programmable Interval Timer running at 100 Hz:

```cpp
void pit_init();
uint64_t pit_get_ticks();  // Returns tick count (10ms per tick)
void sleep(uint32_t ms);   // Millisecond delay
```

Used for: uptime calculation, sleep/delay, time-based operations
