#include <napp.h>

NAPP_APPLICATION("terminal", "Graphical shell terminal", true);

static const napp_api* terminal_api = nullptr;
static constexpr uint32_t MAX_OUTPUT_LINES = 64;
static constexpr uint32_t MAX_LINE_LENGTH = 192;

struct terminal_state
{
    char input[256];
    uint32_t input_length;
    char output[MAX_OUTPUT_LINES][MAX_LINE_LENGTH];
    uint32_t output_count;
    uint32_t scroll_offset;
};

static terminal_state terminal{};
static constexpr uint32_t COLOR_BACKGROUND = 0xFF11151D;
static constexpr uint32_t COLOR_TEXT = 0xFFE1E7EF;
static constexpr uint32_t COLOR_MUTED = 0xFF9BA8B8;
static constexpr uint32_t COLOR_ACCENT = 0xFF59E81B;

static void clear_output()
{
    terminal.output_count = 1;
    terminal.output[0][0] = '\0';
    terminal.scroll_offset = 0;
}

static void terminal_write_char(char character)
{
    if (terminal.output_count == 0)
    {
        clear_output();
    }

    if (character == '\r')
    {
        return;
    }
    if (character == '\n')
    {
        if (terminal.output_count < MAX_OUTPUT_LINES)
        {
            terminal.output[terminal.output_count++][0] = '\0';
        }
        else
        {
            for (uint32_t index = 1; index < MAX_OUTPUT_LINES; ++index)
            {
                for (uint32_t column = 0; column < MAX_LINE_LENGTH; ++column)
                {
                    terminal.output[index - 1][column] = terminal.output[index][column];
                }
            }
            terminal.output[MAX_OUTPUT_LINES - 1][0] = '\0';
        }
        terminal.scroll_offset = 0;
        return;
    }

    uint32_t line = terminal.output_count - 1;
    uint32_t length = 0;
    while (terminal.output[line][length] != '\0' && length + 1 < MAX_LINE_LENGTH)
    {
        ++length;
    }
    if (length + 1 < MAX_LINE_LENGTH)
    {
        terminal.output[line][length] = character;
        terminal.output[line][length + 1] = '\0';
    }
    else
    {
        terminal_write_char('\n');
        terminal_write_char(character);
    }
}

static void terminal_write_string(const char* text)
{
    if (text == nullptr)
    {
        return;
    }
    while (*text != '\0')
    {
        terminal_write_char(*text++);
    }
}

static void draw_terminal(napp_window* window)
{
    if (window == nullptr || terminal_api == nullptr || terminal_api->gui == nullptr)
    {
        return;
    }

    const int content_y = window->pos_y + window->title_height;
    const int content_height = window->height - window->title_height;
    if (content_height <= 0 || window->width <= 0)
    {
        return;
    }
    terminal_api->gui->fill_block(window->pos_x, content_y, COLOR_BACKGROUND, window->width, content_height);

    const int left = window->pos_x + 10;
    const int width = window->width - 20;
    const int line_height = 14;
    const int line_capacity = (content_height - 42) / line_height;
    const uint32_t visible = line_capacity > 0 ? static_cast<uint32_t>(line_capacity) : 1;
    const uint32_t total = terminal.output_count;
    uint32_t first = total > visible ? total - visible : 0;
    if (terminal.scroll_offset > first)
    {
        terminal.scroll_offset = first;
    }
    first -= terminal.scroll_offset;

    terminal_api->gui->draw_text("NasuaOS Terminal", left, content_y + 8, COLOR_ACCENT);
    for (uint32_t row = 0; row < visible && first + row < total; ++row)
    {
        terminal_api->gui->draw_text(terminal.output[first + row], left,
                                     content_y + 30 + static_cast<int>(row * line_height),
                                     COLOR_TEXT);
    }

    char prompt[260] = "> ";
    uint32_t index = 2;
    while (terminal.input[index - 2] != '\0' && index + 1 < sizeof(prompt))
    {
        prompt[index] = terminal.input[index - 2];
        ++index;
    }
    prompt[index++] = '_';
    prompt[index] = '\0';
    terminal_api->gui->fill_block(left, content_y + content_height - 26,
                                  COLOR_BACKGROUND, width, 24);
    terminal_api->gui->draw_text(prompt, left, content_y + content_height - 22, COLOR_TEXT);
}

static bool is_clear_command()
{
    static const char clear[] = "clear";
    for (uint32_t index = 0; index < sizeof(clear) - 1; ++index)
    {
        if (terminal.input[index] != clear[index])
        {
            return false;
        }
    }
    return terminal.input[sizeof(clear) - 1] == '\0' ||
           terminal.input[sizeof(clear) - 1] == ' ';
}

static void terminal_key(napp_window*, char key)
{
    if (key == '\n')
    {
        terminal_write_string("> ");
        terminal_write_string(terminal.input);
        terminal_write_char('\n');
        if (is_clear_command())
        {
            clear_output();
        }
        else if (terminal.input[0] != '\0' && terminal_api != nullptr &&
                 terminal_api->execute_command != nullptr)
        {
            terminal_api->execute_command(terminal.input);
        }
        terminal.input_length = 0;
        terminal.input[0] = '\0';
        return;
    }

    if (key == '\b')
    {
        if (terminal.input_length > 0)
        {
            terminal.input[--terminal.input_length] = '\0';
        }
        return;
    }
    if (key == NAPP_KEY_F5)
    {
        clear_output();
        return;
    }
    if (key == NAPP_KEY_UP || key == 0x0B)
    {
        if (terminal.scroll_offset + 1 < terminal.output_count)
        {
            ++terminal.scroll_offset;
        }
        return;
    }
    if (key == NAPP_KEY_DOWN || key == 0x0C)
    {
        if (terminal.scroll_offset > 0)
        {
            --terminal.scroll_offset;
        }
        return;
    }
    if (key >= 0x20 && key < 0x7F && terminal.input_length + 1 < sizeof(terminal.input))
    {
        terminal.input[terminal.input_length++] = key;
        terminal.input[terminal.input_length] = '\0';
    }
}

static void terminal_close(napp_window*)
{
    if (terminal_api != nullptr && terminal_api->set_print_redirect != nullptr)
    {
        terminal_api->set_print_redirect(nullptr);
    }
    terminal_api = nullptr;
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version < 10 || api->gui == nullptr ||
        api->gui->open_window == nullptr || api->gui->fill_block == nullptr ||
        api->gui->draw_text == nullptr || api->execute_command == nullptr ||
        api->set_print_redirect == nullptr)
    {
        return 1;
    }

    terminal_api = api;
    terminal.input_length = 0;
    terminal.input[0] = '\0';
    terminal.output_count = 0;
    terminal.scroll_offset = 0;
    for (uint32_t index = 0; index < MAX_OUTPUT_LINES; ++index)
    {
        terminal.output[index][0] = '\0';
    }
    clear_output();
    terminal_write_string("NasuaOS Terminal\nType a command and press Enter.\n");
    api->set_print_redirect(terminal_write_char);

    const napp_window_config config =
    {
        "Terminal", 560, 370, true, true, nullptr,
        draw_terminal, terminal_key, nullptr, nullptr, nullptr, 0, nullptr, terminal_close
    };
    if (!api->gui->open_window(&config))
    {
        api->set_print_redirect(nullptr);
        terminal_api = nullptr;
        return 1;
    }
    return 0;
}
