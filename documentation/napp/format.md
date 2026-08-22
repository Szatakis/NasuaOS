# NAPP Format

NAPP (Nasua Application) is the executable binary format used by NasuaOS for all applications and system commands. NAPP files are flat binaries with a custom header.

## NAPP Header

Every NAPP binary begins with a header defined in `utilities/applications/include/napp.h`:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | magic | `NAPP_MAGIC` = `0x5050414E` (ASCII: "NPPA") |
| 0x04 | 4 | abi_version | `2` (current ABI version) |
| 0x08 | 4 | header_size | Size of the header (96 bytes) |
| 0x0C | 4 | entry_offset | Offset from start of file to `_start` function |
| 0x10 | 32 | name | Null-terminated application name |
| 0x30 | 48 | description | Null-terminated short description (shown in `help`) |
| 0x60 | 1 | show_in_start_menu | Whether the app appears in the Start Menu |

**Total header size**: 97 bytes (was 96; the `show_in_start_menu` field was added)

```cpp
#define NAPP_NAME_LENGTH 32
#define NAPP_DESCRIPTION_LENGTH 48
#define NAPP_HEADER_SIZE 96  // Legacy minimum (without show_in_start_menu)

struct napp_header
{
    uint32_t magic;                    // 0x5050414E
    uint32_t abi_version;              // 2
    uint32_t header_size;              // sizeof(napp_header) = 97
    uint32_t entry_offset;
    char     name[NAPP_NAME_LENGTH];   // 32
    char     description[NAPP_DESCRIPTION_LENGTH];  // 48
    bool     show_in_start_menu;       // 1 byte, present when header_size > 96
} __attribute__((packed));
```

### Show In Start Menu Field

The `show_in_start_menu` field controls whether an application appears in the
Start Menu.  When `true`, the application icon and name are shown in the Start
Menu.  When `false`, the application can still be launched via `bootapp --app
<name>` but will not appear in the Start Menu.

This field is only present when `header_size > NAPP_HEADER_SIZE` (96).  Older
binaries compiled without this field default to `true`, so they remain visible
in the Start Menu.

## NAPP vs Flat Binary

| File Type | Header | Usage |
|-----------|--------|-------|
| `.napp` | Yes (NAPP header) | GUI/interactive applications loaded from `/bin` |
| Flat binary | Yes (NAPP header) | `/sbin` system commands |

Both types are NAPP executables with a valid `NAPP_HEADER`. The `.napp` format is used for `/bin` applications, while `/sbin` commands are flat binaries that also contain an NAPP header (but without the `.napp` extension). The loader checks the magic value to validate the header.

## Magic Value

```cpp
#define NAPP_MAGIC 0x5050414E
```

The loader checks this magic value to verify the file is a valid NAPP binary. If the magic does not match, the binary is rejected.

## ABI Version

```cpp
#define NAPP_ABI_VERSION 2
```

The current NAPP ABI version is 2. The loader validates this field and rejects binaries with an incompatible version.

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

The macro takes three arguments:
- **`app_name`** — The application name (max 32 chars)
- **`app_description`** — A short description shown in the help system (max 48 chars)
- **`app_show_in_menu`** — Whether the application appears in the Start Menu (`true` for GUI apps shown in the menu, `false` for console/utility apps and games)

This macro sets up the `_napp_name` symbol used by the loader. The name should match the directory and binary name in `/bin`.

## Color Constants

NAPP applications use these color constants for GUI rendering:

```cpp
#define NAPP_COLOR_BLACK      0xFF000000
#define NAPP_COLOR_WHITE      0xFFFFFFFF
#define NAPP_COLOR_RED        0xFFFF0000
#define NAPP_COLOR_GREEN      0xFF00FF00
#define NAPP_COLOR_BLUE       0xFF0000FF
#define NAPP_COLOR_GRAY       0xFF808080
#define NAPP_COLOR_DARK_GRAY  0xFF404040
#define NAPP_COLOR_LIGHT_GRAY 0xFFC0C0C0
```

Colors are in `0xAARRGGBB` format (alpha, red, green, blue, each 8 bits).

## Application Directory Structure

NAPP applications on the filesystem follow this structure:

```
/bin/<app_name>/
├── .clawfs              # Marker file
└── <app_name>.napp      # The NAPP binary
```

When stored on ClawFS:

```
ClawFS:/
├── bin/
│   ├── calculator/
│   │   └── calculator.napp
│   ├── bootcheck/
│   │   └── bootcheck.napp
│   └── <app>/
│       └── <app>.napp
```

## File Extensions

| Extension | Description |
|-----------|-------------|
| `.napp` | NAPP application binary (with header) |
| (no extension) | `/sbin` command flat binary (no header) |

The `bootapp` command and shell recognize both formats.
