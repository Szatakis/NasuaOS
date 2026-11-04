#include <napp.h>

NAPP_APPLICATION("terminal", "Graphical shell terminal", true);

static const napp_api* terminal_api = nullptr;
static constexpr uint32_t MAX_OUTPUT_LINES = 64;
static constexpr uint32_t MAX_LINE_LENGTH = 192;
static constexpr uint32_t CHARACTER_WIDTH = 9;
static constexpr uint32_t LINE_HEIGHT = 14;
static constexpr uint32_t DEFAULT_WINDOW_WIDTH = 750;

struct terminal_state
{
    char input[256];
    uint32_t input_length;
    char output[MAX_OUTPUT_LINES][MAX_LINE_LENGTH];
    uint32_t output_colors[MAX_OUTPUT_LINES][MAX_LINE_LENGTH];
    uint32_t output_count;
    uint32_t scroll_offset;
    uint32_t characters_per_line;
    uint32_t visible_output_lines;
};

static terminal_state terminal{};
static constexpr uint32_t COLOR_BACKGROUND = 0xFF11151D;
static constexpr uint32_t COLOR_TEXT = 0xFFE1E7EF;
static constexpr uint32_t COLOR_ACCENT = 0xFF59E81B;

static uint32_t output_visual_line_count(uint32_t characters_per_line)
{
    uint32_t visual_lines = 0;
    for (uint32_t line = 0; line < terminal.output_count; ++line)
    {
        uint32_t length = 0;
        while (length + 1 < MAX_LINE_LENGTH && terminal.output[line][length] != '\0')
        {
            ++length;
        }
        visual_lines += length == 0 ? 1 : (length + characters_per_line - 1) / characters_per_line;
    }
    return visual_lines;
}

static uint32_t maximum_scroll_offset()
{
    const uint32_t total_lines = output_visual_line_count(terminal.characters_per_line);
    return total_lines > terminal.visible_output_lines ? total_lines - terminal.visible_output_lines : 0;
}

static void clear_output()
{
    terminal.output_count = 1;
    terminal.output[0][0] = '\0';
    terminal.scroll_offset = 0;
}

static void terminal_write_char_colored(char character, uint32_t color)
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
                    terminal.output_colors[index - 1][column] = terminal.output_colors[index][column];
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
        terminal.output_colors[line][length] = color;
        terminal.output[line][length + 1] = '\0';
    }
    else
    {
        terminal_write_char_colored('\n', color);
        terminal_write_char_colored(character, color);
    }
}

static void terminal_write_char(char character)
{
    terminal_write_char_colored(character, COLOR_TEXT);
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
    terminal.characters_per_line = width > 0 ?
        static_cast<uint32_t>(width) / CHARACTER_WIDTH : 1;
    if (terminal.characters_per_line == 0)
    {
        terminal.characters_per_line = 1;
    }

    terminal_api->gui->draw_text("NasuaOS Terminal", left, content_y + 8, COLOR_ACCENT);
    char prompt[260] = "> ";
    uint32_t prompt_length = 2;
    for (uint32_t index = 0; index < terminal.input_length && prompt_length + 1 < sizeof(prompt); ++index)
    {
        prompt[prompt_length++] = terminal.input[index];
    }
    prompt[prompt_length++] = '_';
    prompt[prompt_length] = '\0';

    const uint32_t prompt_lines = (prompt_length + terminal.characters_per_line - 1) / terminal.characters_per_line;
    const int prompt_area_height = static_cast<int>(prompt_lines * LINE_HEIGHT + 8);
    const int prompt_y = content_y + content_height - prompt_area_height;
    terminal.visible_output_lines = prompt_y > content_y + 30 ? static_cast<uint32_t>((prompt_y - (content_y + 30)) / LINE_HEIGHT) : 0;

    terminal_api->gui->fill_block(left, prompt_y, COLOR_BACKGROUND, width, prompt_area_height);
    for (uint32_t line = 0; line < prompt_lines; ++line)
    {
        const uint32_t start = line * terminal.characters_per_line;
        uint32_t length = prompt_length - start;
        if (length > terminal.characters_per_line)
        {
            length = terminal.characters_per_line;
        }
        char prompt_line[260];
        for (uint32_t index = 0; index < length; ++index)
        {
            prompt_line[index] = prompt[start + index];
        }
        prompt_line[length] = '\0';
        terminal_api->gui->draw_text(prompt_line, left, prompt_y + static_cast<int>(line * LINE_HEIGHT), COLOR_TEXT);
    }

    const uint32_t max_scroll = maximum_scroll_offset();
    if (terminal.scroll_offset > max_scroll)
    {
        terminal.scroll_offset = max_scroll;
    }
    const uint32_t first_visual_line = max_scroll - terminal.scroll_offset;
    uint32_t visual_line = 0;
    uint32_t output_row = 0;
    for (uint32_t line = 0; line < terminal.output_count && output_row < terminal.visible_output_lines; ++line)
    {
        uint32_t length = 0;
        while (length + 1 < MAX_LINE_LENGTH && terminal.output[line][length] != '\0')
        {
            ++length;
        }
        const uint32_t wrapped_lines = length == 0 ? 1 : (length + terminal.characters_per_line - 1) / terminal.characters_per_line;
        for (uint32_t wrapped_line = 0; wrapped_line < wrapped_lines; ++wrapped_line)
        {
            if (visual_line >= first_visual_line && visual_line < first_visual_line + terminal.visible_output_lines)
            {
                const uint32_t start = wrapped_line * terminal.characters_per_line;
                uint32_t chunk_length = length > start ? length - start : 0;
                if (chunk_length > terminal.characters_per_line)
                {
                    chunk_length = terminal.characters_per_line;
                }
                if (chunk_length > 0)
                {
                    uint32_t run_start = 0;
                    while (run_start < chunk_length)
                    {
                        const uint32_t color = terminal.output_colors[line][start + run_start];
                        uint32_t run_end = run_start + 1;
                        while (run_end < chunk_length &&
                            terminal.output_colors[line][start + run_end] == color)
                        {
                            ++run_end;
                        }

                        char output_run[MAX_LINE_LENGTH];
                        const uint32_t run_length = run_end - run_start;
                        for (uint32_t index = 0; index < run_length; ++index)
                        {
                            output_run[index] = terminal.output[line][start + run_start + index];
                        }
                        output_run[run_length] = '\0';
                        terminal_api->gui->draw_text(output_run,
                            left + static_cast<int>(run_start * CHARACTER_WIDTH),
                            content_y + 30 + static_cast<int>(output_row * LINE_HEIGHT),
                            color);
                        run_start = run_end;
                    }
                }
                ++output_row;
            }
            ++visual_line;
        }
    }
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
    return terminal.input[sizeof(clear) - 1] == '\0' || terminal.input[sizeof(clear) - 1] == ' ';
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
        else if (terminal.input[0] != '\0' && terminal_api != nullptr && terminal_api->execute_command != nullptr)
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
        if (terminal.scroll_offset < maximum_scroll_offset())
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
    if (terminal_api != nullptr && terminal_api->abi_version >= 11 &&
        terminal_api->set_print_redirect_colored != nullptr)
    {
        terminal_api->set_print_redirect_colored(nullptr);
    }
    if (terminal_api != nullptr && terminal_api->set_print_redirect != nullptr)
    {
        terminal_api->set_print_redirect(nullptr);
    }
    terminal_api = nullptr;
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version < 10 || api->gui == nullptr || api->gui->open_window == nullptr || api->gui->fill_block == nullptr || api->gui->draw_text == nullptr || api->execute_command == nullptr || api->set_print_redirect == nullptr)
    {
        return 1;
    }

    terminal_api = api;
    terminal.input_length = 0;
    terminal.input[0] = '\0';
    terminal.output_count = 0;
    terminal.scroll_offset = 0;
    terminal.characters_per_line = (DEFAULT_WINDOW_WIDTH - 20) / CHARACTER_WIDTH;
    terminal.visible_output_lines = 1;
    for (uint32_t index = 0; index < MAX_OUTPUT_LINES; ++index)
    {
        terminal.output[index][0] = '\0';
    }
    clear_output();
    terminal_write_string("NasuaOS Terminal\nType a command and press Enter.\n");
    if (api->abi_version >= 11 && api->set_print_redirect_colored != nullptr)
    {
        api->set_print_redirect_colored(terminal_write_char_colored);
    }
    else
    {
        api->set_print_redirect(terminal_write_char);
    }

    const napp_window_config config =
    {
        "Terminal", 750, 500, true, true, nullptr,
        draw_terminal, terminal_key, nullptr, nullptr, nullptr, 0, nullptr, terminal_close
    };
    if (!api->gui->open_window(&config))
    {
        if (api->abi_version >= 11 && api->set_print_redirect_colored != nullptr)
        {
            api->set_print_redirect_colored(nullptr);
        }
        api->set_print_redirect(nullptr);
        terminal_api = nullptr;
        return 1;
    }
    return 0;
}
