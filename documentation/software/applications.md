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
| Entry       | `draw_task_manager()`                                   |
| API         | Kernel process registry and GPU drawing services       |
| Description | Lists system services, GUI apps, and shell commands with PID, type, and state |

Use `j`/`s` or `k`/`w` to scroll the process list. System components are shown
as logical processes; applications and commands are tracked as they start and
exit. The list refreshes automatically. NasuaOS does not yet have a scheduler,
so this list does not imply preemptive multitasking or process isolation.

```
user@nasua-os:/home> bootapp --app task_manager
```

## /bin NAPP Applications

These are dynamically loaded flat binaries from `/bin/<app>` (the rootfs `/bin` directory):

### file_explorer

A graphical ClawFS file explorer.

| Property   | Value |
|------------|-------|
| Type       | GUI window |
| Source     | `utilities/applications/file_explorer/src/file_explorer.cpp` |
| API        | `napp_gui` and ClawFS directory/file read services |
| Description| Browse folders, select locations, and preview text files |

Use the sidebar to jump to Home, Documents, Downloads, or the filesystem root.
Double-click a folder to open it, use Up or Backspace to go to its parent, and
select a text file to preview its contents. F5 refreshes the current folder;
`j`/`s` and `k`/`w` move the selection, and Enter opens the selected folder.
The preview displays up to 1 KB of a file.

```
user@nasua-os:/home> bootapp --app file_explorer
```

### WebBrowser

A graphical web browser that renders HTML and CSS from a local ClawFS file or an HTTP/HTTPS URL.

| Property   | Value |
|------------|-------|
| Type       | GUI window |
| Source     | `utilities/applications/WebBrowser/src/WebBrowser.cpp` |
| API        | `napp_gui`, ClawFS read/resolve, and the NAPP `http_get`/`https_get` services |
| Description| Fetches HTTP and HTTPS pages, follows redirects, opens bare domains over HTTPS, and renders common HTML elements with clickable links and a subset of CSS |

Enter an `http://` or `https://` URL or a local path such as
`/home/user/page.html` in the address field and press Enter. HTTPS uses TLS 1.2
and verifies the server certificate; it requires a valid RTC date and a CPU
with RDRAND. A bare domain such as `google.com` is opened over HTTPS.
Click links to navigate to other web pages or relative local HTML files.
Use `j`/`k` or Page Up/Page Down to scroll, and `e` to edit the address. The
renderer supports linked stylesheets (up to eight per page), embedded
`<style>` rules, and inline `style` attributes. Linked stylesheets can be loaded
over HTTP/HTTPS or from ClawFS; common redirects are followed. `@media`
`min-width` and `max-width` queries in px, em, rem, and vw units are evaluated
when the viewport is resized. `@supports`, `@container`, and `@layer` blocks
are parsed without evaluating their conditions. Selectors include tag names, `.class`, `#id`,
same-element combinations such as
`p.note`, descendant and direct-child selectors, and comma-separated selector
groups. CSS supports text and background colors, `:root` custom
properties with `var()`, alpha hex colors, the first color stop of simple
gradients, bold weight, basic text decoration and transform, text alignment,
vertical spacing from margin and padding, simple text-run borders,
`min-height` spacing for viewport-sized sections, block `width` in px and %,
`max-width` in px, `min(px, %)`, horizontal padding and auto horizontal margins,
`display: none`,
`visibility: hidden`, and preformatted whitespace. Block-element backgrounds
and solid borders are painted over the full content width and the lines occupied
by each element; inline borders remain text-run decorations. Colors
include common named colors, `#RGB`, `#RGBA`, `#RRGGBB`, `#RRGGBBAA`, and
`rgb()` values. The HTML parser supports headings, paragraphs, semantic emphasis,
blockquotes, lists, tables, links, image alt text, and static form controls.
It handles common omitted end tags, custom element names, comments, template
contents, numeric character references, and common named character references.
Entities are decoded in text and displayed text attributes. Links support
absolute, relative, and same-page `#fragment` destinations. Layout is still
text-based: basic block widths and horizontal padding affect wrapping, and
backgrounds/borders follow the resulting lines. Rounded corners and font sizing
are not implemented. The parser retains a bounded element/text tree, but the
painter still lays out a separate text stream. Simple CSS Grid layouts with
equal-width `repeat(N, 1fr)` columns and approximate gaps form columns; nested
grids and other grid track definitions are not laid out. Flex content still
flows in document order rather than forming rows or columns; center-aligned flex
blocks approximate viewport-based `min-height` with blank lines, leaving less
space above the content; the added top spacing is limited to about one tenth of
the minimum height, and no extra blank space is added below. Polish UTF-8
letters are transliterated to ASCII because the system font renderer is
single-byte. The browser does not execute JavaScript or handle form input.
Local files, network responses, and linked stylesheets can be up
to 1 MiB; rendered text is limited by the browser's 768-line display capacity.

When the page has keyboard focus, press Enter once to focus the address field,
type or edit the address, then press Enter again to open it. The first Enter
shows a prompt in the status line.

When an application captures the arrow keys, hold Alt and use the arrow keys
to move the system cursor instead.

```
user@nasua-os:/home> bootapp --app WebBrowser
```

### suaedit

A text editor with an integrated terminal panel.

|   Property  | Value                                                      |
|-------------|------------------------------------------------------------|
| Type        | GUI window                                                 |
| Entry       | `_start()` in `utilities/applications/suaedit/src/suaedit.cpp` |
| API         | Uses `napp_gui` for drawing, `napp_window` for callbacks, `api->set_print_redirect` + `api->execute_command` for integrated commands |
| Description | A lightweight text editor with a VS Code-style menu bar; File contains New, Open, and Save, with Run, Help, and Terminal on the top bar. Run or F5 compiles and executes the current buffer as SuaScript in the integrated terminal. It supports typed `int`, `bool`, and `string` variables, arithmetic and loop control, and uses `.sua` as the script extension. It opens and creates files in the current folder; the editor supports files up to 65,535 bytes, while ClawFS supports files up to 1 MiB. On a formatted ClawFS disk, it creates and opens `info.txt`. Run `format` in the integrated terminal if the disk has not been initialized. |

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
bootapp --app WebBrowser   # Open the HTML/CSS web browser
bootapp --app suaedit      # Launch text editor with integrated terminal
bootapp --app snake        # Launch Snake game
```

See [NAPP Running](napp/running.md) for the full application loading lifecycle.

## Downloading Files from GitHub

The `download` system command reads a public file catalog and downloads ordinary
files over verified HTTPS:

```text
download --link https://github.com/OWNER/REPO list
download --link https://github.com/OWNER/REPO get example.txt
```

The repository contains a `catalog.txt` manifest and a `files/` directory. Each
catalog entry records the destination filename, file version, SHA-256, and path
inside the repository. Downloads are verified against that hash before being
saved to `/home/user/downloads`; existing files are not overwritten. Files are
limited to 60,000 bytes. HTTPS and a catalog hash verify transport and
integrity, but do not authenticate the publisher, so download files only from
repositories you trust.

GitHub links are resolved through `raw.githubusercontent.com`. Other HTTPS
links can point to a compatible file server; the command requests
`/<link-path>/<branch>/catalog.txt` and files below that same branch path. This
lets a self-hosted server use the same catalog and file layout.

Create a public GitHub repository with this structure, then replace
`OWNER/REPO` in the commands with its URL:

```text
catalog.txt
files/
└── example.txt
```

The catalog entry for `example.txt` looks like this:

```text
NASUA-FILE-CATALOG-1
example.txt|1.0.0|91d5cc20a5e6696f6a3549a76b6ff40c9855fa7d5de017942d9af69ee871e53e|files/example.txt
```
