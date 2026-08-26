# Development - Project Structure

This document describes the project layout of NasuaOS.

## Root Directory

```
NasuaOS/
├── GNUmakefile                  # Top-level build system
├── kernel_64bit/                # 64-bit kernel source (primary)
├── kernel_32bit/                # 32-bit kernel source (legacy)
├── utilities/                   # User-space tools and rootfs
├── .config/                     # System configuration
├── documentation/               # This documentation
├── .github/                     # CI/CD workflows
├── CONTRIBUTING.md              # Contribution guidelines
├── SECURITY.md                  # Security policy
├── README.md                    # Project overview
├── AGENTS.md                    # Agent instructions
└── limine/                      # Limine bootloader (submodule or copy)
```

## kernel_64bit/ — 64-bit Kernel

```
kernel_64bit/
├── GNUmakefile                 # Kernel build rules
├── src/
│   ├── kernel/
│   │   └── kernel.cpp          # kmain() — kernel entry point
│   │   └── kernel_panic/
│   │       ├── kernel_panic.cpp    # Panic screen + QR code
│   │       └── kernel_panic.hpp
│   ├── applications/
│   │   ├── shell/              # Shell: commands.cpp, terminal.cpp, etc.
│   │   ├── settings/
│   │   ├── terminal/
│   │   └── task_manager/
│   ├── system/
│   │   ├── drivers/
│   │   │   ├── gpu/            # Graphics, text, windows, framebuffer
│   │   │   ├── keyboard/       # PS/2 keyboard
│   │   │   ├── mouse/          # PS/2 mouse
│   │   │   ├── rtc/            # Real-time clock
│   │   │   ├── timer/          # PIT
│   │   │   ├── disk/           # ATA
│   │   │   ├── cpu/            # CPU detection
│   │   │   ├── pci/
│   │   │   ├── usb/
│   │   │   ├── ethernet/
│   │   │   ├── wifi/
│   │   │   ├── audio/
│   │   │   ├── acpi/
│   │   │   ├── apic/
│   │   │   └── hpet/
│   │   ├── filesystem/
│   │   │   ├── clawfs/         # Native filesystem
│   │   │   ├── fat/            # FAT driver for ISO rootfs
│   │   │   └── file_resolver/  # Overlay logic
│   │   ├── applications/
│   │   │   └── napp/           # NAPP application loader
│   │   ├── interrupts/         # IDT, ISR, exceptions
│   │   ├── memory/             # Paging, PMM, VMM
│   │   ├── libs/
│   │   │   ├── libc/           # Minimal C library
│   │   │   └── asm/            # Inline assembly helpers
│   │   ├── process/            # ELF loading
│   │   ├── scheduler/
│   │   ├── syscalls/
│   │   └── sysfunc/
│   │       ├── logger/
│   │       ├── command_history/
│   │       ├── info_vars/
│   │       └── serial/
│   ├── config/                 # Kernel compile-time config
│   ├── linker/                 # Linker scripts
│   └── bootloader/             # Boot-related code
└── include/
│   └── kernel.hpp              # Public kernel header
```

## kernel_32bit/ — 32-bit Kernel (Legacy)

```
kernel_32bit/
├── src/
│   ├── boot.asm                # Multiboot2 header
│   ├── kernel.cpp              # kmain()
│   ├── system/
│   │   ├── filesystem/
│   │   │   └── clawfs/
│   │   └── drivers/
│   │       └── video/
│   └── applications/
│       └── shell/
│           └── shell.cpp       # Limited shell (7 commands)
└── Makefile
```

## utilities/

```
utilities/
├── applications/
│   ├── app.mk                 # Shared NAPP build rules
│   ├── napp.lds              # NAPP linker script
│   ├── include/
│   │   └── napp.h            # NAPP public API header
│   ├── bootcheck/
│   │   ├── src/
│   │   │   └── bootcheck.cpp
│   │   └── GNUmakefile
│   ├── calculator/
│   │   ├── src/
│   │   │   └── calculator.cpp
│   │   └── GNUmakefile
│   ├── minesweeper/
│   │   ├── src/
│   │   │   └── minesweeper.cpp
│   │   └── GNUmakefile
│   ├── snake/
│   │   ├── src/
│   │   │   └── snake.cpp
│   │   └── GNUmakefile
│   ├── suaedit/
│   │   ├── src/
│   │   │   └── suaedit.cpp
│   │   └── GNUmakefile
│   └── <other_apps>/
├── rootfs/
│   ├── dir/                  # Root filesystem image sources
│   │   ├── bin/              # NAPP apps (flat binaries)
│   │   ├── sbin/             # System command flat binaries
│   │   ├── home/
│   │   └── ...
│   ├── GNUmakefile           # Builds rootfs.img (FAT16, 8MB)
│   └── rootfs.img            # Output: FAT filesystem image
├── boot_options/             # 32-bit boot utilities
│   ├── boot_manager/
│   ├── recovery_tool/
│   └── clawfs_explorer/
└── config_generator/         # Limine config generation
```

## .config/

```
.config/
├── defaults.txt              # System configuration (10 sections)
├── config_generator/
│   ├── generate_conf.py      # Generates limine.conf from defaults
│   └── limine.conf.template
└── config/                   # Generated config (output)
```

## documentation/

```
documentation/
├── main_page.md              # Central entry point
├── build_instructions.md     # Build instructions (existing)
├── debug_instructions/       # Debug instructions (existing)
│   └── debug_instructions.md
├── getting_started/
│   ├── installation.md
│   ├── booting.md
│   ├── basic_usage.md
│   └── configuration.md
├── shell/
│   ├── syntax.md
│   ├── builtins.md
│   └── sbin_commands.md
├── debugging/
│   ├── debug_mode.md
│   └── debug_guide.md
├── filesystem/
│   ├── clawfs.md
│   ├── mounting.md
│   └── directories.md
├── napp/
│   ├── format.md
│   ├── api.md
│   ├── building.md
│   └── running.md
├── software/
│   ├── applications.md
│   └── utilities.md
├── kernel/
│   ├── architecture.md
│   ├── drivers.md
│   └── memory.md
├── gui/
│   └── development.md
├── development/
│   ├── project_structure.md
│   ├── build_system.md
│   └── testing.md
└── contributing/
    └── guidelines.md
```

## /sbin Commands Source

| Command       | Source File                         |
|---------------|-------------------------------------|
| `/sbin/cat`   | `utilities/system_functions/cat/`   |
| `/sbin/cd`    | `utilities/system_functions/cd/`    |
| `/sbin/cp`    | `utilities/system_functions/cp/`    |
| `/sbin/ls`    | `utilities/system_functions/ls/`    |
| `/sbin/mkdir` | `utilities/system_functions/mkdir/` |
| `/sbin/mv`    | `utilities/system_functions/mv/`    |
| `/sbin/pwd`   | `utilities/system_functions/pwd/`   |
| `/sbin/rm`    | `utilities/system_functions/rm/`    |
| `/sbin/tree`  | `utilities/system_functions/tree/`  |

## NAPP Application Sources (/bin)

| Application     | Source                                              |
|-----------------|-----------------------------------------------------|
| Calculator      | `utilities/applications/calculator/src/`            |
| Bootcheck       | `utilities/applications/bootcheck/src/`             |
| Minesweeper     | `utilities/applications/minesweeper/src/`           |
| Snake           | `utilities/applications/snake/src/`                 |
| SuaEdit         | `utilities/applications/suaedit/src/`               |

## Configuration File Sources

| File                                | Purpose                                                               |
|-------------------------------------|-----------------------------------------------------------------------|
| `.config/defaults.txt`              | All system configuration (hostname, resolution, debug, storage, etc.) |
| `GNUmakefile`                       | Build system configuration (architecture, QEMU flags)                 |
| `limine.conf.template`              | Boot menu template                                                    |
| `config_generator/generate_conf.py` | Config-to-boot-config generator                                       |

## Built-in Application Sources

| Application  | Source                                        |
|--------------|-----------------------------------------------|
| Shell        | `kernel_64bit/src/applications/shell/`        |
| Settings     | `kernel_64bit/src/applications/settings/`     |
| Terminal     | `kernel_64bit/src/applications/terminal/`     |
| Task Manager | `kernel_64bit/src/applications/task_manager/` |
