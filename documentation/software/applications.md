# Software - Applications

NasuaOS ships with several applications, both built-in (compiled into the kernel) and dynamically loaded (from `/bin`).

## Built-in Applications

These applications are compiled into the kernel image and launched via `bootapp --app <name>`:

### settings

A GUI settings application.

| Property | Value |
|----------|-------|
| Type | GUI window |
| Entry | `settings_main()` in `kernel_64bit/src/applications/` |
| API | Uses `napp_gui` and `napp_window` |
| Description | Provides a graphical interface for system configuration |

```
user@nasua-os:/home> bootapp --app settings
```

### terminal

A terminal emulator window.

| Property | Value |
|----------|-------|
| Type | GUI window |
| Entry | `terminal_main()` |
| API | Uses `napp_gui` for drawing |
| Description | Opens a terminal window within the GUI environment |

```
user@nasua-os:/home> bootapp --app terminal
```

### suaedit

A text editor with an integrated terminal panel.

| Property | Value |
|----------|-------|
| Type | GUI window |
| Entry | `_start()` in `utilities/applications/suaedit/suaedit.cpp` |
| API | Uses `napp_gui` for drawing, `napp_window` for callbacks, `api->set_print_redirect` + `api->execute_command` for integrated commands |
| Description | A lightweight text editor with a built-in terminal panel at the bottom of the window. The terminal captures command output via `set_print_redirect`, renders text with wrapping, supports mouse-wheel scroll, and draws input inline on the same line as the kernel prompt. |

The terminal panel is toggled via the **Terminal** menu item. When visible, command output from
`execute_command` is redirected into the panel instead of the main shell buffer.
The input prompt (`$ `) is rendered on the same visual row as the last line of
output, matching the behaviour of the kernel built-in terminal.

```
user@nasua-os:/home> bootapp --app suaedit
```

### task_manager

A process/task manager.

| Property | Value |
|----------|-------|
| Type | GUI window |
| Entry | `task_manager_main()` |
| API | Uses `napp_gui` for drawing |
| Description | Displays running NAPP processes and their status |

```
user@nasua-os:/home> bootapp --app task_manager
```

## /bin NAPP Applications

These are dynamically loaded from `/bin/<app>/<app>.napp`:

### bootcheck

| Property | Value |
|----------|-------|
| Type | Console (terminal output) |
| Source | `utilities/applications/bootcheck/bootcheck.cpp` |
| API | `api->print_info`, `api->print_line`, `api->print_dec`, `api->serial_log` |
| Description | Tests the NAPP API and confirms successful loading |

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

### calculator

| Property | Value |
|----------|-------|
| Type | GUI (interactive window) |
| Source | `utilities/applications/calculator/calculator.cpp` |
| API | `napp_gui` (fill_block, draw_text, draw_rect, put_pixel) |
| Description | A graphical calculator with button callbacks |

```cpp
int _start(const napp_api* api)
{
    api->window.create_window(400, 600, "Calculator");
    api->window.on_draw = calculator_draw;
    api->window.on_key = calculator_key;
    api->window.on_mouse = calculator_mouse;
    api->window.draw_window();
    return 0;
}
```

```
user@nasua-os:/home> bootapp --app calculator
```

## Running Applications

All applications are launched through the `bootapp` command:

```
bootapp --list        # List all available apps
bootapp --app settings     # Launch settings
bootapp --app calculator   # Launch calculator
```

See [NAPP Running](napp/running.md) for the full application loading lifecycle.
