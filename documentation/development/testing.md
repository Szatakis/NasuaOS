# Development - Testing

NasuaOS can be tested using QEMU. This guide covers running, debugging, and testing.

## Running in QEMU

### Standard Build and Run

```bash
make all    # Build the ISO
make run    # Run in QEMU
```

### HDD Mode

```bash
make all-hdd   # Build ISO and HDD
make run-hdd   # Run HDD image in QEMU
```

### BIOS Mode (Legacy)

```bash
make run-bios
```

### WSL Mode

The Makefile auto-detects WSL and uses the Windows QEMU binary:

```bash
make run   # Automatically detects WSL
```

## QEMU Debugging

### Serial Output

NasuaOS uses UART (COM1, port 0x3F8) for debug output:

```bash
qemu-system-x86_64 -serial file:serial.log -cdrom NasuaOS-x86_64.iso
```

To enable more serial output, use debug mode:

```
user@nasua-os:/home> debug --on
```

Or add `DEBUG` to the boot config.

### GDB Debugging

```bash
# Build with debug symbols
make all DEBUG_BUILD=true

# Run QEMU with GDB stub
qemu-system-x86_64 -s -S -cdrom NasuaOS-x86_64.iso

# In another terminal
gdb-multiarch
(gdb) target remote :1234
(gdb) symbol-file kernel_64bit/build/kernel.elf
(gdb) break kmain
(gdb) continue
```

### QEMU Monitor

While QEMU is running, access the monitor with `Ctrl+Alt+2` (or `Ctrl+A` then `c`):

| Command | Description |
|---------|-------------|
| `info registers` | View CPU registers |
| `info mem` | View memory mappings |
| `xp /x 10 0x100000` | Examine 10 hex words at 0x100000 |
| `info pic` | View interrupt controller state |
| `q` | Quit QEMU |

## Testing Checklist

When developing or testing changes:

1. **Boot test** — Does the kernel boot without hanging?
2. **Shell test** — Does the shell accept and execute commands?
3. **Filesystem test** — Does `ls`, `cd`, `pwd` work?
4. **Mount test** — Does `mount`/`format` work?
5. **NAPP test** — Does `bootapp --app calculator` launch?
6. **Serial test** — Check for errors in `serial.log`
7. **Panic test** — Does `panic` render the panic screen correctly?

## Serial Output Verification

After running, check `serial.log` for:

```
[NAPP] Initializing...
[NAPP] Application started: calculator
[Storage] Disk detected.
[File Resolver] ClawFS overlay mounted.
[MOUSE] PS/2 mouse connected
[CPU] MP available
```

Missing messages indicate where initialization failed.

## Debug Instructions

For detailed debugging procedures, see:

- `documentation/debug_instructions/debug_instructions.md` — Comprehensive debug guide
- `documentation/debugging/debug_mode.md` — Debug mode settings
- `documentation/debugging/debug_guide.md` — Troubleshooting guide

## CI Verification

The CI pipeline (`.github/workflows/build.yml`) performs:

```yaml
- name: Build ISO
  run: make all

- name: Build HDD
  run: make all-hdd

- name: Upload ISO
  # Uploads NasuaOS-x86_64.iso

- name: Upload HDD
  # Uploads NasuaOS-x86_64.hdd
```

Ensure both `make all` and `make all-hdd` succeed before submitting changes.

## Testing on Real Hardware

Booting NasuaOS on real hardware:

1. Write the ISO or HDD image to a USB drive:
   ```bash
   sudo dd if=NasuaOS-x86_64.iso of=/dev/sdX bs=4M
   ```
2. Boot from USB
3. Select "NasuaOS x86_64" from the Limine boot menu

**Warning**: Use a spare USB drive. Data will be destroyed.

## Shell Testing

Test all shell commands:

```
help --page 1     # Built-in commands
help --page 11    # /sbin commands (requires rootfs mount)
info              # System information
time --get        # RTC time
uptime            # Uptime
logs --show       # Kernel logs
fetch             # System summary
```

Test filesystem:

```
cd /sbin
ls
cat --file README          # (if exists)
mkdir --dir_name testdir
touch --file testfile
echo --text "Hello" --file testfile
cat --file testfile
rm --name testfile --type file
rm --name testdir --type dir
```

## NAPP Application Testing

```
bootapp --list            # List all apps
bootapp --app bootcheck   # Console app (prints to log)
bootapp --app calculator  # GUI app (opens calculator window)
bootapp --app settings    # GUI app
```
