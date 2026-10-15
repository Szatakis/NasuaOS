#include <napp.h>

NAPP_APPLICATION("browser", "Simple HTML browser", true);

#define BROWSER_ADDRESS_MAX 96
#define BROWSER_DOCUMENT_MAX 512
#define BROWSER_MAX_LINES 128
#define BROWSER_LINE_MAX 120

static const napp_api* browser_api = nullptr;
static const napp_gui* browser_gui = nullptr;

static char browser_address[BROWSER_ADDRESS_MAX];
static char browser_document[BROWSER_DOCUMENT_MAX];
static char browser_lines[BROWSER_MAX_LINES][BROWSER_LINE_MAX];
static int browser_line_count = 0;
static int browser_scroll = 0;
static int browser_columns = 0;
static bool browser_address_focused = true;
static bool browser_truncated = false;
static bool browser_source_truncated = false;
static const char* browser_status = "Enter a local HTML file path and press Enter";
static uint32_t browser_status_color = NAPP_COLOR_GRAY;

static int browser_length(const char* text)
{
    int length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}

static bool browser_equal_ignore_case(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0')
    {
        char a = *left++;
        char b = *right++;
        if (a >= 'A' && a <= 'Z')
        {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }
    return *left == '\0' && *right == '\0';
}

static void browser_set_document(const char* text)
{
    int index = 0;
    while (text[index] != '\0' && index < BROWSER_DOCUMENT_MAX - 1)
    {
        browser_document[index] = text[index];
        ++index;
    }
    browser_document[index] = '\0';
    browser_columns = 0;
    browser_scroll = 0;
}

static bool browser_append_document(char character, bool& pending_space)
{
    int length = browser_length(browser_document);
    if (character == '\n')
    {
        while (length > 0 && browser_document[length - 1] == ' ')
        {
            browser_document[--length] = '\0';
        }
        if (length > 0 && browser_document[length - 1] != '\n')
        {
            if (length < BROWSER_DOCUMENT_MAX - 1)
            {
                browser_document[length] = '\n';
                browser_document[length + 1] = '\0';
            }
        }
        pending_space = false;
        return length < BROWSER_DOCUMENT_MAX - 1;
    }

    if (character == ' ' || character == '\t' || character == '\r')
    {
        pending_space = true;
        return true;
    }

    if (pending_space && length > 0 && browser_document[length - 1] != '\n')
    {
        if (length >= BROWSER_DOCUMENT_MAX - 1)
        {
            return false;
        }
        browser_document[length++] = ' ';
    }
    pending_space = false;
    if (length >= BROWSER_DOCUMENT_MAX - 1)
    {
        return false;
    }
    browser_document[length] = character;
    browser_document[length + 1] = '\0';
    return true;
}

static bool browser_tag_is_block(const char* tag)
{
    return browser_equal_ignore_case(tag, "p")
        || browser_equal_ignore_case(tag, "div")
        || browser_equal_ignore_case(tag, "h1")
        || browser_equal_ignore_case(tag, "h2")
        || browser_equal_ignore_case(tag, "h3")
        || browser_equal_ignore_case(tag, "h4")
        || browser_equal_ignore_case(tag, "h5")
        || browser_equal_ignore_case(tag, "h6")
        || browser_equal_ignore_case(tag, "li")
        || browser_equal_ignore_case(tag, "ul")
        || browser_equal_ignore_case(tag, "ol")
        || browser_equal_ignore_case(tag, "tr")
        || browser_equal_ignore_case(tag, "section")
        || browser_equal_ignore_case(tag, "article")
        || browser_equal_ignore_case(tag, "blockquote")
        || browser_equal_ignore_case(tag, "pre");
}

static bool browser_decode_entity(const char* source, int& index, char& output)
{
    if (source[index] != '&')
    {
        return false;
    }

    const char* entities[] = {"amp;", "lt;", "gt;", "quot;", "apos;", "nbsp;"};
    const char values[] = {'&', '<', '>', '"', '\'', ' '};
    for (int entity = 0; entity < 6; ++entity)
    {
        int offset = 0;
        while (entities[entity][offset] != '\0' &&
               source[index + 1 + offset] == entities[entity][offset])
        {
            ++offset;
        }
        if (entities[entity][offset] == '\0')
        {
            output = values[entity];
            index += offset + 1;
            return true;
        }
    }
    return false;
}

static void browser_parse_html(const char* source)
{
    browser_document[0] = '\0';
    bool pending_space = false;
    bool skip_head = false;
    bool skip_script = false;
    bool skip_style = false;
    bool truncated = false;

    for (int index = 0; source[index] != '\0';)
    {
        if (source[index] == '<')
        {
            if (source[index + 1] == '!' && source[index + 2] == '-' && source[index + 3] == '-')
            {
                index += 4;
                while (source[index] != '\0' &&
                       !(source[index] == '-' && source[index + 1] == '-' && source[index + 2] == '>'))
                {
                    ++index;
                }
                if (source[index] != '\0')
                {
                    index += 3;
                }
                continue;
            }

            int end = index + 1;
            while (source[end] != '\0' && source[end] != '>' && end - index < 64)
            {
                ++end;
            }
            if (source[end] != '>')
            {
                if (!(skip_head || skip_script || skip_style) &&
                    !browser_append_document('<', pending_space))
                {
                    truncated = true;
                    break;
                }
                ++index;
                continue;
            }

            char tag[16] = {};
            int tag_index = index + 1;
            while (tag_index < end && (source[tag_index] == ' ' || source[tag_index] == '/'))
            {
                ++tag_index;
            }
            int tag_length = 0;
            while (tag_index < end && tag_length < static_cast<int>(sizeof(tag) - 1))
            {
                char character = source[tag_index];
                if (!((character >= 'A' && character <= 'Z') ||
                      (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9')))
                {
                    break;
                }
                tag[tag_length++] = character;
                ++tag_index;
            }
            const bool closing = source[index + 1] == '/';

            if (browser_equal_ignore_case(tag, "head"))
            {
                skip_head = !closing;
            }
            else if (browser_equal_ignore_case(tag, "script"))
            {
                skip_script = !closing;
            }
            else if (browser_equal_ignore_case(tag, "style"))
            {
                skip_style = !closing;
            }
            else if (!(skip_head || skip_script || skip_style))
            {
                if (browser_equal_ignore_case(tag, "br") || browser_equal_ignore_case(tag, "hr") ||
                    browser_tag_is_block(tag))
                {
                    if (!browser_append_document('\n', pending_space))
                    {
                        truncated = true;
                        break;
                    }
                }
            }
            index = end + 1;
            continue;
        }

        if (skip_head || skip_script || skip_style)
        {
            ++index;
            continue;
        }

        char character = source[index];
        if (browser_decode_entity(source, index, character))
        {
            if (!browser_append_document(character, pending_space))
            {
                truncated = true;
                break;
            }
            continue;
        }
        if (!browser_append_document(character, pending_space))
        {
            truncated = true;
            break;
        }
        ++index;
    }

    int length = browser_length(browser_document);
    while (length > 0 && (browser_document[length - 1] == ' ' || browser_document[length - 1] == '\n'))
    {
        browser_document[--length] = '\0';
    }
    browser_source_truncated = truncated;
    browser_truncated = truncated;
    browser_columns = 0;
    browser_scroll = 0;
}

static void browser_add_line(const char* text, int length)
{
    if (browser_line_count >= BROWSER_MAX_LINES)
    {
        browser_truncated = true;
        return;
    }

    if (length >= BROWSER_LINE_MAX)
    {
        length = BROWSER_LINE_MAX - 1;
    }
    for (int index = 0; index < length; ++index)
    {
        browser_lines[browser_line_count][index] = text[index];
    }
    browser_lines[browser_line_count][length] = '\0';
    ++browser_line_count;
}

static void browser_layout_document(int columns)
{
    browser_line_count = 0;
    browser_truncated = browser_source_truncated;
    char line[BROWSER_LINE_MAX] = {};
    int line_length = 0;

    for (int index = 0; browser_document[index] != '\0';)
    {
        if (browser_document[index] == '\n')
        {
            browser_add_line(line, line_length);
            line_length = 0;
            ++index;
            continue;
        }
        if (browser_document[index] == ' ')
        {
            ++index;
            continue;
        }

        char word[BROWSER_LINE_MAX] = {};
        int word_length = 0;
        while (browser_document[index] != '\0' && browser_document[index] != ' ' &&
               browser_document[index] != '\n' && word_length < BROWSER_LINE_MAX - 1)
        {
            word[word_length++] = browser_document[index++];
        }

        int word_offset = 0;
        while (word_offset < word_length)
        {
            if (line_length > 0 && line_length + 1 >= columns)
            {
                browser_add_line(line, line_length);
                line_length = 0;
            }
            if (line_length > 0)
            {
                line[line_length++] = ' ';
            }
            int available = columns - line_length;
            int remaining = word_length - word_offset;
            int copy_length = remaining < available ? remaining : available;
            if (copy_length <= 0)
            {
                browser_add_line(line, line_length);
                line_length = 0;
                continue;
            }
            for (int character = 0; character < copy_length; ++character)
            {
                line[line_length++] = word[word_offset++];
            }
            if (word_offset < word_length)
            {
                browser_add_line(line, line_length);
                line_length = 0;
            }
        }
    }

    if (line_length > 0 || browser_line_count == 0)
    {
        browser_add_line(line, line_length);
    }
    browser_columns = columns;
}

static bool browser_load_page()
{
    if (browser_address[0] == '\0')
    {
        browser_status = "Enter a local HTML file path";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    if (browser_api == nullptr || browser_api->clawfs_resolve_path == nullptr ||
        browser_api->clawfs_read_sector == nullptr)
    {
        browser_status = "Filesystem access is unavailable";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    const char* current_path = browser_api->current_path != nullptr ? browser_api->current_path : "/";
    const uint32_t sector = browser_api->clawfs_resolve_path(current_path, browser_address);
    if (sector == 0)
    {
        browser_status = "File not found";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    char source[512] = {};
    if (!browser_api->clawfs_read_sector(sector, source))
    {
        browser_status = "Could not read file";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    source[sizeof(source) - 1] = '\0';
    browser_parse_html(source);
    browser_status = browser_truncated ? "Page opened (content truncated)" : "Page opened";
    browser_status_color = browser_truncated ? 0xFFFFCC66 : NAPP_COLOR_GREEN;
    browser_address_focused = false;
    return true;
}

static void browser_draw(napp_window* window)
{
    const int left = window->pos_x + 10;
    const int content_top = window->pos_y + window->title_height;
    const int toolbar_y = content_top + 8;
    const int body_y = content_top + 58;
    const int body_height = window->height - window->title_height - 72;
    const int columns = (window->width - 32) / 8;

    browser_gui->fill_block(window->pos_x, content_top, NAPP_COLOR_WHITE,
                            window->width, window->height - window->title_height);
    browser_gui->fill_block(left, toolbar_y, browser_address_focused ? 0xFFE5F0FF : 0xFFF0F0F0,
                            window->width - 88, 24);
    browser_gui->draw_text(browser_address[0] != '\0' ? browser_address : "Local HTML file path",
                           left + 6, toolbar_y + 7, NAPP_COLOR_BLACK);
    browser_gui->fill_block(window->pos_x + window->width - 70, toolbar_y, 0xFF59E81B, 60, 24);
    browser_gui->draw_text("Open", window->pos_x + window->width - 57, toolbar_y + 7, NAPP_COLOR_BLACK);
    browser_gui->draw_text(browser_status, left, content_top + 38, browser_status_color);

    if (columns < 12)
    {
        browser_gui->draw_text("Window is too narrow", left, body_y, NAPP_COLOR_BLACK);
        return;
    }
    if (columns > BROWSER_LINE_MAX - 1)
    {
        browser_layout_document(BROWSER_LINE_MAX - 1);
    }
    else if (columns != browser_columns)
    {
        browser_layout_document(columns);
    }

    const int visible_rows = body_height > 0 ? body_height / 12 : 1;
    int max_scroll = browser_line_count - visible_rows;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }
    if (browser_scroll > max_scroll)
    {
        browser_scroll = max_scroll;
    }

    int rows_drawn = 0;
    for (int line = browser_scroll; line < browser_line_count && rows_drawn < visible_rows; ++line)
    {
        browser_gui->draw_text(browser_lines[line], left, body_y + rows_drawn * 12, NAPP_COLOR_BLACK);
        ++rows_drawn;
    }
    if (browser_truncated)
    {
        browser_gui->draw_text("[Document truncated]", left, body_y + rows_drawn * 12, 0xFFAA5500);
    }
}

static void browser_key(napp_window* window, char key)
{
    if (browser_address_focused)
    {
        if (key == '\n')
        {
            browser_load_page();
        }
        else if (key == '\b')
        {
            const int length = browser_length(browser_address);
            if (length > 0)
            {
                browser_address[length - 1] = '\0';
            }
        }
        else if (key >= 32 && key <= 126)
        {
            const int length = browser_length(browser_address);
            if (length < BROWSER_ADDRESS_MAX - 1)
            {
                browser_address[length] = key;
                browser_address[length + 1] = '\0';
            }
        }
        return;
    }

    const int page_rows = (window->height - window->title_height - 72) / 12;
    if (key == 'e' || key == 'g' || key == '\n')
    {
        browser_address_focused = true;
    }
    else if (key == 'j' || key == 0x0B)
    {
        browser_scroll += key == 0x0B && page_rows > 1 ? page_rows - 1 : 1;
    }
    else if (key == 'k' || key == 0x0C)
    {
        browser_scroll -= key == 0x0C && page_rows > 1 ? page_rows - 1 : 1;
        if (browser_scroll < 0)
        {
            browser_scroll = 0;
        }
    }
}

static void browser_mouse(napp_window* window, int mouse_x, int mouse_y)
{
    const int content_top = window->pos_y + window->title_height;
    const int toolbar_y = content_top + 8;
    if (mouse_y >= toolbar_y && mouse_y < toolbar_y + 26)
    {
        if (mouse_x >= window->pos_x + window->width - 72)
        {
            browser_load_page();
        }
        else
        {
            browser_address_focused = true;
        }
    }
    else
    {
        browser_address_focused = false;
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->gui == nullptr || api->gui->open_window == nullptr ||
        api->gui->draw_text == nullptr || api->gui->fill_block == nullptr)
    {
        return 1;
    }

    browser_api = api;
    browser_gui = api->gui;
    browser_address[0] = '\0';
    browser_source_truncated = false;
    browser_truncated = false;
    browser_set_document(
        "Welcome to Nasua Browser\n"
        "\n"
        "This preview renders basic HTML from local ClawFS files.\n"
        "Enter a file path above, for example /home/user/page.html, then press Enter.\n"
        "\n"
        "Supported: headings, paragraphs, lists, line breaks and common text entities.\n"
        "Use j/k to scroll, Page Up/Down to move a page, or e to edit the path.\n"
        "\n"
        "Internet downloads are not available yet.");

    napp_window_config config = {};
    config.title = "Nasua Browser";
    config.width = 820;
    config.height = 620;
    config.resizable = true;
    config.can_maximize = true;
    config.draw = browser_draw;
    config.key = browser_key;
    config.mouse = browser_mouse;

    return api->gui->open_window(&config) ? 0 : 1;
}
