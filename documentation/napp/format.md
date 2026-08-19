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

**Total header size**: 96 bytes

```cpp
#define NAPP_NAME_LENGTH 32
#define NAPP_DESCRIPTION_LENGTH 48
#define NAPP_HEADER_SIZE 96

struct napp_header
{
    uint32_t magic;                    // 0x5050414E
    uint32_t abi_version;              // 2
    uint32_t header_size;              // 96
    uint32_t entry_offset;
    char     name[NAPP_NAME_LENGTH];   // 32
    char     description[NAPP_DESCRIPTION_LENGTH];  // 48
} __attribute__((packed));
```

### Description Field

The `description` field stores a short human-readable description of the application. It is read by the shell's `help` command (pages 11+) when listing `/sbin` commands. If a binary lacks a valid description (e.g., compiled with an older header), the fallback text `"No description available."` is displayed.

### Backward Compatibility

Older NAPP binaries compiled with the previous header format (`NAPP_HEADER_SIZE = 64`) are still executable. The loader checks `header->header_size >= sizeof(napp_header)` (96) to determine whether the description field is present. Binaries with `header_size = 64` are detected as legacy and receive the default description.

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

## Application Name Macro

All NAPP applications declare their name using the `NAPP_APPLICATION` macro:

```cpp
NAPP_APPLICATION("my_app", "Short description of the app");
```

The macro takes two arguments:
- **`app_name`** — The application name (max 32 chars)
- **`app_description`** — A short description shown in the help system (max 48 chars)

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
