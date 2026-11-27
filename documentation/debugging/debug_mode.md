# Debug Mode

NasuaOS provides multiple debugging mechanisms: runtime debug mode, serial port output, kernel logs, and safe mode.

## Enabling Debug Mode

Debug mode can be enabled in three ways:

| Method            | How                              |
|-------------------|----------------------------------|
| **Boot config**   | Add `DEBUG` to `boot_config.txt` |
| **Shell command** | Run `debug --on`                 |
| **Command-line**  | Pass `DEBUG_BUILD=true` to make  |

```
user@nasua-os:/home> debug --on
Debug mode enabled.
```

```
user@nasua-os:/home> debug --off
Debug mode disabled.
```

## What Debug Mode Does

When debug mode is enabled (`debug_mode = true` in kernel.cpp):

1. **Keyboard scancode tracing** — Every keyboard scancode is printed to the serial port (UART) as `SC: 0xXX`
2. **Additional logging** — The kernel emits more verbose log messages via the logger

Debug mode is checked in the keyboard interrupt handler (`keyboard.cpp:235`):

```cpp
if (debug_mode)
{
    print_sc(scancode);
}
```

## Serial Port Debugging (UART)

NasuaOS uses **UART (COM1)** at port `0x3F8` for serial debug output. This is the primary debugging channel for low-level kernel messages.

| Component     | UART Output                               |
|---------------|-------------------------------------------|
| NAPP loader   | `[NAPP] Initializing...`                  |
| NAPP loader   | `[NAPP] Starting application: calculator` |
| File resolver | `[File Resolver] ClawFS overlay mounted.` |
| Storage       | `[Storage] Disk detected.`                |
| Mouse         | `[MOUSE] PS/2 mouse connected`            |
| CPU           | `[CPU] MP available`                      |

To capture serial output when running in QEMU:

```bash
# QEMU with serial to file
qemu-system-x86_64 -serial file:serial.log -cdrom NasuaOS-x86_64.iso

# QEMU with serial to stdio
qemu-system-x86_64 -serial stdio -cdrom NasuaOS-x86_64.iso
```

## Kernel Logs

The kernel maintains an in-memory log buffer (128 entries, circular) via the logger subsystem. See [Debug Guide](debug_guide.md) for filtering and management commands.

```
user@nasua-os:/home> logs --show
user@nasua-os:/home> logs --level ERROR
user@nasua-os:/home> logs --subsystem NAPP
user@nasua-os:/home> logs --put "Manual test message"
```

## Safe Mode

Safe mode restricts functionality for troubleshooting. See [Safe Mode](debug_guide.md#safe-mode) in the debug guide for details.

## Kernel Panic Screen

When a kernel panic occurs, the system renders a detailed panic screen with:
- Panic reason message
- Error code
- RIP (instruction pointer)
- RSP (stack pointer)
- Fault address (CR2)
- CPU model
- A QR code linking to the debug instructions documentation

See [Debug Guide - Kernel Panics](debug_guide.md#kernel-panics-and-triple-faults) for details.
