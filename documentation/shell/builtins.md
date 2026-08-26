# Shell Built-in Commands

These commands are compiled directly into the 64-bit kernel. They are always available, even without a mounted disk.

## help

Display the help system, paginated across 10 static pages plus dynamic `/sbin` pages.

```
help
help --page <n>
```

Pages 1–10 list built-in commands. Page 11 and above list `/sbin` commands loaded from the filesystem. The number of `/sbin` pages depends on how many commands are available.

```
user@nasua-os:/home> help --page 1
Available commands (Page 1/<total>):
 See page 11 and above to view commands loaded from /sbin.
 -help                               - Show the first page of help
   --page [1-10]                     - Show specific help page
 -info                               - Display OS version and hardware info
 -source                             - Show link to the OS source code
 -shutdown                           - Power off the system safely
 -reboot                             - Restart the computer
 -echo                               - Print text to the screen or write to file
   --text <text>                     - (Required) Specify the text to print
   --color [0xRRGGBB]                - (Optional) Set custom HEX text color
   --file <file_name.file_ext>       - (Optional) Save the text output into a file
 -asciiart                           - Convert text into large ASCII banner
    --text <string>                  - (Required) Text to transform
```

## clear

Clear the terminal screen.

```
clear
```

## echo

Print text to the screen, optionally with custom color, or save to a file.

```
echo --text <text> [--color 0xRRGGBB] [--file <filename>]
```

| Flag      | Required | Description                     |
|-----------|----------|---------------------------------|
| `--text`  | Yes      | Text to print                   |
| `--color` | No       | Hex color (e.g., `0xFF5500`)    |
| `--file`  | No       | Save output to a file on ClawFS |

```
user@nasua-os:/home> echo --text "Hello, world!" --color 0xFF5500
user@nasua-os:/home> echo --text "Saved line" --file myfile.txt
```

## fetch

Display the NasuaOS ASCII art logo and system summary.

```
fetch
```

## time

System clock utility for reading and setting the RTC (Real-Time Clock).

```
time --get
time --set <DD.MM.YYYY-HH:MM:SS>
time --info
```

| Flag     | Description                                              |
|----------|----------------------------------------------------------|
| `--get`  | Display current date and time from RTC                   |
| `--set`  | Set system date and time (format: `DD.MM.YYYY-HH:MM:SS`) |
| `--info` | Display CMOS storage and RTC battery status              |

```
user@nasua-os:/home> time --get
15.06.2025 14:30:00
user@nasua-os:/home> time --set 15.06.2025-16:45:30
System time updated successfully!
user@nasua-os:/home> time --info
RTC battery: OK
```

## reboot / shutdown

Restart or power off the system via ACPI.

```
reboot
shutdown
```

## format

Format ClawFS or copy system commands from the ISO rootfs to ClawFS.

```
format
format --commands
format --clear
```

| Option       | Description                                                        |
|--------------|--------------------------------------------------------------------|
| (no args)    | Format (initialize) ClawFS — creates the base directory structure  |
| `--commands` | Copy all `/bin` and `/sbin` flat binaries from ISO to ClawFS       |
| `--clear`    | Completely wipe ClawFS without recreating the structure            |

```
user@nasua-os:/home> format --commands
Setting up persistent command storage...
Commands copied successfully to ClawFS.
Run 'mount' to enable the overlay.
```

## mount / unmount

Toggle the ClawFS overlay for command loading.

```
mount
unmount
```

| Command   | Effect                                                                      |
|-----------|-----------------------------------------------------------------------------|
| `mount`   | Mount ClawFS overlay. Commands load from ClawFS (taking priority over ISO). |
| `unmount` | Unmount ClawFS overlay. Commands load from the ISO rootfs (FAT) only.       |

After mounting, `/sbin` commands and `/bin` applications load from ClawFS instead of the read-only ISO.

## touch

Create a new empty file in the current directory on ClawFS.

```
touch --file <filename>
```

## info

Display system version, CPU model, RAM, storage, and ClawFS overlay status.

```
info
```

```
user@nasua-os:/home> info
Software information
  System Version:  NasuaOS 0.8.0
  Kernel Version:  0.3.0

Hardware information
  CPU:             Intel(R) Core(TM) ...
  Total RAM:       2048MB
  Used RAM:        128MB

Storage information
  Storage type:    ATA Disk
  Storage Total:   2048MB
  Storage Used:    128MB

ClawFS status
  Overlay:         Unmounted (commands from ISO)
```

## source

Display the NasuaOS source code URL.

```
source
```

## debug

Enable or disable debug mode. When enabled, keyboard scancodes are printed to the serial port (UART).

```
debug --on
debug --off
```

## uptime

Display system uptime since boot.

```
uptime
```

## panic

Trigger a kernel panic with a user-specified message. Intended for debugging the kernel panic handler.

```
panic
```

## resolution

Display the current screen resolution and framebuffer video mode information.

```
resolution
```

## logs

Kernel log management utility. Logs are stored in a circular buffer (128 entries) with levels (INFO, WARN, ERROR, DEBUG) and subsystem names.

```
logs --show
logs --clear
logs --level <INFO|WARN|ERROR|DEBUG>
logs --subsystem <subsystem_name>
logs --put <text>
```

| Flag          | Description                              |
|---------------|------------------------------------------|
| `--show`      | Display all stored kernel logs           |
| `--clear`     | Clear the kernel log buffer              |
| `--level`     | Filter by log level                      |
| `--subsystem` | Filter by subsystem name                 |
| `--put`       | Add a custom log message with INFO level |

```
user@nasua-os:/home> logs --put "Manual test message"
Log added successfully
```

## bootapp

Application manager. Launches built-in kernel applications or NAPP binaries from `/bin`.

```
bootapp --list
bootapp --app <application_name>
```

| Flag     | Description                                                             |
|----------|-------------------------------------------------------------------------|
| `--list` | List all available applications                                         |
| `--app`  | Launch a specific application (checks built-in apps first, then `/bin`) |

Built-in applications: `settings`, `terminal`, `suaedit`, `task_manager`.

```
user@nasua-os:/home> bootapp --list
Built-in applications:
 - settings
 - terminal
 - suaedit
 - task_manager

/bin applications:
 - bootcheck
 - calculator
```

## beep

Play a tone on the PC speaker.

```
beep
beep --freq <frequency> --dur <duration>
```

| Flag     | Default | Description    |
|----------|---------|----------------|
| `--freq` | 1000 Hz | Tone frequency |
| `--dur`  | 200 ms  | Duration       |

## calc

Command-line calculator supporting basic arithmetic.

```
calc --op <add|sub|mul|div> --num1 <value> --num2 <value>
```

```
user@nasua-os:/home> calc --op add --num1 15 --num2 27
Result: 42
```

## rand

Generate a random number within a range.

```
rand --min <value> --max <value>
```

```
user@nasua-os:/home> rand --min 1 --max 100
Random number: 42
```

## inb / outb

Read or write a byte to an I/O port (hardware access).

```
inb --port 0xHEX
outb --port 0xHEX --val 0xHEX
```

```
user@nasua-os:/home> inb --port 0x60
Port 0x60 = 0xFA
user@nasua-os:/home> outb --port 0x3F8 --val 0x48
Written 0x48 -> 0x3F8
```

## asciiart

Convert text into a large ASCII banner.

```
asciiart --text "Hello"
```

## safe_mode

Enable safe mode for system debugging. When active, certain subsystems are restricted.

```
safe_mode
```
