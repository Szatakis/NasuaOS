# Building NAPP Applications

NAPP applications are compiled and linked into flat binaries using a cross-compilation toolchain and custom linker script. This guide covers the build process.

## Build Requirements

- x86_64 cross-compiler (`x86_64-elf-gcc` or `gcc` with `-ffreestanding`)
- GNU Binutils (`objcopy`)
- GNU Make

## Build Files

NAPP applications use two key build files:

| File                              | Purpose                                     |
|-----------------------------------|---------------------------------------------|
| `utilities/applications/app.mk`   | Makefile include with build rules and flags |
| `utilities/applications/napp.lds` | Linker script for flat binary output        |

## Compiler Flags

The following flags are used to compile every NAPP application (from `app.mk`):

```makefile
CXXFLAGS = -std=gnu++20 \
           -O2 \
           -Wall \
           -Wextra \
           -ffreestanding \
           -fno-exceptions \
           -fno-rtti \
           -fno-stack-protector \
           -fno-stack-check \
           -fno-lto \
           -fno-plt \
           -fPIC \
           -fvisibility=hidden \
           -m64 \
           -mno-red-zone \
           -mno-mmx \
           -mno-sse \
           -mno-sse2 \
           -mno-80387 \
           -mcmodel=small \
           -I $(INCLUDE_DIR)
```

| Flag                    | Purpose                                                |
|-------------------------|--------------------------------------------------------|
| `-std=gnu++20`          | C++20 with GNU extensions                              |
| `-ffreestanding`        | No standard library                                    |
| `-fno-exceptions`       | Disable C++ exceptions                                 |
| `-fno-rtti`             | Disable runtime type information                       |
| `-fno-lto`              | Disable link-time optimization                         |
| `-fno-plt`              | No procedure linkage table (smaller/faster calls)      |
| `-fPIC`                 | Position-independent code                              |
| `-fvisibility=hidden`   | Hide symbols by default (smaller binaries)             |
| `-m64`                  | Generate 64-bit code                                   |
| `-mno-red-zone`         | Disable x86-64 red zone (interrupts may use the stack) |
| `-mno-mmx/-sse/-sse2`   | Disable SIMD instructions (kernel not using them)      |
| `-mno-80387`            | Disable x87 FPU instructions                           |
| `-mcmodel=small`        | Use small code model (32-bit offsets)                  |
| `-fno-stack-protector`  | No stack canaries (no runtime support)                 |
| `-fno-stack-check`      | No stack probing                                       |
| `-O2`                   | Optimization level 2                                   |

## Linker Script

The NAPP linker script (`utilities/applications/napp.lds`):

```ld
ENTRY(_start)

SECTIONS
{
    . = 0x00000000;

    .napp_header :
    {
        KEEP(*(.napp_header))
        . = ALIGN(64);
    }

    .text :
    {
        KEEP(*(.napp_entry))
        *(.text .text.*)
    }

    .rodata :
    {
        *(.rodata .rodata.*)
    }

    .data :
    {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }
}
```

Key points:
- Output format is `binary` (set by `objcopy -O binary` in `app.mk`)
- The `.napp_header` section is placed first (at offset 0), aligned to 64 bytes
- The `.napp_entry` section (containing the `NAPP_APPLICATION` macro output) is placed at the beginning of `.text`
- Comments, notes, and exception handling sections are discarded

## The NAPP Header Section

Applications place their NAPP header in the `.napp_header` section using the `NAPP_APPLICATION` macro, which automatically emits the header and pins the entry point right behind it. You do not usually need to define the header manually:

```cpp
#include <napp.h>

NAPP_APPLICATION("my_app", "A brief description", false);

int _start(const napp_api* api)
{
    api->print_line("Hello from my app!");
    return 0;
}
```

The macro expands to:

```cpp
struct napp_header
{
    uint32_t magic;                    // 0x5050414E ("NPPA")
    uint32_t abi_version;              // NAPP_ABI_VERSION (3)
    uint32_t header_size;              // sizeof(napp_header) = 97
    uint32_t entry_offset;             // Offset to _start function
    char     name[NAPP_NAME_LENGTH];   // 32 bytes
    char     description[NAPP_DESCRIPTION_LENGTH]; // 48 bytes
    bool     show_in_start_menu;       // 1 byte, present when header_size > NAPP_HEADER_SIZE
} __attribute__((packed));
```

The `show_in_start_menu` field (added in ABI version 3) controls whether the application appears in the Start Menu. Use `true` for GUI apps that should be visible, and `false` for console/utility apps and games that should only be launched on demand.

## Application Makefile

Each application directory can either provide its own `GNUmakefile` or rely on the
shared `app.mk` build rules. The shared `app.mk` handles the full build process
and is used automatically when no custom `GNUmakefile` exists:

```makefile
APP ?= $(notdir $(CURDIR))
TARGET := $(APP).napp
include ../app.mk
```

The `app.mk` file (at `utilities/applications/app.mk`) handles compiling all `.cpp`
files in the application folder (and optional `src/` subfolder), linking with
`napp.lds`, and producing the final `.napp` flat binary.

## Build Output

The build produces a flat binary file:
- **With NAPP header**: `<app_name>.napp` (placed in `/bin/<app_name>` on rootfs — no file extension)
- `/sbin` commands use the same build process and are placed in `/sbin/<command>` on rootfs

## Adding a New Application

### Step 1: Create the application directory

```
utilities/applications/my_app/
├── src/
│   └── my_app.cpp
└── GNUmakefile
```

### Step 2: Write the application

```cpp
#include <napp.h>

NAPP_APPLICATION("my_app", "A brief description of what the app does", false);

int _start(const napp_api* api)
{
    api->print_line("Hello from my app!");
    return 0;
}
```

### Step 3: Create the makefile

```makefile
APP = my_app
include ../app.mk
```

### Step 4: Build automatically

New applications are picked up automatically by the build system. The top-level
`GNUmakefile` iterates every folder in `utilities/applications/` and builds it.
The resulting `.napp` binary is placed in `/bin/<app_name>` on the rootfs.

## Building

```bash
# Build all NAPP applications
make -C utilities/applications

# Build the entire system (kernel + rootfs + ISO)
make all
```

## /sbin Commands Build Process

`/sbin` commands follow the same build process as `/bin` applications (same `CXXFLAGS`,
same `napp.lds` linker script, same `NAPP_APPLICATION` macro). They are placed in
`/sbin/` on the rootfs as flat binary files. The primary difference is their location
on the filesystem — `/bin` for interactive applications and `/sbin` for system commands.

The `/sbin` commands are loaded at runtime via `napp_run_path()` when the shell
receives a command it does not recognize as a built-in. See [Running NAPP
Applications](running.md) for details.
