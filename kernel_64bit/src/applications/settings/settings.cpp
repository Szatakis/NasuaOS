#include "settings.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/cpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/disk/driver.hpp"
#include "system/gui/gui.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.hpp"

static int settings_page = 0;

static constexpr int PAGE_COUNT = 4;

static const char* page_names[PAGE_COUNT] =
{
    "System",
    "Display",
    "Appearance",
    "About"
};

static constexpr int CHAR_WIDTH = 9;
static constexpr int CHAR_HEIGHT = 13;


// CLIPPED TEXT
static void draw_text_clipped(const char* text, int x, int y, int right_limit, int bottom_limit, uint32_t color)
{
    if (!text || y >= bottom_limit || y + CHAR_HEIGHT <= 0 || x >= right_limit)
    {
        return;
    }

    int available_width = right_limit - x;

    if (available_width <= 0)
    {
        return;
    }

    int max_chars = available_width / CHAR_WIDTH;

    if (max_chars <= 0)
    {
        return;
    }

    int i = 0;

    while (text[i] && i < max_chars)
    {
        Gpu::draw_char8((unsigned char)text[i], x + i * CHAR_WIDTH, y, color);

        i++;
    }
}

static void format_resolution(char* buffer, size_t size, uint64_t width, uint64_t height)
{
    if (!buffer || size < 2)
    {
        return;
    }

    char temp[32];
    int pos = 0;

    // Width
    char digits[21];
    int digit_count = 0;

    if (width == 0)
    {
        digits[digit_count++] = '0';
    }
    else
    {
        while (width > 0 && digit_count < 20)
        {
            digits[digit_count++] = '0' + (width % 10);
            width /= 10;
        }
    }

    for (int i = digit_count - 1; i >= 0 && pos < 30; i--)
    {
        temp[pos++] = digits[i];
    }

    if (pos < 30)
    {
        temp[pos++] = 'x';
    }

    // Height
    digit_count = 0;

    if (height == 0)
    {
        digits[digit_count++] = '0';
    }
    else
    {
        while (height > 0 && digit_count < 20)
        {
            digits[digit_count++] = '0' + (height % 10);
            height /= 10;
        }
    }

    for (int i = digit_count - 1; i >= 0 && pos < 31; i--)
    {
        temp[pos++] = digits[i];
    }

    if ((size_t)pos >= size)
    {
        pos = (int)size - 1;
    }

    for (int i = 0; i < pos; i++)
    {
        buffer[i] = temp[i];
    }

    buffer[pos] = '\0';
}


// PANEL
static void draw_panel(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    Gpu::fill_block(x, y, color, w, h);
}


// PAGE BUTTON
static void draw_page_button(int x, int y, int w, int h, const char* text, bool selected, int right_limit, int bottom_limit)
{
    (void)right_limit;
    (void)bottom_limit;

    if (w <= 0 || h <= 0)
    {
        return;
    }

    uint32_t bg;
    uint32_t border;
    uint32_t text_color;

    if (selected)
    {
        bg = 0x252D3A;
        border = 0x4A5568;
        text_color = COLOR_WHITE;
    }
    else
    {
        bg = 0x181D25;
        border = 0x252B35;
        text_color = 0xA8B0BC;
    }

    draw_panel(x, y, w, h, bg);
    Gpu::draw_rect(x, y, x + w, y + h, border);
    draw_text_clipped(text, x + 10, y + ((h - (CHAR_HEIGHT / 2)) / 2), x + w - 10, y + h, text_color);
}


// SYSTEM PAGE
static void draw_system_page(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    const int right = x + w;
    const int bottom = y + h;


    // Header
    draw_text_clipped("System Information", x, y, right, bottom, COLOR_WHITE);


    // Operating System
    draw_text_clipped("Operating System:", x, y + 28, right, bottom, 0x9AA4B2);
    draw_text_clipped("NasuaOS", x + 160, y + 28, right, bottom, COLOR_WHITE);


    // Architecture
    draw_text_clipped("Architecture:", x, y + 50, right, bottom, 0x9AA4B2);
    draw_text_clipped(Cpu::get_architecture(), x + 160, y + 50, right, bottom, COLOR_WHITE);


    // Kernel
    draw_text_clipped("Kernel:", x, y + 72, right, bottom, 0x9AA4B2);
    draw_text_clipped("NasuaOS Kernel", x + 160, y + 72, right, bottom, COLOR_WHITE);


    // Graphics
    draw_text_clipped("Graphics:", x, y + 94, right, bottom, 0x9AA4B2);
    draw_text_clipped("Framebuffer", x + 160, y + 94, right, bottom, COLOR_WHITE);


    // Resolution
    draw_text_clipped("Resolution:", x, y + 116, right, bottom, 0x9AA4B2);

    if (Gpu::fb)
    {
        char resolution[32];

        format_resolution(resolution, sizeof(resolution), Gpu::fb->width, Gpu::fb->height);
        draw_text_clipped(resolution, x + 160, y + 116, right, bottom, COLOR_WHITE);
    }
    else
    {
        draw_text_clipped("Unknown", x + 160, y + 116, right, bottom, COLOR_WHITE);
    }


    // Memory
    draw_text_clipped("Memory Size:", x, y + 146, right, bottom, 0x9AA4B2);

    char memory[10];

    strcpy(memory, Memory::total());
    strcat(memory, " MB");

    draw_text_clipped(memory, x + 160, y + 146, right, bottom, COLOR_WHITE);


    // Disk
    draw_text_clipped("Disk Size:", x, y + 168, right, bottom, 0x9AA4B2);

    char storage[10];
    char storage_mb[10];

    uint64_to_string(Disk::total() / (1024 * 1024), storage_mb, sizeof(storage));

    strcpy(storage, storage_mb);
    strcat(storage, " MB");

    draw_text_clipped(storage, x + 160, y + 168, right, bottom, COLOR_WHITE);
}


// EMPTY PAGE
static void draw_empty_page(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    draw_text_clipped(page_names[settings_page], x, y, x + w, y + h, COLOR_WHITE);
}


// SETTINGS DRAW
void draw_settings(Gpu::Window_Manager::window_struct* win)
{
    if (!win || !Gpu::fb)
    {
        return;
    }

    const int window_x = win->pos_x;
    const int window_y = win->pos_y;

    const int window_w = win->width;
    const int window_h = win->height;

    if (window_w <= 0 || window_h <= 0)
    {
        return;
    }

    const int titlebar_h = 24;

    if (window_h <= titlebar_h)
    {
        return;
    }

    const int content_x = window_x;
    const int content_y = window_y + titlebar_h;

    const int content_w = window_w;
    const int content_h = window_h - titlebar_h;

    if (content_w <= 0 || content_h <= 0)
    {
        return;
    }


    // SIDEBAR WIDTH
    int sidebar_w = 130;

    if (content_w < 400)
    {
        sidebar_w = 110;
    }

    if (sidebar_w > content_w - 80)
    {
        sidebar_w = content_w / 3;
    }

    if (sidebar_w < 80)
    {
        sidebar_w = 80;
    }

    if (sidebar_w >= content_w)
    {
        sidebar_w = content_w;
    }


    // MAIN BACKGROUND
    draw_panel(content_x, content_y, content_w, content_h, 0x11151C);


    // SIDEBAR
    draw_panel(content_x, content_y, sidebar_w, content_h, 0x151A22);

    // Separator
    if (sidebar_w < content_w)
    {
        draw_panel(content_x + sidebar_w, content_y, 1, content_h, 0x2A303A);
    }

    // PAGE BUTTONS
    const int button_x = content_x + 8;
    int button_y = content_y + 8;
    const int button_w = sidebar_w - 16;
    const int button_h = 30;

    for (int i = 0; i < PAGE_COUNT; i++)
    {
        if (button_y >= content_y + content_h)
        {
            break;
        }

        int draw_h = button_h;

        if (button_y + draw_h > content_y + content_h)
        {
            draw_h = content_y + content_h - button_y;
        }

        if (draw_h <= 0)
        {
            break;
        }

        draw_page_button(button_x, button_y, button_w, draw_h, page_names[i], settings_page == i, content_x + sidebar_w, content_y + content_h);

        button_y += button_h + 6;
    }


    // PAGE CONTENT
    const int page_x = content_x + sidebar_w + 20;
    const int page_y = content_y + 18;
    const int page_w = content_w - sidebar_w - 32;
    const int page_h = content_h - 28;

    if (page_w <= 0 || page_h <= 0)
    {
        return;
    }

    if (settings_page == 0)
    {
        draw_system_page(page_x, page_y, page_w, page_h);
    }
    else
    {
        draw_empty_page(page_x, page_y, page_w, page_h);
    }
}


// MOUSE CLICK
static void settings_mouse_click(Gpu::Window_Manager::window_struct* win, int mouse_x, int mouse_y)
{
    if (!win)
    {
        return;
    }

    const int titlebar_h = 24;

    const int content_x = win->pos_x;
    const int content_y = win->pos_y + titlebar_h;

    const int content_w = win->width;
    const int content_h = win->height - titlebar_h;

    if (content_w <= 0 || content_h <= 0)
    {
        return;
    }

    int sidebar_w = 130;

    if (content_w < 400)
    {
        sidebar_w = 110;
    }

    if (sidebar_w > content_w - 80)
    {
        sidebar_w = content_w / 3;
    }

    if (sidebar_w < 80)
    {
        sidebar_w = 80;
    }

    const int button_x = content_x + 8;
    int button_y = content_y + 8;
    const int button_w = sidebar_w - 16;
    const int button_h = 30;

    for (int i = 0; i < PAGE_COUNT; i++)
    {
        if (mouse_x >= button_x && mouse_x < button_x + button_w && mouse_y >= button_y && mouse_y < button_y + button_h)
        {
            settings_page = i;
            return;
        }

        button_y += button_h + 6;
    }
}


// SETTINGS WINDOW
Gpu::Window_Manager::window_struct settings =
{
    .name = "Settings",
    .id = 0,

    .pos_x = 10,
    .pos_y = 10,

    .width = 500,
    .height = 350,

    .visible = false,
    .minimized = false,
    .focused = false,

    .resizable = true,
    .can_maximize = true,
    .maximized = false,

    .restore_pos_x = 0,
    .restore_pos_y = 0,
    .restore_width = 0,
    .restore_height = 0,

    .is_dragging = false,
    .drag_offset_x = 0,
    .drag_offset_y = 0,

    .is_resizing = false,
    .resize_right = false,
    .resize_bottom = false,
    .resize_start_mouse_x = 0,
    .resize_start_mouse_y = 0,
    .resize_start_width = 0,
    .resize_start_height = 0,
    .max_width = 0,
    .max_height = 0,

    .userdata = nullptr,
    .draw_content = draw_settings,
    .key_press = nullptr,
    .mouse_click = settings_mouse_click,
    .mouse_button = 0
};