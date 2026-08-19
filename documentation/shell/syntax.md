# Shell Syntax

The NasuaOS shell is built into the 64-bit kernel (`kernel_64bit/src/applications/shell/commands.cpp`). It processes commands entered at the `user@nasua-pc:/home>` prompt.

## Command Format

Commands use a **flag-based** syntax. Unlike POSIX shells, NasuaOS commands require flags prefixed with `--`:

```
<command> <flag> <value> <flag> <value> ...
```

### Example Comparison

| Traditional Unix | NasuaOS |
|------------------|---------|
| `mv old.txt new.txt` | `mv --source old.txt --destination new.txt` |
| `cp src dest` | `cp --source src --destination dest` |
| `cat file.txt` | `cat --file file.txt` |
| `time` | `time --get` |
| `ls /home` | `ls /home` |
| `pwd` | `pwd` |
| `mkdir dir` | `mkdir --dir_name dir` |

## Flag Parsing

The shell splits the command line into a command name and an arguments string. Arguments are tokenized by spaces (with a maximum of 64 characters for the command buffer). Quoted arguments (using `"`) are supported for flags that accept values containing spaces.

Arguments are passed to `/sbin` commands as `argv[]` with `argv[0]` set to the command name.

## Built-in vs /sbin Commands

| Property | Built-in Commands | /sbin Commands |
|----------|-------------------|----------------|
| Location | Compiled into kernel | Loaded from `/sbin` |
| Source | `kernel_64bit/src/applications/shell/commands.cpp` | `utilities/system_functions/` |
| Load Source | Always available | ISO rootfs (FAT) or ClawFS overlay |
| Mount Dependency | No | Yes (when ClawFS is mounted) |
| Help Pages | Pages 1–10 | Page 11 and above |
| Execution | Direct function call | NAPP binary loaded and executed |
| Memory | Part of kernel image | Dynamically loaded to heap |

## Built-in Commands

The following commands are compiled directly into the kernel:

| Command | Description |
|---------|-------------|
| `help` | Show help pages |
| `clear` | Clear the screen |
| `echo` | Print text or write to a file |
| `fetch` | Display system summary with ASCII logo |
| `time` | System clock utility |
| `reboot` | Restart the computer |
| `shutdown` | Power off the system |
| `format` | Format ClawFS or copy commands |
| `mount` | Mount ClawFS overlay |
| `unmount` | Unmount ClawFS overlay |
| `touch` | Create a new empty file |
| `info` | Display system and hardware information |
| `source` | Show source code URL |
| `debug` | Enable/disable debug mode |
| `uptime` | Show system uptime |
| `panic` | Trigger a kernel panic (debugging) |
| `resolution` | Show screen resolution |
| `logs` | Kernel log management |
| `bootapp` | Launch applications |
| `beep` | PC speaker beep |
| `calc` | Calculator |
| `rand` | Random number generator |
| `inb` | Read I/O port byte |
| `outb` | Write I/O port byte |
| `asciiart` | Convert text to ASCII banner |
| `safe_mode` | Enable safe mode |
| `pwd` | Print working directory (built-in duplicate) |

## /sbin Commands

The following commands are dynamically loaded flat-binary NAPP applications from `/sbin`:

| Command | Description |
|---------|-------------|
| `cat` | Display file contents |
| `cd` | Change directory |
| `cp` | Copy file |
| `ls` | List directory contents |
| `mkdir` | Create directory |
| `mv` | Move/rename file |
| `pwd` | Print working directory |
| `rm` | Remove file or directory |

These commands are loaded from the root filesystem. See [NAPP Format](napp/format.md) and [Shell /sbin Commands](sbin_commands.md) for details.

## Command History

- Press `Shift` + `↑` to navigate to the previous command
- Press `Shift` + `↓` to navigate to the next command
- History stores up to 10 entries (ring buffer)
- Duplicate consecutive commands are not stored separately

## Entering Commands

Commands are entered at the text prompt. The `Enter` key (scancode `0x1C`) triggers execution. The `Backspace` key (scancode `0x0E`) deletes the last character. The command buffer supports up to 63 characters.

When a graphical window is active and the mouse cursor is over it, keyboard input is sent to the window instead of the shell. The shell prompt is temporarily disabled in this case.
