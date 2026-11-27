# Software - Utilities

NasuaOS provides a set of system utilities accessible through the shell. These utilities fall into two categories: built-in commands and `/sbin` commands.

## Built-in Utility Commands

These commands are compiled directly into the kernel and are always available.

| Command             | Description                           |
|---------------------|---------------------------------------|
| `logs`              | Manage kernel log buffer              |
| `info`              | Show system and hardware information  |
| `time`              | Read/set RTC clock                    |
| `bootapp`           | Launch built-in or NAPP applications  |
| `format`            | Format ClawFS filesystem              |
| `mount` / `unmount` | Toggle ClawFS overlay                 |
| `fetch`             | Display system summary with ASCII art |
| `echo`              | Print text to screen or save to file  |
| `clear`             | Clear the terminal                    |
| `calc`              | Command-line calculator               |
| `rand`              | Random number generator               |
| `beep`              | PC speaker beep                       |
| `asciiart`          | Convert text to ASCII banner          |
| `debug`             | Enable/disable debug mode             |
| `resolution`        | Show screen resolution                |
| `source`            | Show source code URL                  |
| `uart`              | Send text to serial port              |

### Example Usage

```
user@nasua-os:/home> info
Software information
  System Version:  NasuaOS 0.8.0
  Kernel Version:  0.3.0

Hardware information
  CPU:             Intel(R) Core(TM) ...

logs --show
[14:30:00][INFO][NAPP] Application started: calculator
[14:30:05][ERROR][File Resolver] File not found: /sbin/unknown

bootapp --list
Built-in applications:
 - settings
 - terminal
 - task_manager

/bin applications:
 - bootcheck
 - calculator
 - minesweeper
 - snake
 - suaedit
```

## /sbin System Commands

These commands are loaded dynamically as flat-binary NAPP applications from `/sbin/`.

| Command | Description                | Flags                       |
|---------|----------------------------|-----------------------------|
| `cat`   | Display file contents      | `--file`, `--file_n`        |
| `cd`    | Change directory           | `[path]`                    |
| `cp`    | Copy a file                | `--source`, `--destination` |
| `ls`    | List directory contents    | `[path]`                    |
| `mkdir` | Create a directory         | `--dir_name`                |
| `mv`    | Move/rename a file         | `--source`, `--destination` |
| `pwd`   | Print current directory    | None                        |
| `rm`    | Remove a file or directory | `--name`, `--type`          |
| `tree`  | Display directory tree     | `[path]`                    |

### Example Usage

```
user@nasua-os:/home> cat --file /home/user/notes.txt
Contents of notes.txt...

user@nasua-os:/home/user> ls
documents  notes.txt  projects

user@nasua-os:/home/user> cp --source notes.txt --destination /tmp/
File copied.

user@nasua-os:/home/user> mkdir --dir_name newdir
Directory created.

user@nasua-os:/home/user> mv --source notes.txt --destination rename.txt
File moved.

user@nasua-os:/home/user> rm --name newdir --type dir
Directory removed.
```

## Utility Loading

Built-in commands are always available. `/sbin` commands require:

1. The ISO rootfs to be mounted (automatic at boot) — OR —
2. ClawFS to be mounted via `mount` (for persistence)

Use `format --commands` to copy `/sbin` commands to ClawFS for persistent storage.

## File Operations Summary

| Operation            | Command                      | Source   |
|----------------------|------------------------------|----------|
| Create directory     | `mkdir --dir_name`           | `/sbin`  |
| Delete file          | `rm --name --type file`      | `/sbin`  |
| Delete directory     | `rm --name --type dir`       | `/sbin`  |
| Copy file            | `cp --source --destination`  | `/sbin`  |
| Move/rename          | `mv --source --destination`  | `/sbin`  |
| Read file            | `cat --file`                 | `/sbin`  |
| List directory       | `ls`                         | `/sbin`  |
| Directory tree       | `tree`                       | `/sbin`  |
| Create empty file    | `touch --file`               | Built-in |
| Write to file (echo) | `echo --text --file`         | Built-in |

## System Information

| Command       | Shows                                                |
|---------------|------------------------------------------------------|
| `info`        | OS version, CPU, RAM, storage, ClawFS overlay status |
| `time --get`  | Current RTC date and time                            |
| `time --info` | RTC battery status                                   |
| `uptime`      | System uptime since boot                             |
| `resolution`  | Screen resolution and framebuffer mode               |
| `logs`        | Kernel log entries (filterable by level/subsystem)   |
| `fetch`       | ASCII art logo with system summary                   |
