# NAPP Format

NAPP (Nasua Application) is the executable binary format used by NasuaOS for all applications and system commands. NAPP files are flat binaries with a custom header.

## NAPP Header

Every NAPP binary begins with a header defined in `utilities/applications/include/napp.h`:

| Offset | Size | Field              | Description                                             |
|--------|------|--------------------|---------------------------------------------------------|
| 0x00   | 4    | magic              | `NAPP_MAGIC` = `0x5050414E` (ASCII: "NAPP")             |
| 0x04   | 4    | abi_version        | `13` (current ABI version)                              |
| 0x08   | 4    | header_size        | Aligned header size (448 bytes in ABI 13)               |
| 0x0C   | 4    | entry_offset       | Offset from start of file to `_start` function          |
| 0x10   | 32   | name               | Null-terminated application name                        |
| 0x30   | 48   | description        | Null-terminated short description (shown in `help`)     |
| 0x60   | 1    | show_in_start_menu | Pins the app in ABI 13; visibility in older ABIs         |
| 0x61   | 256  | help_text          | Optional command usage and argument details             |
| 0x161  | 1    | user_mode          | ABI 12: whether the app runs in user mode                |
| 0x162  | 32   | category           | ABI 13: optional Start Menu category                     |

The ABI 13 header is 386 bytes; `header_size` and `entry_offset` are rounded
up to the next 64-byte boundary. Older ABI 6-12 binaries remain supported.

```cpp
#define NAPP_NAME_LENGTH 32
#define NAPP_DESCRIPTION_LENGTH 48
#define NAPP_HELP_TEXT_LENGTH 256
#define NAPP_CATEGORY_LENGTH 32
#define NAPP_HEADER_SIZE 96  // Legacy minimum (without show_in_start_menu)

struct napp_header
{
    uint32_t magic;                    // 0x5050414E
    uint32_t abi_version;              // 4
    uint32_t header_size;              // Aligned size of the header (384 in ABI 8+)
    uint32_t entry_offset;
    char     name[NAPP_NAME_LENGTH];   // 32
    char     description[NAPP_DESCRIPTION_LENGTH];  // 48
    bool     show_in_start_menu;       // ABI 13: pin in Start Menu
    char     help_text[NAPP_HELP_TEXT_LENGTH]; // Optional command help
    bool     user_mode;                // ABI 12
    char     category[NAPP_CATEGORY_LENGTH]; // ABI 13
} __attribute__((packed));
```

### Start Menu Pin and Category

For ABI 13, `show_in_start_menu` is the pin flag: pinned apps appear in the
Start Menu's Pinned view. Any app with a non-empty `category` also appears
under that category (for example, `Games` or `Utilities`). Apps with neither
a pin nor a category are omitted.

Before ABI 13 this field controlled visibility, so old binaries retain their
original Start Menu behavior. Older headers without the field default to
pinned.

## NAPP vs Flat Binary

| File Type      | Header            | Usage                        |
|----------------|-------------------|------------------------------|
| `/bin/<app>`   | Yes (NAPP header) | GUI/interactive applications |
| `/sbin/<cmd>`  | Yes (NAPP header) | System commands              |

Both `/bin` and `/sbin` binaries are built using the same toolchain
(`NAPP_APPLICATION` macro, `napp.lds` linker script, and `app.mk` rules) and
contain a valid NAPP header. The only difference is their location on the
filesystem and their purpose. The loader checks the magic value to validate
the header. If the magic does not match, the binary is treated as a flat
binary (entry point at offset 0) for backward compatibility.

## Magic Value

```cpp
#define NAPP_MAGIC 0x5050414E
```

The loader checks this magic value to verify the file is a valid NAPP binary. If the magic does not match, the binary is rejected.

## ABI Version

```cpp
#define NAPP_ABI_VERSION 13
```

The current NAPP ABI version is 13. The loader supports ABI versions 6 through
13. ABI version 13 adds Start Menu categories; ABI version 12 adds user-mode
applications; ABI version 11 adds colored output redirection; ABI version 10
adds process snapshots and system information; ABI version 9 adds username
lookup; ABI version 8 adds optional command help text to the header. ABI
version 7 adds the Start Menu visibility field.
ABI version 4 added the `http_get`
service for plain HTTP requests over the kernel's DNS and TCP network stack.
ABI version 5 added complete-file ClawFS read and write services. ABI version 6
added the `https_get` service with TLS 1.2 and certificate verification.
ABI version 3 introduced the `set_print_redirect` function in the `napp_api`
struct.

## Entry Point

NAPP applications do not use the standard ELF entry point mechanism. Instead:

1. The NAPP loader reads the header to find `entry_offset`
2. It allocates memory and loads all sections
3. It calls the entry function as:

```cpp
int _start(const napp_api* api);
```

The `_start` function receives a pointer to a `napp_api` structure that provides all system services (I/O, filesystem, GUI, etc.).

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
    napp_window_tick tick;       // Optional periodic callback
    int tick_interval_ms;        // Interval for tick callback (100 Hz kernel)
};
```

### Tick Callback

The `tick` callback is invoked by the kernel at `tick_interval_ms` intervals while
the window is visible. When `tick` is `nullptr` or `tick_interval_ms <= 0`, no
periodic callback is registered.

```cpp
typedef void (*napp_window_tick)(struct napp_window* window);
```

### Monotonic Tick Counter

The `napp_api` struct provides a `get_ticks` function pointer that returns the
number of PIT timer ticks since boot (100 Hz, i.e. 10 ms per tick):

```cpp
uint64_t (*get_ticks)(void);
```

This is useful for measuring elapsed time inside tick callbacks or animation
timing logic.

## Application Name Macro

All NAPP applications declare their name using the `NAPP_APPLICATION` macro:

```cpp
NAPP_APPLICATION("my_app", "Short description of the app", true);
```

The optional fourth argument contains command usage and argument descriptions.
For example:

```cpp
NAPP_APPLICATION("cat", "Display the contents of one or more files", false,
                 "    --file <filename> - File to display\n");
```

The macro takes these arguments:
- **`app_name`** — The application name (max 32 chars)
- **`app_description`** — A short description shown in the help system (max 48 chars)
- **`app_show_in_menu`** — Whether the application appears in the Start Menu (`true` for GUI apps shown in the menu, `false` for console/utility apps and games)
- **`app_help_text`** — Optional command usage and argument details (max 255 chars); read from the NAPP header by shell help

The macro emits the NAPP header and declares the application entry point. The name should match the directory and binary name in `/bin`.

## Color Constants

NAPP applications use these color constants for GUI rendering:

```cpp
#define NAPP_COLOR_BLACK      0xFF000000
#define NAPP_COLOR_WHITE      0xFFFFFFFF
#define NAPP_COLOR_GREEN      0xFF59E81B
#define NAPP_COLOR_GRAY       0xFFAAAAAA
#define NAPP_COLOR_WINDOW     0xFF1A1F2C
#define NAPP_COLOR_TITLEBAR   0xFF2B3140
```

Colors are in `0xAARRGGBB` format (alpha, red, green, blue, each 8 bits).

## Application Directory Structure

NAPP applications on the filesystem are stored as flat binary files (with a NAPP
header) in the rootfs:

```
/bin/suaedit         # NAPP binary
/bin/calculator      # NAPP binary
/bin/minesweeper     # NAPP binary
/bin/snake           # NAPP binary
/bin/bootcheck       # NAPP binary
```

When ClawFS is mounted, the same flat binary files are stored on the ClawFS
partition and shadow the read-only ISO rootfs copies.

## File Extensions

| Path            | Description                             |
|-----------------|-----------------------------------------|
| `/bin/<app>`    | NAPP binary (with NAPP header)          |
| `/sbin/<cmd>`   | NAPP binary (with NAPP header)          |

The `bootapp` command and shell recognize both `/bin` and `/sbin` paths.
The loader checks the NAPP magic value to validate the header; if the magic
does not match, the binary is treated as a flat binary (entry at offset 0).
