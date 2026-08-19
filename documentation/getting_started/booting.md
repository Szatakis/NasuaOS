# Booting NasuaOS

## Boot Process Overview

NasuaOS uses the **Limine** bootloader for the 64-bit kernel (UEFI and BIOS) and **Multiboot2** for the 32-bit kernel and boot utilities.

The boot configuration is generated from `.config/defaults.txt` by `config_generator/generate_conf.py`, producing `.config/limine.conf`.

## Boot Menu

On startup, Limine displays the boot menu with the following entries:

| Entry | Protocol | Description |
|-------|----------|-------------|
| NasuaOS 32-bit | Multiboot2 | Console-only 32-bit kernel |
| NasuaOS 64-bit | Limine | Full 64-bit kernel with graphics (recommended) |
| Safe Mode 32-bit | Multiboot2 | 32-bit kernel with `SAFE_MODE` flag |
| Safe Mode 64-bit | Limine | 64-bit kernel with `SAFE_MODE` flag |
| Boot Manager | Multiboot2 | `boot_mgr` utility for kernel selection |
| Recovery Tool | Multiboot2 | `recovery_tool` for troubleshooting |
| ClawFS Explorer | Multiboot2 | `clawfs_explorer` for browsing ClawFS |
| Hardware Detection Tool | Multiboot2 | `hdt` for viewing system info |
| Hardware Test | Multiboot2 | `hdtest` for hardware diagnostics |
| Kernel Debugger | Multiboot2 | `kdebug` for kernel inspection |

## Boot Modules

The 64-bit kernel receives the following modules from the bootloader:

| Module | Path | Description |
|--------|------|-------------|
| Rootfs | `/system/rootfs.img` | FAT image containing `/bin/`, `/sbin/`, and directory structure |
| Boot Config | `/config/boot_config.txt` | Boot-time configuration (may contain `SAFE_MODE`, `DEBUG`) |
| Defaults | `/config/defaults.txt` | System-wide default configuration |

## Boot Configuration Flags

The boot config files (`boot_config.txt`, `boot_config_sf.txt`) can contain keywords that modify boot behavior:

- `SAFE_MODE` — Enables safe mode at boot
- `DEBUG` — Enables debug mode at boot

If no boot config matches, the system boots normally with default settings.

## Booting in QEMU

### ISO Mode (default)
```bash
make run
```
This builds and launches NasuaOS with the ISO as a CD-ROM, using the Limine bootloader.

### HDD Mode
```bash
make run-hdd
```
Boots from the HDD image as a virtual hard disk.

### BIOS Mode
```bash
make run-bios       # ISO via BIOS
make run-hdd-bios   # HDD via BIOS
```

## Booting on Real Hardware

1. Write the ISO to a USB drive or CD:
   ```bash
   # USB (Linux)
   sudo dd if=NasuaOS-x86_64.iso of=/dev/sdX bs=4M status=progress && sync
   ```
2. Boot from USB/CD in UEFI or BIOS mode.

## Boot Manager

The **Boot Manager** (`boot_mgr`) is a 32-bit Multiboot2 application that:
- Detects CPU architecture (x86-64 vs IA-32)
- Selects the appropriate kernel based on CPU capabilities
- Loads kernel modules (rootfs, boot config, defaults)
- Launches the kernel

It can be accessed from the Limine boot menu under **Advanced Options → Boot Manager**.

## Storage Detection at Boot

At initialization, the kernel attempts ATA disk detection first. If no disk is found, a 64 MB RAM disk is created automatically as a fallback. The storage type is displayed via the `info` command.
