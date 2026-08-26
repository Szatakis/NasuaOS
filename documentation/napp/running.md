# Running NAPP Applications

NAPP applications can be launched in two ways: directly from `/bin` via the `bootapp` command, or through the shell for `/sbin` commands.

## bootapp Command

The `bootapp` built-in command (in `commands.cpp`) manages application loading and execution:

```
bootapp --list
bootapp --app <app_name>
```

### Listing Applications

```
user@nasua-os:/home> bootapp --list
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

The `--list` option queries two sources:

1. **Built-in applications**: compiled into the kernel image
2. **`/bin` applications**: NAPP binaries found in `/bin/` (flat files)

### Launching Applications

```
bootapp --app <app_name>
```

This executes the named application. The loader:

1. Checks if the app is a built-in (settings, terminal, task_manager)
2. If not built-in, checks `/bin/<app_name>`
3. Validates the NAPP header (magic, ABI version)
4. Loads sections into memory
5. Calls `_start` with the `napp_api` pointer

## Loading Process

The NAPP loader (`napp.cpp`) uses these key functions:

| Function                                  | Description                                |
|-------------------------------------------|--------------------------------------------|
| `napp_init()`                             | Initialize the NAPP subsystem              |
| `napp_run(const void* napp_data)`         | Load and execute a NAPP binary from memory |
| `napp_run_path(const char* path)`         | Load and execute a NAPP binary from a path |
| `napp_run_sbin(const char* path)`         | Load and execute a `/sbin` flat binary     |
| `napp_list()`                             | List NAPP applications in `/bin`           |
| `napp_list_sbin()`                        | List `/sbin` command binaries              |
| `napp_exists(const char* name)`           | Check if a NAPP application exists         |
| `napp_get_current_path()`                 | Get current NAPP working directory         |
| `napp_set_current_path(const char* path)` | Set current NAPP working directory         |

## Command Loading Flow

When a `/sbin` command is requested via the shell:

```
1. Shell receives command name (e.g., "ls")
2. Shell checks if it's a built-in → No
3. Shell calls execute_command("/sbin/ls", args)
4. File resolver resolves path:
   a. If ClawFS mounted: check ClawFS /sbin/ls
   b. If not found: check FAT ISO /sbin/ls
5. Binary loaded via napp_run_sbin()
6. _start() called with napp_api pointer + argc/argv
7. Application runs, returns exit code
```

## Application Types

### Built-in Applications

These are compiled into the kernel and launched directly:

| Application    | Description              |
|----------------|--------------------------|
| `settings`     | System settings GUI      |
| `terminal`     | Terminal emulator window |
| `task_manager` | Process/task manager     |

Built-in apps do not go through the NAPP loader — they have direct function calls in `commands.cpp`.

### NAPP Applications (/bin)

These are loaded from `/bin/` (flat binary files, one per application):

| Application  | Type    | Description                           |
|--------------|---------|---------------------------------------|
| `bootcheck`  | Console | Tests NAPP API functions              |
| `calculator` | GUI     | Graphical calculator with callbacks   |
| `minesweeper`| GUI     | Classic Minesweeper game              |
| `snake`      | GUI     | Classic Snake game with tick callback |
| `suaedit`    | GUI     | Text editor with integrated terminal  |

These go through the NAPP loader (`napp_run`).

### /sbin Commands (/sbin)

These are flat-binary NAPP applications loaded from `/sbin/`:

| Command                                             | Description         |
|-----------------------------------------------------|---------------------|
| `cat`, `cd`, `cp`, `ls`, `mkdir`, `mv`, `pwd`, `rm` | File operations     |
| `tree`                                              | Directory tree view |

These go through the `/sbin`-specific loader path (`napp_run_path`).

## Exit Codes

NAPP applications return an `int` from `_start`:

| Return Value | Meaning                            |
|--------------|------------------------------------|
| `0`          | Success                            |
| Non-zero     | Error (code varies by application) |

The kernel logs the exit code via the logger.

## Serial Logging

NAPP applications can send debug messages to the UART serial port:

```cpp
api->serial_log("Debug: application starting\n");
```

This is useful for debugging loading issues and runtime behavior. Serial output is visible in QEMU with the `-serial file:serial.log` flag.

## Built-in vs Dynamically Loaded

| Property              | Built-in       | /bin (NAPP)     | /sbin (flat binary) |
|-----------------------|---------------|-----------------|----------------------|
| Storage               | Kernel image  | /bin directory  | /sbin directory      |
| Header                | None          | NAPP header     | NAPP header optional |
| Loader                | Direct call   | napp_run()      | napp_run_path()      |
| Overlay Affected?     | No            | Yes             | Yes                  |
| Persistence Required? | No            | Only on ClawFS  | Only on ClawFS       |

## Error Handling

If a NAPP application fails to load:

1. The loader logs an `[NAPP]` error via the logger
2. The shell prints an error message
3. Common failures: invalid magic, wrong ABI version, file not found, memory allocation failure
