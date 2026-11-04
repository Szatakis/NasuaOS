#include <napp.h>

NAPP_APPLICATION("settings", "System settings and information", true);

static const napp_api* settings_api = nullptr;
static int settings_page = 0;

static constexpr int PAGE_COUNT = 4;
static constexpr int CHAR_WIDTH = 9;
static constexpr int CHAR_HEIGHT = 13;

static const char* page_name(int page)
{
    switch (page)
    {
        case 0:
            return "System";
        case 1:
            return "Display";
        case 2:
            return "Appearance";
        case 3:
            return "About";
        default:
            return "";
    }
}

static void draw_text_clipped(const char* text, int x, int y, int right_limit, int bottom_limit, uint32_t color)
{
    if (settings_api == nullptr || settings_api->gui == nullptr || settings_api->gui->draw_text == nullptr || text == nullptr || y >= bottom_limit || y + CHAR_HEIGHT <= 0 || x >= right_limit)
    {
        return;
    }

    const int max_chars = (right_limit - x) / CHAR_WIDTH;
    if (max_chars <= 0)
    {
        return;
    }

    char clipped[96];
    int index = 0;
    while (text[index] != '\0' && index < max_chars && index + 1 < static_cast<int>(sizeof(clipped)))
    {
        clipped[index] = text[index];
        ++index;
    }
    clipped[index] = '\0';
    settings_api->gui->draw_text(clipped, x, y, color);
}

static void draw_panel(int x, int y, int width, int height, uint32_t color)
{
    if (settings_api == nullptr || settings_api->gui == nullptr || settings_api->gui->fill_block == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    settings_api->gui->fill_block(x, y, color, width, height);
}

static void draw_border(int x, int y, int width, int height, uint32_t color)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    draw_panel(x, y, width, 1, color);
    draw_panel(x, y + height - 1, width, 1, color);
    draw_panel(x, y, 1, height, color);
    draw_panel(x + width - 1, y, 1, height, color);
}

static void draw_page_button(int x, int y, int width, int height, const char* text, bool selected)
{
    if (width <= 0 || height <= 0 || text == nullptr || settings_api == nullptr || settings_api->gui == nullptr || settings_api->gui->draw_text == nullptr)
    {
        return;
    }

    const uint32_t background = selected ? 0x252D3A : 0x181D25;
    const uint32_t border = selected ? 0x4A5568 : 0x252B35;
    const uint32_t text_color = selected ? 0xFFFFFFFF : 0xA8B0BC;

    draw_panel(x, y, width, height, background);
    draw_border(x, y, width, height, border);
    settings_api->gui->draw_text(text, x + 10, y + (height - 8) / 2, text_color);
}

static void append_number(char* output, uint32_t capacity, uint64_t value)
{
    if (capacity == 0)
    {
        return;
    }

    char digits[24];
    uint32_t count = 0;
    do
    {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (value != 0 && count < sizeof(digits));

    uint32_t index = 0;
    while (count > 0 && index + 1 < capacity)
    {
        output[index++] = digits[--count];
    }
    output[index] = '\0';
}

static void format_megabytes(char* output, uint32_t capacity, uint64_t bytes)
{
    if (capacity == 0)
    {
        return;
    }

    char number[24];
    append_number(number, sizeof(number), bytes / (1024 * 1024));
    uint32_t index = 0;
    while (number[index] != '\0' && index + 1 < capacity)
    {
        output[index] = number[index];
        ++index;
    }
    const char suffix[] = " MB";
    for (uint32_t character = 0; suffix[character] != '\0' && index + 1 < capacity; ++character)
    {
        output[index++] = suffix[character];
    }
    output[index] = '\0';
}

static void format_resolution(char* output, uint32_t capacity, uint64_t width, uint64_t height)
{
    if (capacity < 2)
    {
        return;
    }

    char width_text[24];
    char height_text[24];
    append_number(width_text, sizeof(width_text), width);
    append_number(height_text, sizeof(height_text), height);

    uint32_t index = 0;
    for (uint32_t character = 0; width_text[character] != '\0' && index + 1 < capacity; ++character)
    {
        output[index++] = width_text[character];
    }
    if (index + 1 < capacity)
    {
        output[index++] = 'x';
    }
    for (uint32_t character = 0; height_text[character] != '\0' && index + 1 < capacity; ++character)
    {
        output[index++] = height_text[character];
    }
    output[index] = '\0';
}

static void draw_system_page(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const int right = x + width;
    const int bottom = y + height;
    draw_text_clipped("System Information", x, y, right, bottom, 0xFFFFFFFF);

    napp_system_info info{};
    const bool has_system_info = settings_api != nullptr && settings_api->abi_version >= 10 && settings_api->get_system_info != nullptr && settings_api->get_system_info(&info);

    draw_text_clipped("Operating System:", x, y + 28, right, bottom, 0x9AA4B2);
    draw_text_clipped("NasuaOS", x + 160, y + 28, right, bottom, 0xFFFFFFFF);

    draw_text_clipped("Architecture:", x, y + 50, right, bottom, 0x9AA4B2);
    if (has_system_info)
    {
        draw_text_clipped(info.architecture, x + 160, y + 50, right, bottom, 0xFFFFFFFF);
    }

    draw_text_clipped("Kernel:", x, y + 72, right, bottom, 0x9AA4B2);
    draw_text_clipped("NasuaOS Kernel", x + 160, y + 72, right, bottom, 0xFFFFFFFF);

    draw_text_clipped("Graphics:", x, y + 94, right, bottom, 0x9AA4B2);
    draw_text_clipped("Framebuffer", x + 160, y + 94, right, bottom, 0xFFFFFFFF);

    draw_text_clipped("Resolution:", x, y + 116, right, bottom, 0x9AA4B2);
    if (has_system_info)
    {
        char resolution[48];
        format_resolution(resolution, sizeof(resolution), info.framebuffer_width, info.framebuffer_height);
        draw_text_clipped(resolution, x + 160, y + 116, right, bottom, 0xFFFFFFFF);
    }

    draw_text_clipped("Memory Size:", x, y + 146, right, bottom, 0x9AA4B2);
    if (has_system_info)
    {
        char memory[40];
        format_megabytes(memory, sizeof(memory), info.memory_total_bytes);
        draw_text_clipped(memory, x + 160, y + 146, right, bottom, 0xFFFFFFFF);
    }

    draw_text_clipped("Disk Size:", x, y + 168, right, bottom, 0x9AA4B2);
    if (has_system_info)
    {
        char storage[40];
        format_megabytes(storage, sizeof(storage), info.storage_total_bytes);
        draw_text_clipped(storage, x + 160, y + 168, right, bottom, 0xFFFFFFFF);
    }
}

static int get_sidebar_width(int content_width)
{
    int sidebar_width = content_width < 400 ? 110 : 130;
    if (sidebar_width > content_width - 80)
    {
        sidebar_width = content_width / 3;
    }
    if (sidebar_width < 80)
    {
        sidebar_width = 80;
    }
    if (sidebar_width >= content_width)
    {
        sidebar_width = content_width;
    }
    return sidebar_width;
}

static void draw_settings(napp_window* window)
{
    if (window == nullptr || window->width <= 0 || window->height <= 0)
    {
        return;
    }

    const int titlebar_height = window->title_height;
    if (window->height <= titlebar_height)
    {
        return;
    }

    const int content_x = window->pos_x;
    const int content_y = window->pos_y + titlebar_height;
    const int content_width = window->width;
    const int content_height = window->height - titlebar_height;
    const int sidebar_width = get_sidebar_width(content_width);

    draw_panel(content_x, content_y, content_width, content_height, 0x11151C);
    draw_panel(content_x, content_y, sidebar_width, content_height, 0x151A22);
    if (sidebar_width < content_width)
    {
        draw_panel(content_x + sidebar_width, content_y, 1, content_height, 0x2A303A);
    }

    const int button_x = content_x + 8;
    int button_y = content_y + 8;
    const int button_width = sidebar_width - 16;
    const int button_height = 30;
    for (int page = 0; page < PAGE_COUNT; ++page)
    {
        if (button_y >= content_y + content_height)
        {
            break;
        }

        int draw_height = button_height;
        if (button_y + draw_height > content_y + content_height)
        {
            draw_height = content_y + content_height - button_y;
        }
        if (draw_height <= 0)
        {
            break;
        }

        draw_page_button(button_x, button_y, button_width, draw_height, page_name(page), settings_page == page);
        button_y += button_height + 6;
    }

    const int page_x = content_x + sidebar_width + 20;
    const int page_y = content_y + 18;
    const int page_width = content_width - sidebar_width - 32;
    const int page_height = content_height - 28;
    if (page_width <= 0 || page_height <= 0)
    {
        return;
    }

    if (settings_page == 0)
    {
        draw_system_page(page_x, page_y, page_width, page_height);
    }
    else
    {
        draw_text_clipped(page_name(settings_page), page_x, page_y, page_x + page_width, page_y + page_height, 0xFFFFFFFF);
    }
}

static void settings_mouse(napp_window* window, int mouse_x, int mouse_y)
{
    if (window == nullptr)
    {
        return;
    }

    const int content_x = window->pos_x;
    const int content_y = window->pos_y + window->title_height;
    const int content_width = window->width;
    const int sidebar_width = get_sidebar_width(content_width);
    const int button_x = content_x + 8;
    int button_y = content_y + 8;
    const int button_width = sidebar_width - 16;
    const int button_height = 30;

    for (int page = 0; page < PAGE_COUNT; ++page)
    {
        if (mouse_x >= button_x && mouse_x < button_x + button_width && mouse_y >= button_y && mouse_y < button_y + button_height)
        {
            settings_page = page;
            return;
        }
        button_y += button_height + 6;
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version < 10 || api->gui == nullptr || api->gui->open_window == nullptr || api->gui->fill_block == nullptr || api->gui->draw_text == nullptr)
    {
        return 1;
    }

    settings_api = api;
    settings_page = 0;
    const napp_window_config config =
    {
        "Settings", 500, 350, true, true, nullptr,
        draw_settings, nullptr, settings_mouse, nullptr, nullptr, 0, nullptr, nullptr
    };
    return api->gui->open_window(&config) ? 0 : 1;
}
