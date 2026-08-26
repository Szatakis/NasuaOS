# Software - Applications

NasuaOS ships with several applications, both built-in (compiled into the kernel) and dynamically loaded (from `/bin`).

## Built-in Applications

These applications are compiled into the kernel image and launched via `bootapp --app <name>`:

### settings

A GUI settings application.

|   Property  | Value                                                   |
|-------------|---------------------------------------------------------|
| Type        | GUI window                                              |
| Entry       | `settings_main()` in `kernel_64bit/src/applications/`   |
| API         | Uses `napp_gui` and `napp_window`                       |
| Description | Provides a graphical interface for system configuration |

```
user@nasua-os:/home> bootapp --app settings
```

### terminal

A terminal emulator window.

|   Property  | Value                                                   |
|-------------|---------------------------------------------------------|
| Type        | GUI window                                              |
| Entry       | `terminal_main()`                                       |
| API         | Uses `napp_gui` for drawing                             |
| Description | Opens a terminal window within the GUI environment      |

```
user@nasua-os:/home> bootapp --app terminal
```

### task_manager

A process/task manager.

|   Property  | Value                                                   |
|-------------|---------------------------------------------------------|
| Type        | GUI window                                              |
| Entry       | `task_manager_main()`                                   |
| API         | Uses `napp_gui` for drawing                             |
| Description | Displays running NAPP processes and their status         |

```
user@nasua-os:/home> bootapp --app task_manager
```

## /bin NAPP Applications

These are dynamically loaded flat binaries from `/bin/<app>` (the rootfs `/bin` directory):

### suaedit

A text editor with an integrated terminal panel.

|   Property  | Value                                                      |
|-------------|------------------------------------------------------------|
| Type        | GUI window                                                 |
| Entry       | `_start()` in `utilities/applications/suaedit/src/suaedit.cpp` |
| API         | Uses `napp_gui` for drawing, `napp_window` for callbacks, `api->set_print_redirect` + `api->execute_command` for integrated commands |
| Description | A lightweight text editor with a built-in terminal panel at the bottom of the window. The terminal captures command output via `set_print_redirect`, renders text with wrapping, supports mouse-wheel scroll, and draws input inline on the same line as the kernel prompt. |

The terminal panel is toggled via the **Terminal** menu item. When visible, command output from
`execute_command` is redirected into the panel instead of the main shell buffer.
The input prompt (`$ `) is rendered on the same visual row as the last line of
output, matching the behaviour of the kernel built-in terminal.

```
user@nasua-os:/home> bootapp --app suaedit
```

### calculator

A graphical calculator with button callbacks.

|  Property   | Value                                                      |
|-------------|------------------------------------------------------------|
| Type        | GUI (interactive window)                                   |
| Source      | `utilities/applications/calculator/src/calculator.cpp`         |
| API         | `napp_gui` (fill_block, draw_text, open_window)            |
| Description | A graphical calculator with button callbacks               |

```cpp
int _start(const napp_api* api)
{
    api->gui->open_window(&config);
    config.draw = calculator_draw;
    config.key = calculator_key;
    config.mouse = calculator_mouse;
    return 0;
}
```

```
user@nasua-os:/home> bootapp --app calculator
```

### bootcheck

|  Property   | Value                                                                     |
|-------------|---------------------------------------------------------------------------|
| Type        | Console (terminal output)                                                 |
| Source      | `utilities/applications/bootcheck/src/bootcheck.cpp`                      |
| API         | `api->print_info`, `api->print_line`, `api->print_dec`, `api->serial_log` |
| Description | Tests the NAPP API and confirms successful loading                        |

```cpp
int _start(const napp_api* api)
{
    api->print_info("NAPP API check passed");
    api->print_line("Version: ");
    api->print_dec(NAPP_ABI_VERSION);
    api->serial_log("[bootcheck] Running successfully\n");
    return 0;
}
```

```
user@nasua-os:/home> bootapp --app bootcheck
[bootcheck] Running successfully
NAPP API check passed
Version: 3
```

### minesweeper

A classic Minesweeper game.

|  Property  | Value                                                                              |
|------------|------------------------------------------------------------------------------------|
| Type       | GUI window                                                                         |
| Source     | `utilities/applications/minesweeper/src/minesweeper.cpp`                           |
| API        | `napp_gui` (fill_block, draw_text, open_window, resize_window)                     |
| Description| Classic Minesweeper game with beginner, intermediate, and expert difficulty levels |

Games do not appear in the Start Menu by default; launch via `bootapp`.

```
user@nasua-os:/home> bootapp --app minesweeper
```

### snake

A classic Snake game.

|  Property  | Value                                                                                 |
|------------|---------------------------------------------------------------------------------------|
| Type       | GUI window (with tick callback)                                                       |
| Source     | `utilities/applications/snake/src/snake.cpp`                                          |
| API        | `napp_gui` (fill_block, draw_text, open_window), `api->get_ticks`                     |
| Description| Classic Snake game with WASD controls and periodic tick callback for game loop timing |

Games do not appear in the Start Menu by default; launch via `bootapp`.

```
user@nasua-os:/home> bootapp --app snake
```

## Running Applications

All applications are launched through the `bootapp` command:

```
bootapp --list        # List all available apps
bootapp --app settings     # Launch settings
bootapp --app calculator   # Launch calculator
bootapp --app suaedit      # Launch text editor with integrated terminal
bootapp --app snake        # Launch Snake game
```

See [NAPP Running](napp/running.md) for the full application loading lifecycle.
