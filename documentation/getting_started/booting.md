# Booting NasuaOS

## Boot Process Overview

NasuaOS uses the **Limine** bootloader for the 64-bit kernel (UEFI and BIOS) and **Multiboot2** for the 32-bit kernel and boot utilities.

The boot configuration is generated from `.config/defaults.txt` by `config_generator/generate_conf.py`, producing `.config/limine.conf`.

## Boot Menu

On startup, Limine displays the boot menu with two top-level folders,
**Kernel 64-bit** and **Kernel 32-bit**, each containing one entry per
architecture:

| Entry              | Protocol   | Description                                        |
|--------------------|------------|----------------------------------------------------|
| Kernel 64-bit      | —          | Folder: 64-bit kernels                             |
| ├ x86-64           | Limine     | Full 64-bit kernel with graphics                   |
| ├ aarch64          | Limine     | Simple UART shell (work in progress)               |
| ├ riscv64          | Limine     | Simple UART shell (work in progress)               |
| └ loongarch64      | Limine     | Simple UART shell (work in progress)               |
| Kernel 32-bit      | —          | Folder: 32-bit kernels                             |
| ├ x86               | Multiboot2 | Console-only 32-bit kernel                         |
| ├ armv7             | Limine     | Simple UART shell (work in progress)               |
| ├ riscv32           | Limine     | Simple UART shell (work in progress)               |
| └ loongarch32       | Multiboot2 | Simple UART shell (work in progress)               |
| Safe Mode 64-bit   | Limine     | x86-64 kernel with safe mode config                |
| Safe Mode 32-bit   | Multiboot2 | x86-32 kernel with safe mode config                |
| Boot Manager        | Multiboot2 | `boot_mgr_x86`/`boot_mgr_arm`/`boot_mgr_riscv` utilities for kernel selection            |
| Recovery Tool       | Multiboot2 | `recovery_tool` for troubleshooting                |
| ClawFS Explorer     | Multiboot2 | `clawfs_explorer` for browsing ClawFS              |
| Hardware Detection  | Multiboot2 | `hdt` for viewing system info                      |
| Hardware Test       | Multiboot2 | `hdtest` for hardware diagnostics                  |
| Kernel Debugger     | Multiboot2 | `kdebug` for kernel inspection                     |

AArch64 and RISC-V64 boot the ISO through Limine with their matching EDK2
firmware. ARMv7 and RISC-V32 kernels are shipped on the same ISO under
`/boot/kernel_32bit/` with Limine protocol entries, and every `BOOT*.EFI`
loader present in `limine-binary` is copied into `/EFI/BOOT`. QEMU still
loads those 32-bit kernels directly (`make run-aarch32` / `make run-riscv32`)
because the bundled EDK2 set has no 32-bit ARM or RISC-V firmware yet.
Use the QEMU `Serial` tab to type into each experimental shell.

## Boot Modules

The 64-bit kernel receives the following modules from the bootloader:

| Module      | Path                      | Description                                                     |
|-------------|---------------------------|-----------------------------------------------------------------|
| Rootfs      | `/system/rootfs.img`      | FAT image containing `/bin/`, `/sbin/`, and directory structure |
| Boot Config | `/config/boot_config.txt` | Boot-time configuration (may contain `SAFE_MODE`, `DEBUG`)      |
| Defaults    | `/config/defaults.txt`    | System-wide default configuration                               |

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

The **Boot Manager** (`boot_mgr_x86`) is a 32-bit Multiboot2 application (with `boot_mgr_arm`/`boot_mgr_riscv` Limine-protocol variants for ARM/RISC-V) that:
- Detects CPU architecture (x86-64 vs IA-32)
- Selects the appropriate kernel based on CPU capabilities
- Loads kernel modules (rootfs, boot config, defaults)
- Launches the kernel

It can be accessed from the Limine boot menu under **Advanced Options → Boot Manager**.

## Storage Detection at Boot

At initialization, the kernel attempts ATA disk detection first. If no disk is found, a 64 MB RAM disk is created automatically as a fallback. The storage type is displayed via the `info` command.
