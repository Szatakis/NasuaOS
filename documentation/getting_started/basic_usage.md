# Basic Usage

This guide covers the essentials of using NasuaOS after boot.

## Shell Prompt

After booting into the 64-bit kernel, you are presented with a shell prompt in the text console:

```
user@nasua-pc:/home>
```

The prompt format is:

| Component  | Color  | Description                                          |
|------------|--------|------------------------------------------------------|
| `user`     | Blue   | Username (currently hardcoded as `user`)             |
| `nasua-pc` | White  | Machine/hostname (currently hardcoded as `nasua-pc`) |
| `/home`    | Yellow | Current working directory                            |
| `>`        | Gray   | Command prompt                                       |

## Desktop Environment

The 64-bit kernel provides a graphical desktop with:

- **Taskbar** at the bottom with:
  - Start button (left Windows key)
  - Taskbar entries for open applications
  - System tray icons (speaker, notifications)
  - Clock and date
- **Start Menu** — Shows built-in applications and NAPP apps
- **Desktop** — Background image, supports open windows

### Built-in Applications

Access these from the shell via `bootapp`:

| Application  | Command                      | Description               |
|--------------|------------------------------|---------------------------|
| Terminal     | `bootapp --app terminal`     | Graphical terminal window |
| Settings     | `bootapp --app settings`     | System settings UI        |
| Task Manager | `bootapp --app task_manager` | Process/task manager      |

### /bin Applications (NAPP)

These are loaded dynamically from `/bin`:

| Application  | Command                      | Description                      |
|--------------|------------------------------|----------------------------------|
| SuaEdit      | `bootapp --app suaedit`      | Text editor with terminal panel  |
| Calculator   | `bootapp --app calculator`   | Graphical calculator             |
| Snake        | `bootapp --app snake`        | Classic Snake game               |
| Minesweeper  | `bootapp --app minesweeper`  | Classic Minesweeper game         |

List all available applications:
```
bootapp --list
```

## Window Management

- **Move**: Click and drag the window title bar
- **Resize**: Drag window edges/corners
- **Maximize**: Click the maximize button in the title bar
- **Minimize**: Click the minimize button
- **Close**: Click the close button
- **Focus**: Click anywhere on a window to bring it to front

## Keyboard Shortcuts

| Keys                             | Action                                        |
|----------------------------------|-----------------------------------------------|
| `Enter`                          | Execute shell command / submit terminal input |
| `Backspace`                      | Delete previous character                     |
| `Shift` + `↑`                    | Previous command in shell history             |
| `Shift` + `↓`                    | Next command in shell history                 |
| `Left Windows` / `Right Windows` | Toggle start menu                             |
| `←` / `↑` / `↓` / `→` (no Shift) | Move mouse cursor                             |
| `Caps Lock`                      | Toggle capitalization                         |
| `Esc` (in terminal)              | Sends to active window                        |

## File Operations

Navigate and manage files using the shell commands:

```bash
# List files
ls              # List current directory
ls /home        # List a specific path

# Change directory
cd /home/user
cd ..           # Go up one level
cd ~            # Return to /home

# Show current directory
pwd

# Create files and directories
touch --file myfile.txt
mkdir --dir_name mydir

# Read files
cat --file myfile.txt

# Copy and move
cp --source /home/myfile.txt --destination /tmp/myfile.txt
mv --source /home/myfile.txt --destination /tmp/myfile.txt

# Remove
rm --name myfile.txt --type file
rm --name mydir --type dir
```

## System Commands

```bash
# System information
info                    # CPU, RAM, storage, ClawFS status
uptime                  # System uptime
fetch                   # ASCII art logo + system summary

# Clock
time --get              # Show current date/time
time --set 15.06.2025-14:30:00   # Set system time
time --info             # RTC battery status

# Power
shutdown                # Power off
reboot                  # Restart

# Debug
debug --on              # Enable debug mode
debug --off             # Disable debug mode
safe_mode               # Enable safe mode
```

## Help System

The `help` command is paginated. Pages 1–10 document built-in shell commands. Page 11 and above list `/sbin` commands (dynamically loaded from disk):

```bash
help --page 1     # Built-in commands (page 1 of 10)
help --page 6     # Debug commands
help --page 11    # /sbin commands (first page)
```
