# Configuration

NasuaOS configuration is stored in text-based configuration files loaded as boot modules.

## Configuration Files

| File | Path (in ISO) | Description |
|------|---------------|-------------|
| `defaults.txt` | `/config/defaults.txt` | Default system-wide settings |
| `boot_config.txt` | `/config/boot_config.txt` | Boot-time behavior modifiers |
| `boot_config_sf.txt` | `/config/boot_config_sf.txt` | Safe-mode boot config |

### defaults.txt

The main configuration file. It uses an INI-like format with sections:

```ini
[System]
Hostname=NasuaOS
Language=en_US
Timezone=null

[Graphics]
Resolution=1280x720
VSync=false
Cursor=true
Animations=true

[Boot]
Timeout=5
DefaultEntry=NasuaOS 64-bit
Verbose=true
HelpHidden=true

[Keyboard]
Layout=us

[Audio]
Enabled=true
Volume=80

[Debug]
DebugMode=false
SerialOutput=false
ShowFPS=false

[Storage]
DefaultFilesystem=CLAWFS
AutoMount=true
```

### boot_config.txt

Contains keywords that modify boot behavior. Currently recognized keywords:

| Keyword | Effect |
|---------|--------|
| `SAFE_MODE` | Enables safe mode (same as running `safe_mode` command at boot) |
| `DEBUG` | Enables debug mode (same as running `debug --on` at boot) |

The `boot_config_sf.txt` (safe mode config) typically contains `SAFE_MODE`.

## Safe Mode

Safe mode can be activated in three ways:

1. **Boot-time**: Use the "Safe Mode 64-bit" entry from the Limine boot menu
2. **Boot config**: Add `SAFE_MODE` to `boot_config.txt`
3. **Runtime**: Run the `safe_mode` command from the shell

When safe mode is active, the kernel sets `safe_mode = true` and may restrict certain functionality. The `info` command shows the ClawFS overlay status as "Unmounted" when safe mode is enabled.

## Debug Mode

Debug mode can be activated:

1. **Boot-time**: Add `DEBUG` to `boot_config.txt`
2. **Runtime**: Run `debug --on` from the shell
3. **Safe mode**: If `DEBUG` is in the boot config

When debug mode is enabled:
- Scancode tracing is activated (keyboard scancodes printed to UART)
- Additional logging is emitted via the serial port (UART)

## Limine Configuration

The `limine.conf` file is **auto-generated** from a template located at `.config/config_generator/limine.conf.template` using:

```bash
python3 .config/config_generator/generate_conf.py
```

Manual edits to `.config/limine.conf` will be overwritten on the next build. To change boot behavior, edit `.config/defaults.txt` and regenerate.

## Kernel Configuration

Build-time architecture settings are in `.config/kernel_config.mk`:

```makefile
ARCH := x86_64
SUB_ARCH := x86_32
DEBUG_WSL := false
DEBUG_LINUX := false
DEBUG_BUILD := false
```

Override these variables when running make:

```bash
make run ARCH=x86_64 QEMUFLAGS="-m 4G"
```
