# NasuaOS Documentation

Welcome to the official documentation for **NasuaOS** — a hobby operating system built from scratch for x86_64 computers (with experimental AArch64, RISC-V, and LoongArch64 support), written primarily in C++.

NasuaOS includes its own kernel, memory manager, drivers, graphical user interface, shell, custom filesystem (ClawFS), and a NAPP application format for user-space programs and system commands.

## Table of Contents

| Category | Description |
|----------|-------------|
| [Getting Started](getting_started/installation.md) | Installation, booting, basic usage, and configuration |
| [Shell & Commands](shell/syntax.md) | Shell syntax, built-in commands, and `/sbin` commands |
| [Debugging](debugging/debug_mode.md) | Debug mode, logs, safe mode, and troubleshooting |
| [Filesystem](filesystem/clawfs.md) | ClawFS, `/bin`, `/sbin`, mounting, and storage behavior |
| [NAPP Applications](napp/format.md) | NAPP format, API, building, and running applications |
| [Writing Your Own Software](software/applications.md) | Creating NAPP applications and `/sbin` system utilities |
| [Kernel Development](kernel/architecture.md) | Kernel structure, drivers, memory management, and hardware access |
| [GUI Development](gui/development.md) | Windows, rendering, input, and graphics |
| [Development Environment](development/project_structure.md) | Project structure, build system, and testing workflow |
| [Contributing](contributing/guidelines.md) | Coding guidelines and Git workflow |

---

## Quick Links

- [Build Instructions](../build_instructions.md)
- [Debug Instructions](debugging/debug_guide.md)
- [Shell Built-in Commands](shell/builtins.md)
- [Shell `/sbin` Commands](shell/sbin_commands.md)
- [ClawFS Filesystem](filesystem/clawfs.md)
- [NAPP Application Format](napp/format.md)
- [NAPP API Reference](napp/api.md)
- [Writing NAPP Applications](software/applications.md)
- [Writing `/sbin` Utilities](software/utilities.md)
- [Kernel Architecture](kernel/architecture.md)
- [Driver Overview](kernel/drivers.md)
- [Memory Management](kernel/memory.md)
- [GUI Development](gui/development.md)
- [Project Structure](development/project_structure.md)
- [Build System](development/build_system.md)
- [Contributing Guidelines](contributing/guidelines.md)

---

## Architecture Overview

NasuaOS uses a **dual-kernel** design:

### kernel_64bit (Full OS)
The primary 64-bit kernel providing:
- Full graphical desktop with window manager
- Built-in shell with 26 built-in commands
- ClawFS filesystem support
- NAPP application and `/sbin` command loading
- USB, PCI, SATA/AHCI, and PS/2 support
- Multi-core support via APIC
- Audio (PC speaker, HDA)

### kernel_32bit (Minimal Console)
A 32-bit fallback kernel for basic console-only operation, using Multiboot2 instead of Limine.

### Boot Utilities
Located in `utilities/boot_options/`, these are 32-bit Multiboot2 applications accessible from the boot menu:

| Utility | Description |
|---------|-------------|
| `boot_mgr` | Boot manager for selecting kernels |
| `clawfs_explorer` | File browser for ClawFS partitions |
| `hdtest` | Hard disk diagnostic tool |
| `hdt` | Hardware detection tool |
| `kdebug` | Kernel debugger |
| `recovery_tool` | Recovery environment |

---

## Key Concepts

### Two-Tier Command System

Commands in NasuaOS are split into two categories:

1. **Built-in Shell Commands** — Compiled directly into the kernel. Available immediately without a disk filesystem (examples: `help`, `echo`, `time`, `format`, `mount`, `debug`, `logs`). See [Shell Built-in Commands](shell/builtins.md).

2. **`/sbin` Commands** — Flat-binary applications loaded dynamically from `/sbin` on the root filesystem. These are **NAPP binaries** placed in the rootfs image and optionally copied to ClawFS. See [Shell `/sbin` Commands](shell/sbin_commands.md) and [NAPP Format](napp/format.md).

The help system documents built-in commands on pages 1–10, and `/sbin` commands on page 11 and above. Run `help --page 1` through `help --page 11` (and beyond) to browse.

### File Resolution: File Resolver

NasuaOS uses a **file resolver** mechanism that provides an overlay between the ISO's read-only root filesystem (FAT) and the writable ClawFS partition on disk:

- When ClawFS is **unmounted**, commands and files load from the ISO (FAT rootfs).
- When ClawFS is **mounted** (via the `mount` command), the overlay is active: ClawFS takes priority, falling back to the ISO rootfs.
- The `format --commands` command copies `/bin` and `/sbin` from the ISO rootfs to ClawFS.
- Deleted files in ClawFS are tracked via **tombstones** (stored at sector 1024) so they can be hidden from the ISO fallback.

See [Filesystem - Mounting](filesystem/mounting.md) for details.

---

## Version Information

- **System Version**: NasuaOS 0.8.0
- **Kernel Version**: 0.3.0 (64-bit)
- **NAPP ABI Version**: 2

---

*This documentation is maintained alongside the [NasuaOS repository](https://github.com/Szatakis/NasuaOS).*
