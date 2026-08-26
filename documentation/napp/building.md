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

The following flags are used to compile every NAPP application:

```makefile
CXXFLAGS = -std=gnu++20 \
           -ffreestanding \
           -nostdlib \
           -fPIC \
           -mno-red-zone \
           -mno-mmx \
           -mno-sse \
           -mno-sse2 \
           -c \
           -fno-stack-protector \
           -fno-stack-check \
           -Wall \
           -Wextra \
           -O2 \
           -I$(APPS_ROOT)/include
```

| Flag                   | Purpose                                                |
|------------------------|--------------------------------------------------------|
| `-std=gnu++20`         | C++20 with GNU extensions                              |
| `-ffreestanding`       | No standard library                                    |
| `-nostdlib`            | No standard library linking                            |
| `-fPIC`                | Position-independent code                              |
| `-mno-red-zone`        | Disable x86-64 red zone (interrupts may use the stack) |
| `-mno-mmx/-sse/-sse2`  | Disable SIMD instructions (kernel not using them)      |
| `-fno-stack-protector` | No stack canaries (no runtime support)                 |
| `-O2`                  | Optimization level 2                                   |

## Linker Script

The NAPP linker script (`napp.lds`):

```ld
OUTPUT_FORMAT(binary)
ENTRY(_start)
SECTIONS
{
    . = 0x00000000;
    .napp_header :
    {
        *(.napp_header)
    }
    .text :
    {
        *(.text)
    }
    .rodata :
    {
        *(.rodata)
    }
    .data :
    {
        *(.data)
    }
    .bss :
    {
        *(.bss)
    }
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gcc_except_table)
        *(.interp)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.rela.dyn)
        *(.rela.plt)
    }
}
```

Key points:
- Output format is `binary` (flat binary, no ELF wrapper)
- The `.napp_header` section is placed first (at offset 0)
- The entry point is `_start`
- Comments, notes, and exception handling sections are discarded

## The NAPP_HEADER Section

Applications place their NAPP header in the `.napp_header` section using a special attribute:

```cpp
__attribute__((section(".napp_header")))
const napp_header _napp_header = {
    .magic          = NAPP_MAGIC,
    .abi_version    = NAPP_ABI_VERSION,
    .header_size    = sizeof(napp_header),
    .entry_offset   = (uint32_t)&_start - (uint32_t)&_napp_header,
    .code_size      = 0,
    .rodata_size    = 0,
    .data_size      = 0,
    .bss_size       = 0,
    .name           = "app_name"
};
```

## Application Makefile

Each application directory includes `app.mk` from its Makefile:

```makefile
APP_NAME = calculator
include ../../../applications/app.mk
```

The `app.mk` file handles the full build process and sets the output path.

## Build Output

The build produces a flat binary file:
- **With NAPP header**: `<app_name>.napp` (placed in `/bin/<app_name>/` on rootfs)
- **Without NAPP header** (flat binary): for `/sbin` commands (placed in `/sbin/` on rootfs)

## Adding a New Application

### Step 1: Create the application directory

```
utilities/applications/my_app/
├── my_app.cpp
└── makefile
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
APP_NAME = my_app
include ../../../applications/app.mk
```

### Step 4: Add to rootfs structure

Create the directory `utilities/rootfs/dir/bin/my_app/` with a `.clawfs` marker file, and add `my_app` to the rootfs GNUmakefile build.

## Building

```bash
# Build all NAPP applications
make -C utilities/applications

# Build the entire system (kernel + rootfs + ISO)
make all
```

## /sbin Commands Build Process

`/sbin` commands follow the same build process but are compiled without the NAPP header (flat binary format):

1. Compiled with the same `CXXFLAGS` from `app.mk`
2. Linked with `napp.lds` but without the `.napp_header` section
3. Output is a raw flat binary placed in `/sbin/` on rootfs
4. Loaded directly into memory at offset 0 by the NAPP loader

The `entry_offset` field in the NAPP header for `/sbin` commands is 0 (since there is no header — the binary starts directly at the entry code).
