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

### browser

A simple graphical browser that renders HTML from a local ClawFS file or a plain HTTP URL.

| Property   | Value |
|------------|-------|
| Type       | GUI window |
| Source     | `utilities/applications/browser/src/browser.cpp` |
| API        | `napp_gui`, ClawFS read/resolve, and the NAPP `http_get` service |
| Description| Fetches plain HTTP pages over DNS/TCP and renders basic HTML structure, including headings, paragraphs, lists, line breaks, and common text entities |

Enter an `http://` URL or a local path such as `/home/user/page.html` in the
address field and press Enter. Use `j`/`k` or Page Up/Page Down to scroll, and
`e` to edit the path. HTTPS/TLS, automatic redirects, JavaScript, and CSS are
not supported. Local files are limited to one 512-byte sector; network
responses are limited to 8 KB.

```
user@nasua-os:/home> bootapp --app browser
```

### suaedit

A text editor with an integrated terminal panel.

|   Property  | Value                                                      |
|-------------|------------------------------------------------------------|
| Type        | GUI window                                                 |
| Entry       | `_start()` in `utilities/applications/suaedit/src/suaedit.cpp` |
| API         | Uses `napp_gui` for drawing, `napp_window` for callbacks, `api->set_print_redirect` + `api->execute_command` for integrated commands |
| Description | A lightweight text editor with a VS Code-style menu bar; File contains New, Open, and Save, with Run, Help, and Terminal on the top bar. Run or F5 compiles and executes the current buffer as SuaScript in the integrated terminal. It supports typed `int`, `bool`, and `string` variables, arithmetic and loop control, and uses `.sua` as the script extension. It opens and creates files in the current folder; files are limited to one 512-byte CLAWFS sector. On a formatted ClawFS disk, it creates and opens `info.txt`. Run `format` in the integrated terminal if the disk has not been initialized. |

The terminal panel is toggled via the **Terminal** menu item. When visible, command output from
`execute_command` is redirected into the panel instead of the main shell buffer.
The input prompt (`$ `) is rendered on the same visual row as the last line of
output, matching the behaviour of the kernel built-in terminal.

SuaEdit can run typed `.sua` scripts; see [SuaScript](suascript.md) for the
language syntax and examples.

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
bootapp --app browser      # Open the local HTML browser
bootapp --app suaedit      # Launch text editor with integrated terminal
bootapp --app snake        # Launch Snake game
```

See [NAPP Running](napp/running.md) for the full application loading lifecycle.
