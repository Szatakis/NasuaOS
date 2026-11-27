# Debugging Guide

Comprehensive guide for debugging NasuaOS, covering kernel panics, common problems, and useful techniques.

## Table of Contents

- [Kernel Panics and Triple Faults](#kernel-panics-and-triple-faults)
- [Safe Mode](#safe-mode)
- [Logging](#logging)
- [Kernel Panic Screen](#kernel-panic-screen)
- [Serial Output (UART)](#serial-output-uart)
- [QEMU Debugging](#qemu-debugging)
- [Common Problems and Solutions](#common-problems-and-solutions)
- [Boot Troubleshooting](#boot-troubleshooting)

---

## Kernel Panics and Triple Faults

### Kernel Panic

A kernel panic is a deliberate halt triggered when the kernel encounters an unrecoverable error. It is invoked via:

```cpp
void kernel_panic(const char* message, const char* error_code, ...);
```

Source: `kernel_64bit/src/kernel/kernel_panic/kernel_panic.cpp`

**Triggering a panic manually** (for testing):
```
user@nasua-os:/home> panic
```

### Panic Screen Layout

When a panic occurs, the system renders a formatted screen with:

| Field        | Description                                 |
|--------------|---------------------------------------------|
| `Reason`     | The panic message describing the failure    |
| `Error Code` | Error code associated with the fault        |
| `RIP`        | Instruction pointer at time of fault        |
| `RSP`        | Stack pointer at time of fault              |
| `Fault Addr` | Virtual address that caused the fault (CR2) |
| `Process ID` | Current process identifier                  |
| `CPU Model`  | CPU model string                            |

The screen also displays a **QR code** linking to the debug instructions at:
`https://github.com/Szatakis/NasuaOS/blob/main/documentation/debug_instructions/debug_instructions.md#kernel_panic`

After displaying the panic screen, the kernel disables interrupts and halts in an infinite loop.

### Triple Faults

A triple fault occurs when the CPU cannot invoke the double fault handler, typically due to:
- GDT misconfiguration
- IDT misconfiguration
- Invalid interrupt return (`iretq`)
- Kernel stack corruption
- Double fault handler crash

A triple fault causes an automatic CPU reset. If the machine reboots instead of showing a panic screen, investigate the interrupt/exception subsystem.

---

## Safe Mode

Safe mode is activated by:

1. Selecting "Safe Mode 64-bit" from the Limine boot menu
2. Adding `SAFE_MODE` to `boot_config.txt`
3. Running `safe_mode` from the shell

```
user@nasua-os:/home> safe_mode
Safe mode ON
```

When safe mode is enabled (`safe_mode = true`):
- The ClawFS overlay starts **unmounted** (commands load from ISO rootfs)
- Some kernel initialization messages are printed via UART
- The `info` command shows "Overlay: Unmounted (commands from ISO)"

Safe mode is designed for troubleshooting filesystem or command loading issues.

---

## Logging

The kernel logger maintains an in-memory circular log buffer (128 entries).

### Log Entry Structure

Each log entry contains:
- **Level**: `INFO`, `WARN`, `ERROR`, `DEBUG`
- **Subsystem**: Component name (e.g., `NAPP`, `File Resolver`, `Storage`, `MOUSE`, `CPU`)
- **Message**: Log message text
- **Timestamp**: RTC time at log insertion

### Log Commands

```
logs --show                           # Display all logs
logs --clear                          # Clear the log buffer
logs --level <INFO|WARN|ERROR|DEBUG>  # Filter by level
logs --subsystem <name>               # Filter by subsystem
logs --put <text>                     # Add a custom INFO log
```

### Common Subsystems

| Subsystem       | Source File                                                          |
|-----------------|----------------------------------------------------------------------|
| `NAPP`          | `kernel_64bit/src/system/applications/napp/napp.cpp`                 |
| `File Resolver` | `kernel_64bit/src/system/filesystem/file_resolver/file_resolver.cpp` |
| `Storage`       | `kernel_64bit/src/system/drivers/disk/ata/functions/storage.cpp`     |
| `MOUSE`         | `kernel_64bit/src/system/drivers/mouse/functions/mouse.cpp`          |
| `CPU`           | `kernel_64bit/src/system/drivers/cpu/functions/cpu.cpp`              |
| `VGA` / `GUI`   | `kernel_64bit/src/system/drivers/gpu/`                               |

---

## Kernel Panic Screen

See the panic screen section above. The QR code encodes a URL pointing to the full debug instructions document (`documentation/debug_instructions/debug_instructions.md`).

---

## Serial Output (UART)

NasuaOS uses UART (COM1, port `0x3F8`) for serial debug output. This is the most reliable output mechanism during early boot and kernel panics.

### Capturing Serial Output

**QEMU:**
```bash
qemu-system-x86_64 -serial file:serial.log -cdrom NasuaOS-x86_64.iso
# or
qemu-system-x86_64 -serial stdio -cdrom NasuaOS-x86_64.iso
```

**QEMU (serial redirect to terminal):**
```bash
qemu-system-x86_64 -serial /dev/ttyS0 -cdrom NasuaOS-x86_64.iso
```

### Key UART Messages

Watch for these messages during boot:

```
[NAPP] Initializing...
[NAPP] Rootfs mounted
[Storage] Disk detected.
[File Resolver] ClawFS overlay mounted.
[MOUSE] PS/2 mouse connected
[CPU] MP available
```

If any of these are missing or show errors, inspect the corresponding subsystem.

---

## QEMU Debugging

### QEMU Command Line

The project's default QEMU invocation (via `make run`) uses:

```bash
qemu-system-x86_64 \
    -M q35 \
    -drive if=pflash,unit=0,format=raw,file=edk2-bins/code-x86_64.fd,readonly=on \
    -cdrom NasuaOS-x86_64.iso \
    -audiodev sdl,id=snd0 \
    -machine pcspk-audiodev=snd0 \
    -m 2G
```

### Useful QEMU Flags

| Flag              | Purpose                                       |
|-------------------|-----------------------------------------------|
| `-d int`          | Log all interrupts to `qemu.log`              |
| `-d cpu_reset`    | Log CPU resets                                |
| `-d guest_errors` | Log guest errors                              |
| `-D qemu.log`     | Redirect QEMU debug output to file            |
| `-no-reboot`      | Exit instead of rebooting on triple fault     |
| `-s`              | Shorthand for `-gdb tcp::1234`                |
| `-S`              | Freeze CPU at startup (use with `-s` for GDB) |

### QEMU Monitor

Press `Ctrl+Alt+G` in the QEMU window to release the mouse. Then use:
- `Ctrl+Alt+2` to enter the QEMU monitor
- `info registers` — View CPU state
- `info mem` — View memory mappings
- `xp /x <count> <addr>` — Examine physical memory

### WSL / VirtualBox

The build system detects WSL automatically. On WSL, QEMU uses the Windows binary at `/mnt/c/Program Files/qemu/qemu-system-x86_64.exe`. See [Debug Instructions](../debug_instructions/debug_instructions.md) for platform-specific considerations.

---

## Common Problems and Solutions

### "Unknown command" after typing a `/sbin` command

**Cause**: ClawFS is not mounted, and the command does not exist in the ISO rootfs `/sbin/`, or the rootfs was not loaded.

**Fix**: Run `mount` to mount ClawFS, or `format --commands` to copy commands to ClawFS first, then `mount`.

### "no commands found in /sbin — rootfs may not be mounted"

**Cause**: The FAT rootfs image was not loaded as a boot module.

**Fix**: Ensure `rootfs.img` is included in the ISO. Run `make clean && make all` and check the build output.

### Commands load from wrong source

**Cause**: The file resolver overlay state is incorrect.

**Fix**: Run `unmount` to force loading from ISO, or `mount` to load from ClawFS. Check `info` output to verify overlay status.

### File not found after `cd` or path operations

**Cause**: Path resolution failed. `/` is the root, `~` returns to `/home`, `..` goes up.

**Fix**: Ensure the path exists. Use absolute paths starting with `/` for clarity.

### Screen flickering or rendering issues

**Cause**: Potential framebuffer pitch mismatch or double-buffering timing issue.

**Fix**: See [Debug Instructions](../debug_instructions/debug_instructions.md#25-backbuffer-and-flickering) for detailed debugging steps.

### Mouse cursor not moving

**Cause**: PS/2 mouse not detected or initialized.

**Fix**: Check UART output for `[MOUSE]` messages. Ensure the PS/2 mouse is connected. Arrow keys (without Shift) provide manual cursor movement in the 64-bit kernel.

### Keyboard not responding

**Cause**: PS/2 keyboard controller issue or scancode handling bug.

**Fix**: Enable debug mode (`debug --on`) and check UART output for scancodes. See the debug instructions for page fault and interrupt handler verification.

---

## Boot Troubleshooting

### System hangs during boot

1. Check that all Limine boot modules are present in the ISO
2. Verify the rootfs image was built: `ls -la utilities/rootfs/rootfs.img`
3. Check serial output for the last successful initialization message

### No framebuffer / screen stays black

1. Verify the Limine configuration in `.config/limine.conf`
2. Check that the resolution in `defaults.txt` matches your display
3. Try booting via BIOS mode: `make run-bios`

### ClawFS overlay not working

1. Run `format` to create the ClawFS filesystem
2. Run `format --commands` to copy commands to ClawFS
3. Run `mount` to enable the overlay
4. Verify with `info`

---

## Debug Instructions Reference

NasuaOS maintains a separate comprehensive debug instruction document at:

`documentation/debug_instructions/debug_instructions.md`

This document covers:
- General debugging rules and methodology
- Page fault analysis (CR2, RIP, CR3, error code decoding)
- PMM / VMM / paging debugging
- Use-after-free detection
- GDT / IDT / CPU exception debugging
- Interrupt debugging
- PCI, USB, ATA, and PS/2 debugging
- GUI / framebuffer debugging
- Platform-specific considerations (QEMU vs VirtualBox vs real hardware)
- Bug report format and root cause analysis requirements
