# Development - Project Structure

This document describes the project layout of NasuaOS.

## Root Directory

```
NasuaOS/
├── GNUmakefile                  # Top-level build system
├── kernel_64bit/                # 64-bit kernels, one directory per architecture
├── kernel_32bit/                # 32-bit kernels, one directory per architecture
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

## kernel_64bit/ — 64-bit Kernels

Each architecture has its own directory:

```
kernel_64bit/
├── GNUmakefile                 # Builds every architecture below
├── x86/                        # x86_64 — full NasuaOS kernel
│   ├── GNUmakefile
│   ├── src/                    # Full kernel + userspace sources
│   ├── linker-scripts/         # Per-arch linker scripts
│   ├── freestanding-c-hdrs/    # Freestanding C headers (per arch)
│   ├── freestanding-cxx-hdrs/  # Freestanding C++ headers (per arch)
│   ├── cc-runtime/             # Compiler runtime sources
│   ├── limine-protocol/        # Limine boot protocol headers
│   └── bin-x86_64/             # Build output
├── arm/                        # aarch64 — simple UART shell (WIP)
├── risc-v/                     # riscv64 — simple UART shell (WIP)
└── longarch/                   # loongarch64 — simple UART shell (WIP)

The non-x86 directories each contain a minimal `GNUmakefile`,
`src/start.S` (architecture entry point) and `src/kernel.c`
(UART shell with `help`, `uname`, `echo`, `about` commands).
Built kernels are copied into the ISO under
`/boot/kernel_64bit/<arch>/kernel`.

## kernel_32bit/ — 32-bit Kernels

Same layout as `kernel_64bit/`: `x86/` holds the full Multiboot2
kernel, while `arm/` (armv7), `risc-v/` (riscv32) and `longarch/`
(loongarch32) contain simple UART shells.

```
kernel_32bit/
├── GNUmakefile                 # Builds every architecture below
├── x86/                        # x86_32 — console kernel
│   ├── src/
│   │   ├── boot.asm            # Multiboot2 header
│   │   ├── kernel.cpp          # kmain()
│   │   └── applications/
│   │       └── shell/          # Limited shell (8 commands)
│   └── linker-scripts/
├── arm/                        # armv7 — simple UART shell (WIP)
├── risc-v/                     # riscv32 — simple UART shell (WIP)
└── longarch/                   # loongarch32 — simple UART shell (WIP)
```


## utilities/

```
utilities/
├── applications/
│   ├── app.mk                 # Shared NAPP build rules
│   ├── napp.lds              # NAPP linker script
│   ├── include/
│   │   └── napp.h            # NAPP public API header
│   ├── settings/             # NAPP system settings application
│   ├── task_manager/         # NAPP process viewer
│   ├── terminal/             # NAPP graphical shell terminal
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
| `/sbin/download` | `utilities/system_functions/download/` |
| `/sbin/ls`    | `utilities/system_functions/ls/`    |
| `/sbin/mkdir` | `utilities/system_functions/mkdir/` |
| `/sbin/mv`    | `utilities/system_functions/mv/`    |
| `/sbin/pwd`   | `utilities/system_functions/pwd/`   |
| `/sbin/rm`    | `utilities/system_functions/rm/`    |
| `/sbin/tree`  | `utilities/system_functions/tree/`  |
| `/sbin/wc`    | `utilities/system_functions/wc/`    |

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

## Application Sources

| Application  | Source                                        |
|--------------|-----------------------------------------------|
| Shell        | `kernel_64bit/src/applications/shell/`        |
| Settings     | `utilities/applications/settings/`            |
| Terminal     | `utilities/applications/terminal/`            |
| Task Manager | `utilities/applications/task_manager/`        |
