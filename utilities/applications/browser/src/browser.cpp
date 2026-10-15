#include <napp.h>

NAPP_APPLICATION("browser", "Simple HTML browser", true);

#define BROWSER_ADDRESS_MAX 256
#define BROWSER_RESPONSE_MAX 8192
#define BROWSER_DOCUMENT_MAX 8192
#define BROWSER_MAX_LINES 512
#define BROWSER_LINE_MAX 120

static constexpr uint32_t BROWSER_BACKGROUND = 0xFF11151D;
static constexpr uint32_t BROWSER_SURFACE = 0xFF1B222D;
static constexpr uint32_t BROWSER_INPUT = 0xFF283240;
static constexpr uint32_t BROWSER_TEXT = 0xFFE1E7EF;
static constexpr uint32_t BROWSER_MUTED = 0xFF9BA8B8;
static constexpr uint32_t BROWSER_ACCENT = 0xFF59E81B;

static const napp_api* browser_api = nullptr;
static const napp_gui* browser_gui = nullptr;

static char browser_address[BROWSER_ADDRESS_MAX];
static char browser_response[BROWSER_RESPONSE_MAX];
static char browser_page_source[BROWSER_RESPONSE_MAX];
static char browser_document[BROWSER_DOCUMENT_MAX];
static char browser_lines[BROWSER_MAX_LINES][BROWSER_LINE_MAX];
static int browser_document_length = 0;
static int browser_line_count = 0;
static int browser_scroll = 0;
static int browser_columns = 0;
static bool browser_address_focused = true;
static bool browser_truncated = false;
static bool browser_source_truncated = false;
static char browser_status_text[64];
static const char* browser_status = "Enter an http:// URL or a local ClawFS file path";
static uint32_t browser_status_color = BROWSER_MUTED;

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

static bool browser_has_prefix_ignore_case(const char* text, const char* prefix)
{
    while (*prefix != '\0')
    {
        char character = *text++;
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
        char expected = *prefix++;
        if (expected >= 'A' && expected <= 'Z')
        {
            expected = static_cast<char>(expected - 'A' + 'a');
        }
        if (character != expected)
        {
            return false;
        }
    }
    return true;
}

static bool browser_parse_http_url(const char* url, char* hostname, int hostname_capacity, uint16_t& port, char* path, int path_capacity)
{
    const char* cursor = url;
    if (browser_has_prefix_ignore_case(cursor, "http://"))
    {
        cursor += 7;
    }
    else
    {
        return false;
    }

    int host_length = 0;
    while (*cursor != '\0' && *cursor != '/' && *cursor != '?' && *cursor != '#')
    {
        if (*cursor == '@' || *cursor == '[' || *cursor == ']' || *cursor == '\r' || *cursor == '\n')
        {
            return false;
        }
        if (*cursor == ':')
        {
            break;
        }
        if (host_length >= hostname_capacity - 1)
        {
            return false;
        }
        hostname[host_length++] = *cursor++;
    }
    hostname[host_length] = '\0';
    if (host_length == 0)
    {
        return false;
    }

    port = 80;
    if (*cursor == ':')
    {
        ++cursor;
        uint32_t parsed_port = 0;
        int digits = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            parsed_port = parsed_port * 10 + static_cast<uint32_t>(*cursor++ - '0');
            if (parsed_port > 65535 || ++digits > 5)
            {
                return false;
            }
        }
        if (digits == 0 || parsed_port == 0)
        {
            return false;
        }
        port = static_cast<uint16_t>(parsed_port);
    }

    int path_length = 0;
    if (*cursor == '?' )
    {
        path[path_length++] = '/';
        while (*cursor != '\0' && *cursor != '#')
        {
            if (*cursor == '\r' || *cursor == '\n' || path_length >= path_capacity - 1)
            {
                return false;
            }
            path[path_length++] = *cursor++;
        }
    }
    else if (*cursor == '\0' || *cursor == '#')
    {
        path[path_length++] = '/';
    }
    else if (*cursor == '/')
    {
        while (cursor[path_length] != '\0' && cursor[path_length] != '#')
        {
            if (cursor[path_length] == '\r' || cursor[path_length] == '\n' ||
                path_length >= path_capacity - 1)
            {
                return false;
            }
            path[path_length] = cursor[path_length];
            ++path_length;
        }
    }
    else
    {
        return false;
    }
    path[path_length] = '\0';
    return true;
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
    browser_document_length = index;
    browser_columns = 0;
    browser_scroll = 0;
}

static bool browser_append_document(char character, bool& pending_space)
{
    int length = browser_document_length;
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
                browser_document_length = length + 1;
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
        browser_document_length = length;
    }
    pending_space = false;
    if (length >= BROWSER_DOCUMENT_MAX - 1)
    {
        return false;
    }
    browser_document[length] = character;
    browser_document[length + 1] = '\0';
    browser_document_length = length + 1;
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
        while (entities[entity][offset] != '\0' && source[index + 1 + offset] == entities[entity][offset])
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
    browser_document_length = 0;
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
                while (source[index] != '\0' && !(source[index] == '-' && source[index + 1] == '-' && source[index + 2] == '>'))
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
                if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9')))
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
                if (browser_equal_ignore_case(tag, "br") || browser_equal_ignore_case(tag, "hr") || browser_tag_is_block(tag))
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

    int length = browser_document_length;
    while (length > 0 && (browser_document[length - 1] == ' ' || browser_document[length - 1] == '\n'))
    {
        browser_document[--length] = '\0';
    }
    browser_document_length = length;
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
        while (browser_document[index] != '\0' && browser_document[index] != ' ' && browser_document[index] != '\n' && word_length < BROWSER_LINE_MAX - 1)
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

static void browser_draw_text_clipped(const char* text, int x, int y, uint32_t color, int max_characters)
{
    char clipped[BROWSER_LINE_MAX] = {};
    if (max_characters >= BROWSER_LINE_MAX)
    {
        max_characters = BROWSER_LINE_MAX - 1;
    }
    if (max_characters <= 0)
    {
        return;
    }

    int index = 0;
    while (index < max_characters && text[index] != '\0')
    {
        clipped[index] = text[index];
        ++index;
    }
    clipped[index] = '\0';
    browser_gui->draw_text(clipped, x, y, color);
}

static bool browser_find_http_body(const char* response, uint32_t length, uint32_t& body_offset)
{
    for (uint32_t index = 0; index + 3 < length; ++index)
    {
        if (response[index] == '\r' && response[index + 1] == '\n' && response[index + 2] == '\r' && response[index + 3] == '\n')
        {
            body_offset = index + 4;
            return true;
        }
        if (response[index] == '\n' && response[index + 1] == '\n')
        {
            body_offset = index + 2;
            return true;
        }
    }
    return false;
}

static int browser_http_status_code(const char* response, uint32_t length)
{
    if (length < 12 || !browser_has_prefix_ignore_case(response, "HTTP/"))
    {
        return 0;
    }
    uint32_t index = 0;
    while (index < length && response[index] != ' ')
    {
        ++index;
    }
    while (index < length && response[index] == ' ')
    {
        ++index;
    }
    if (index + 2 >= length || response[index] < '0' || response[index] > '9' || response[index + 1] < '0' || response[index + 1] > '9' || response[index + 2] < '0' || response[index + 2] > '9')
    {
        return 0;
    }
    return (response[index] - '0') * 100 + (response[index + 1] - '0') * 10 + (response[index + 2] - '0');
}

static bool browser_http_is_chunked(const char* response, uint32_t body_offset)
{
    uint32_t line_start = 0;
    while (line_start < body_offset && response[line_start] != '\n')
    {
        ++line_start;
    }
    if (line_start < body_offset)
    {
        ++line_start;
    }
    while (line_start < body_offset)
    {
        uint32_t line_end = line_start;
        while (line_end < body_offset && response[line_end] != '\n')
        {
            ++line_end;
        }
        if (browser_has_prefix_ignore_case(response + line_start, "transfer-encoding:"))
        {
            for (uint32_t index = line_start; index + 7 <= line_end; ++index)
            {
                if (browser_has_prefix_ignore_case(response + index, "chunked"))
                {
                    return true;
                }
            }
        }
        line_start = line_end + 1;
    }
    return false;
}

static bool browser_decode_chunked(const char* input, uint32_t length, char* output, uint32_t capacity, uint32_t& output_length)
{
    uint32_t input_offset = 0;
    output_length = 0;
    while (input_offset < length)
    {
        uint32_t chunk_size = 0;
        uint32_t digits = 0;
        while (input_offset < length && input[input_offset] != '\r' && input[input_offset] != '\n')
        {
            const char character = input[input_offset++];
            if (character == ';')
            {
                while (input_offset < length && input[input_offset] != '\r' && input[input_offset] != '\n')
                {
                    ++input_offset;
                }
                break;
            }
            uint32_t digit = 0;
            if (character >= '0' && character <= '9')
            {
                digit = static_cast<uint32_t>(character - '0');
            }
            else if (character >= 'a' && character <= 'f')
            {
                digit = static_cast<uint32_t>(character - 'a' + 10);
            }
            else if (character >= 'A' && character <= 'F')
            {
                digit = static_cast<uint32_t>(character - 'A' + 10);
            }
            else
            {
                return false;
            }
            if (chunk_size > (capacity - digit) / 16)
            {
                return false;
            }
            chunk_size = chunk_size * 16 + digit;
            ++digits;
        }
        if (digits == 0 || input_offset >= length)
        {
            return false;
        }
        if (input[input_offset] == '\r')
        {
            ++input_offset;
            if (input_offset >= length || input[input_offset++] != '\n')
            {
                return false;
            }
        }
        else if (input[input_offset++] != '\n')
        {
            return false;
        }
        if (chunk_size == 0)
        {
            output[output_length] = '\0';
            return true;
        }
        if (chunk_size > length - input_offset || chunk_size >= capacity - output_length)
        {
            return false;
        }
        for (uint32_t index = 0; index < chunk_size; ++index)
        {
            output[output_length++] = input[input_offset++];
        }
        if (input_offset < length && input[input_offset] == '\r')
        {
            ++input_offset;
            if (input_offset >= length || input[input_offset++] != '\n')
            {
                return false;
            }
        }
        else if (input_offset < length && input[input_offset] == '\n')
        {
            ++input_offset;
        }
        else
        {
            return false;
        }
    }
    return false;
}

static void browser_set_status_code(const char* prefix, int status_code)
{
    int index = 0;
    while (prefix[index] != '\0' && index < static_cast<int>(sizeof(browser_status_text) - 8))
    {
        browser_status_text[index] = prefix[index];
        ++index;
    }
    browser_status_text[index++] = static_cast<char>('0' + (status_code / 100) % 10);
    browser_status_text[index++] = static_cast<char>('0' + (status_code / 10) % 10);
    browser_status_text[index++] = static_cast<char>('0' + status_code % 10);
    browser_status_text[index] = '\0';
    browser_status = browser_status_text;
}

static bool browser_load_page()
{
    if (browser_address[0] == '\0')
    {
        browser_status = "Enter a URL or local HTML file path";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    if (browser_has_prefix_ignore_case(browser_address, "https://"))
    {
        browser_status = "HTTPS/TLS is not supported yet; use plain HTTP";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    char hostname[128] = {};
    char path[256] = {};
    uint16_t port = 80;
    const bool is_http_url = browser_has_prefix_ignore_case(browser_address, "http://");
    if (is_http_url)
    {
        if (browser_api == nullptr || browser_api->http_get == nullptr)
        {
            browser_status = "HTTP network service is unavailable";
            browser_status_color = 0xFFFF7777;
            return false;
        }
        if (!browser_parse_http_url(browser_address, hostname, sizeof(hostname),
                                    port, path, sizeof(path)))
        {
            browser_status = "Invalid HTTP URL";
            browser_status_color = 0xFFFF7777;
            return false;
        }

        const int32_t response_length = browser_api->http_get(
            hostname, port, path, browser_response, sizeof(browser_response), 15000);
        if (response_length < 0)
        {
            switch (response_length)
            {
                case NAPP_HTTP_ERROR_NETWORK:
                    browser_status = "Network is offline or has no DHCP address";
                    break;
                case NAPP_HTTP_ERROR_DNS:
                    browser_status = "Could not resolve the host name";
                    break;
                case NAPP_HTTP_ERROR_CONNECT:
                    browser_status = "Could not connect to the HTTP server";
                    break;
                case NAPP_HTTP_ERROR_TIMEOUT:
                    browser_status = "HTTP request timed out";
                    break;
                case NAPP_HTTP_ERROR_TOO_LARGE:
                    browser_status = "HTTP response exceeds the 8 KB limit";
                    break;
                default:
                    browser_status = "Invalid HTTP request";
                    break;
            }
            browser_status_color = 0xFFFF7777;
            return false;
        }

        uint32_t body_offset = 0;
        if (!browser_find_http_body(browser_response, static_cast<uint32_t>(response_length),
                                    body_offset))
        {
            browser_status = "Invalid HTTP response headers";
            browser_status_color = 0xFFFF7777;
            return false;
        }
        const int status_code = browser_http_status_code(
            browser_response, static_cast<uint32_t>(response_length));
        if (status_code == 0)
        {
            browser_status = "Invalid HTTP status line";
            browser_status_color = 0xFFFF7777;
            return false;
        }
        if (status_code >= 300 && status_code < 400)
        {
            browser_set_status_code("Redirect not followed: ", status_code);
            browser_status_color = 0xFFFFCC66;
            return false;
        }

        const uint32_t body_length = static_cast<uint32_t>(response_length) - body_offset;
        uint32_t page_source_length = 0;
        if (browser_http_is_chunked(browser_response, body_offset))
        {
            if (!browser_decode_chunked(browser_response + body_offset, body_length, browser_page_source, sizeof(browser_page_source), page_source_length))
            {
                browser_status = "Invalid chunked HTTP response";
                browser_status_color = 0xFFFF7777;
                return false;
            }
        }
        else
        {
            page_source_length = body_length;
            if (page_source_length >= sizeof(browser_page_source))
            {
                page_source_length = sizeof(browser_page_source) - 1;
                browser_source_truncated = true;
            }
            for (uint32_t index = 0; index < page_source_length; ++index)
            {
                browser_page_source[index] = browser_response[body_offset + index];
            }
            browser_page_source[page_source_length] = '\0';
        }
        browser_parse_html(browser_page_source);
        if (status_code >= 400)
        {
            browser_set_status_code("HTTP error: ", status_code);
            browser_status_color = 0xFFFFCC66;
        }
        else
        {
            browser_status = browser_truncated ? "Page loaded (content truncated)" : "Page loaded";
            browser_status_color = browser_truncated ? 0xFFFFCC66 : BROWSER_ACCENT;
        }
        browser_address_focused = false;
        return true;
    }

    if (browser_api == nullptr || browser_api->clawfs_resolve_path == nullptr || browser_api->clawfs_read_sector == nullptr)
    {
        browser_status = "Local filesystem access is unavailable";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    const char* current_path = browser_api->current_path != nullptr ? browser_api->current_path : "/";
    const uint32_t sector = browser_api->clawfs_resolve_path(current_path, browser_address);
    if (sector == 0)
    {
        browser_status = "Local file not found";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    char source[512] = {};
    if (!browser_api->clawfs_read_sector(sector, source))
    {
        browser_status = "Could not read local file";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    source[sizeof(source) - 1] = '\0';
    browser_source_truncated = false;
    browser_parse_html(source);
    browser_status = browser_truncated ? "Page opened (content truncated)" : "Page opened";
    browser_status_color = browser_truncated ? 0xFFFFCC66 : BROWSER_ACCENT;
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
    const int columns = (window->width - 20) / 9;
    const int address_width = window->width - 88;
    const int address_characters = (address_width - 12) / 9;
    const int status_characters = (window->width - 20) / 9;

    browser_gui->fill_block(window->pos_x, content_top, BROWSER_BACKGROUND, window->width, window->height - window->title_height);
    browser_gui->fill_block(left, toolbar_y, browser_address_focused ? BROWSER_INPUT : BROWSER_SURFACE, address_width, 24);
    browser_draw_text_clipped(browser_address[0] != '\0' ? browser_address : "http:// URL or local file path", left + 6, toolbar_y + 7, BROWSER_TEXT, address_characters);
    browser_gui->fill_block(window->pos_x + window->width - 70, toolbar_y, BROWSER_ACCENT, 60, 24);
    browser_gui->draw_text("Open", window->pos_x + window->width - 57, toolbar_y + 7, BROWSER_BACKGROUND);
    browser_draw_text_clipped(browser_status, left, content_top + 38, browser_status_color, status_characters);

    if (columns < 12)
    {
        browser_draw_text_clipped("Window is too narrow", left, body_y, BROWSER_TEXT, columns);
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
        browser_draw_text_clipped(browser_lines[line], left, body_y + rows_drawn * 12, BROWSER_TEXT, columns);
        ++rows_drawn;
    }
    if (browser_truncated && rows_drawn < visible_rows)
    {
        browser_draw_text_clipped("[Document truncated]", left, body_y + rows_drawn * 12, 0xFFAA5500, columns);
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
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr || api->gui->open_window == nullptr || api->gui->draw_text == nullptr || api->gui->fill_block == nullptr)
    {
        return 1;
    }

    browser_api = api;
    browser_gui = api->gui;
    browser_address[0] = '\0';
    browser_response[0] = '\0';
    browser_page_source[0] = '\0';
    browser_source_truncated = false;
    browser_truncated = false;
    browser_set_document(
        "Welcome to Nasua Browser\n"
        "\n"
        "Renders basic HTML from local ClawFS files or plain HTTP pages.\n"
        "Enter an http:// URL or a local path above, then press Enter.\n"
        "\n"
        "Supported: headings, paragraphs, lists, line breaks and common text entities.\n"
        "Use j/k to scroll, Page Up/Down to move a page, or e to edit the path.\n"
        "\n"
        "HTTPS/TLS is not supported yet.");

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
