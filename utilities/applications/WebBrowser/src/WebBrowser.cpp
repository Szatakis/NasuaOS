#include <napp.h>

NAPP_APPLICATION("WebBrowser", "HTML and CSS web browser", true);

#define BROWSER_ADDRESS_MAX 256
#define BROWSER_RESPONSE_CAPACITY (1024 * 1024 + 1)
#define BROWSER_DOCUMENT_CAPACITY (96 * 1024)
#define BROWSER_MAX_LINES 768
#define BROWSER_LINE_MAX 120
#define BROWSER_MAX_CSS_RULES 256
#define BROWSER_MAX_STYLESHEETS 8
#define BROWSER_MAX_STYLE_DEPTH 20
#define BROWSER_MAX_LINKS 128
#define BROWSER_TAG_MAX 512
#define BROWSER_MAX_CUSTOM_PROPERTIES 24
#define BROWSER_MAX_BOXES 512
#define BROWSER_MAX_DOM_NODES 2048

static constexpr uint32_t BROWSER_BACKGROUND = 0xFF11151D;
static constexpr uint32_t BROWSER_SURFACE = 0xFF1B222D;
static constexpr uint32_t BROWSER_INPUT = 0xFF283240;
static constexpr uint32_t BROWSER_TEXT = 0xFFE1E7EF;
static constexpr uint32_t BROWSER_MUTED = 0xFF9BA8B8;
static constexpr uint32_t BROWSER_ACCENT = 0xFF59E81B;
static constexpr uint32_t BROWSER_NO_BACKGROUND = 0;

enum browser_style_flags
{
    BROWSER_BOLD = 1,
    BROWSER_UNDERLINE = 2,
    BROWSER_STRIKETHROUGH = 4,
    BROWSER_OVERLINE = 8,
    BROWSER_UPPERCASE = 16,
    BROWSER_LOWERCASE = 32,
    BROWSER_CAPITALIZE = 64,
    BROWSER_PREFORMATTED = 128
};

enum browser_alignment
{
    BROWSER_ALIGN_LEFT,
    BROWSER_ALIGN_CENTER,
    BROWSER_ALIGN_RIGHT
};

enum browser_layout_mode
{
    BROWSER_LAYOUT_NORMAL,
    BROWSER_LAYOUT_FLEX,
    BROWSER_LAYOUT_GRID
};

struct browser_style
{
    uint32_t color;
    uint32_t background;
    uint32_t border_color;
    uint8_t flags;
    uint8_t alignment;
    uint16_t color_specificity;
    uint16_t background_specificity;
    uint16_t border_specificity;
    uint16_t space_before_specificity;
    uint16_t space_after_specificity;
    uint16_t min_height_specificity;
    uint16_t font_weight_specificity;
    uint16_t text_decoration_specificity;
    uint16_t text_transform_specificity;
    uint16_t alignment_specificity;
    uint16_t align_items_specificity;
    uint16_t hidden_specificity;
    uint16_t white_space_specificity;
    uint16_t width_specificity;
    uint16_t max_width_specificity;
    uint16_t padding_left_specificity;
    uint16_t padding_right_specificity;
    uint16_t margin_left_specificity;
    uint16_t margin_right_specificity;
    uint16_t display_specificity;
    uint16_t grid_columns_specificity;
    uint16_t grid_gap_specificity;
    uint8_t link_id;
    uint8_t space_before;
    uint8_t space_after;
    uint8_t min_height_lines;
    uint16_t width_pixels;
    uint16_t width_cap_pixels;
    uint16_t max_width_pixels;
    uint8_t width_percent;
    uint8_t padding_left_chars;
    uint8_t padding_right_chars;
    uint8_t layout_mode;
    uint8_t grid_columns;
    uint8_t grid_gap_chars;
    bool hidden;
    bool preformatted;
    bool border_enabled;
    bool width_set;
    bool margin_left_auto;
    bool margin_right_auto;
    bool align_items_center;
};

struct browser_css_rule
{
    char selector[80];
    uint32_t color;
    uint32_t background;
    uint32_t border_color;
    uint8_t flags;
    uint8_t alignment;
    uint32_t declarations;
    uint8_t space_before;
    uint8_t space_after;
    uint8_t min_height_lines;
    uint16_t width_pixels;
    uint16_t width_cap_pixels;
    uint16_t max_width_pixels;
    uint8_t width_percent;
    uint8_t padding_left_chars;
    uint8_t padding_right_chars;
    uint8_t layout_mode;
    uint8_t grid_columns;
    uint8_t grid_gap_chars;
    bool align_items_center;
    bool hidden;
    bool preformatted;
    bool border_enabled;
    bool width_set;
    bool margin_left_auto;
    bool margin_right_auto;
};

struct browser_custom_property
{
    char name[32];
    char value[64];
};

struct browser_element_context
{
    char tag[32];
    char class_name[64];
    char id[64];
    int box_index;
    int dom_node;
    bool centered_min_height;
};

enum browser_dom_node_kind
{
    BROWSER_DOM_DOCUMENT,
    BROWSER_DOM_ELEMENT,
    BROWSER_DOM_TEXT
};

struct browser_dom_node
{
    char tag[32];
    char class_name[64];
    char id[64];
    int parent;
    int first_child;
    int last_child;
    int next_sibling;
    uint32_t source_start;
    uint32_t source_end;
    uint32_t document_start;
    uint32_t document_end;
    uint8_t kind;
    uint8_t layout_mode;
    uint8_t grid_columns;
    uint8_t grid_gap_chars;
    uint8_t grid_row_gap_lines;
    bool hidden;
};

struct browser_box
{
    int start_offset;
    int end_offset;
    int first_line;
    int last_line;
    uint32_t background;
    uint32_t border;
    int dom_node;
    uint16_t width_pixels;
    uint16_t width_cap_pixels;
    uint16_t max_width_pixels;
    uint8_t width_percent;
    uint8_t padding_left_chars;
    uint8_t padding_right_chars;
    uint8_t layout_mode;
    uint8_t grid_columns;
    uint8_t grid_gap_chars;
    bool width_set;
    bool margin_left_auto;
    bool margin_right_auto;
};

struct browser_link
{
    char href[256];
};

struct browser_fragment
{
    char id[64];
    int document_offset;
};

struct browser_media_breakpoint
{
    uint16_t width;
    bool maximum;
};

static const napp_api* browser_api = nullptr;
static const napp_gui* browser_gui = nullptr;

static char browser_address[BROWSER_ADDRESS_MAX];
static char* browser_response = nullptr;
static char* browser_page_source = nullptr;
static char* browser_document = nullptr;
static char browser_lines[BROWSER_MAX_LINES][BROWSER_LINE_MAX];
static uint32_t* browser_document_colors = nullptr;
static uint32_t* browser_document_backgrounds = nullptr;
static uint32_t* browser_document_borders = nullptr;
static uint8_t* browser_document_flags = nullptr;
static uint8_t* browser_document_alignment = nullptr;
static uint8_t* browser_document_links = nullptr;
static uint32_t browser_line_offsets[BROWSER_MAX_LINES][BROWSER_LINE_MAX];
static uint32_t browser_line_document_starts[BROWSER_MAX_LINES];
static uint32_t browser_line_document_ends[BROWSER_MAX_LINES];
static int browser_line_visual_rows[BROWSER_MAX_LINES];
static bool browser_line_grid_assigned[BROWSER_MAX_LINES];
static int browser_grid_item_first_lines[BROWSER_MAX_DOM_NODES];
static int browser_grid_item_last_lines[BROWSER_MAX_DOM_NODES];
static uint8_t browser_line_alignment[BROWSER_MAX_LINES];
static browser_css_rule browser_css_rules[BROWSER_MAX_CSS_RULES];
static browser_custom_property browser_custom_properties[BROWSER_MAX_CUSTOM_PROPERTIES];
static browser_link browser_links[BROWSER_MAX_LINKS];
static browser_fragment browser_fragments[BROWSER_MAX_LINKS];
static browser_media_breakpoint browser_media_breakpoints[32];
static browser_box browser_boxes[BROWSER_MAX_BOXES];
static browser_dom_node* browser_dom_nodes = nullptr;
static int browser_css_rule_count = 0;
static int browser_stylesheet_count = 0;
static int browser_media_breakpoint_count = 0;
static int browser_custom_property_count = 0;
static int browser_link_count = 0;
static int browser_fragment_count = 0;
static int browser_box_count = 0;
static int browser_dom_node_count = 0;
static int browser_document_length = 0;
static int browser_line_count = 0;
static int browser_visual_line_count = 0;
static int browser_scroll = 0;
static int browser_columns = 0;
static int browser_content_height = 12;
static int browser_viewport_width = 800;
static int browser_viewport_height = 548;
static bool browser_address_focused = true;
static bool browser_truncated = false;
static bool browser_source_truncated = false;
static bool browser_stylesheet_failed = false;
static bool browser_box_truncated = false;
static bool browser_dom_truncated = false;
static char browser_status_text[64];
static const char* browser_status = "Enter an http:// URL or a local ClawFS file path";
static uint32_t browser_status_color = BROWSER_MUTED;
static uint32_t browser_page_background = BROWSER_BACKGROUND;

static void browser_release_page_buffers()
{
    if (browser_api == nullptr || browser_api->deallocate == nullptr)
    {
        return;
    }
    if (browser_response != nullptr) browser_api->deallocate(browser_response);
    if (browser_page_source != nullptr) browser_api->deallocate(browser_page_source);
    if (browser_document != nullptr) browser_api->deallocate(browser_document);
    if (browser_document_colors != nullptr) browser_api->deallocate(browser_document_colors);
    if (browser_document_backgrounds != nullptr) browser_api->deallocate(browser_document_backgrounds);
    if (browser_document_borders != nullptr) browser_api->deallocate(browser_document_borders);
    if (browser_document_flags != nullptr) browser_api->deallocate(browser_document_flags);
    if (browser_document_alignment != nullptr) browser_api->deallocate(browser_document_alignment);
    if (browser_document_links != nullptr) browser_api->deallocate(browser_document_links);
    if (browser_dom_nodes != nullptr) browser_api->deallocate(browser_dom_nodes);
    browser_response = nullptr;
    browser_page_source = nullptr;
    browser_document = nullptr;
    browser_document_colors = nullptr;
    browser_document_backgrounds = nullptr;
    browser_document_borders = nullptr;
    browser_document_flags = nullptr;
    browser_document_alignment = nullptr;
    browser_document_links = nullptr;
    browser_dom_nodes = nullptr;
}

static bool browser_allocate_page_buffers()
{
    if (browser_response != nullptr && browser_page_source != nullptr &&
        browser_document != nullptr && browser_document_colors != nullptr &&
        browser_document_backgrounds != nullptr && browser_document_borders != nullptr &&
        browser_document_flags != nullptr &&
        browser_document_alignment != nullptr && browser_document_links != nullptr &&
        browser_dom_nodes != nullptr)
    {
        return true;
    }
    if (browser_api == nullptr || browser_api->allocate == nullptr ||
        browser_api->deallocate == nullptr)
    {
        return false;
    }

    browser_response = static_cast<char*>(browser_api->allocate(BROWSER_RESPONSE_CAPACITY));
    browser_page_source = static_cast<char*>(browser_api->allocate(BROWSER_RESPONSE_CAPACITY));
    browser_document = static_cast<char*>(browser_api->allocate(BROWSER_DOCUMENT_CAPACITY));
    browser_document_colors = static_cast<uint32_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY * sizeof(uint32_t)));
    browser_document_backgrounds = static_cast<uint32_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY * sizeof(uint32_t)));
    browser_document_borders = static_cast<uint32_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY * sizeof(uint32_t)));
    browser_document_flags = static_cast<uint8_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY));
    browser_document_alignment = static_cast<uint8_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY));
    browser_document_links = static_cast<uint8_t*>(
        browser_api->allocate(BROWSER_DOCUMENT_CAPACITY));
    browser_dom_nodes = static_cast<browser_dom_node*>(browser_api->allocate(
        BROWSER_MAX_DOM_NODES * sizeof(browser_dom_node)));

    if (browser_response == nullptr || browser_page_source == nullptr ||
        browser_document == nullptr || browser_document_colors == nullptr ||
        browser_document_backgrounds == nullptr || browser_document_borders == nullptr ||
        browser_document_flags == nullptr ||
        browser_document_alignment == nullptr || browser_document_links == nullptr ||
        browser_dom_nodes == nullptr)
    {
        browser_release_page_buffers();
        return false;
    }
    return true;
}

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

static bool browser_parse_url(const char* url, bool secure, char* hostname,
                              int hostname_capacity, uint16_t& port,
                              char* path, int path_capacity)
{
    const char* cursor = url;
    if (secure && browser_has_prefix_ignore_case(cursor, "https://"))
    {
        cursor += 8;
    }
    else if (!secure && browser_has_prefix_ignore_case(cursor, "http://"))
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

    port = secure ? 443 : 80;
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
    while (text[index] != '\0' && index < BROWSER_DOCUMENT_CAPACITY - 1)
    {
        browser_document[index] = text[index];
        browser_document_colors[index] = BROWSER_TEXT;
        browser_document_backgrounds[index] = BROWSER_NO_BACKGROUND;
        browser_document_borders[index] = BROWSER_NO_BACKGROUND;
        browser_document_flags[index] = 0;
        browser_document_alignment[index] = BROWSER_ALIGN_LEFT;
        browser_document_links[index] = 0;
        ++index;
    }
    browser_document[index] = '\0';
    browser_document_length = index;
    browser_columns = 0;
    browser_scroll = 0;
}

static bool browser_append_document(char character, bool& pending_space, const browser_style& style)
{
    int length = browser_document_length;
    if (character == '\n')
    {
        while (!style.preformatted && length > 0 && browser_document[length - 1] == ' ')
        {
            browser_document[--length] = '\0';
        }
        if (length > 0 &&
            (style.preformatted || browser_document[length - 1] != '\n'))
        {
            if (length < BROWSER_DOCUMENT_CAPACITY - 1)
            {
                browser_document[length] = '\n';
                browser_document_colors[length] = style.color;
                browser_document_backgrounds[length] = style.background;
                browser_document_borders[length] = style.border_enabled
                    ? style.border_color : BROWSER_NO_BACKGROUND;
                browser_document_flags[length] = style.flags |
                    (style.preformatted ? BROWSER_PREFORMATTED : 0);
                browser_document_alignment[length] = style.alignment;
                browser_document_links[length] = style.link_id;
                browser_document[length + 1] = '\0';
                browser_document_length = length + 1;
            }
        }
        pending_space = false;
        return length < BROWSER_DOCUMENT_CAPACITY - 1;
    }

    if (character == '\r' && style.preformatted)
    {
        return true;
    }
    if ((character == ' ' || character == '\t') && style.preformatted)
    {
        if (length >= BROWSER_DOCUMENT_CAPACITY - 1) return false;
        browser_document[length] = character == '\t' ? ' ' : character;
        browser_document_colors[length] = style.color;
        browser_document_backgrounds[length] = style.background;
        browser_document_borders[length] = style.border_enabled
            ? style.border_color : BROWSER_NO_BACKGROUND;
        browser_document_flags[length] = style.flags |
            (style.preformatted ? BROWSER_PREFORMATTED : 0);
        browser_document_alignment[length] = style.alignment;
        browser_document_links[length] = style.link_id;
        browser_document[length + 1] = '\0';
        browser_document_length = length + 1;
        pending_space = false;
        return true;
    }
    if (character == ' ' || character == '\t' || character == '\r')
    {
        pending_space = true;
        return true;
    }

    if (pending_space && length > 0 && browser_document[length - 1] != '\n')
    {
        if (length >= BROWSER_DOCUMENT_CAPACITY - 1)
        {
            return false;
        }
        browser_document[length++] = ' ';
        browser_document_colors[length - 1] = style.color;
        browser_document_backgrounds[length - 1] = style.background;
        browser_document_borders[length - 1] = style.border_enabled
            ? style.border_color : BROWSER_NO_BACKGROUND;
        browser_document_flags[length - 1] = style.flags;
        browser_document_alignment[length - 1] = style.alignment;
        browser_document_links[length - 1] = style.link_id;
        browser_document_length = length;
    }
    pending_space = false;
    if (length >= BROWSER_DOCUMENT_CAPACITY - 1)
    {
        return false;
    }
    const bool word_start = length == 0 || pending_space ||
        browser_document[length - 1] == '\n' || browser_document[length - 1] == ' ';
    if ((style.flags & BROWSER_UPPERCASE) != 0 ||
        ((style.flags & BROWSER_CAPITALIZE) != 0 && word_start))
    {
        if (character >= 'a' && character <= 'z')
            character = static_cast<char>(character - 'a' + 'A');
    }
    else if ((style.flags & BROWSER_LOWERCASE) != 0 &&
             character >= 'A' && character <= 'Z')
    {
        character = static_cast<char>(character - 'A' + 'a');
    }
    browser_document[length] = character;
    browser_document_colors[length] = style.color;
    browser_document_backgrounds[length] = style.background;
    browser_document_borders[length] = style.border_enabled
        ? style.border_color : BROWSER_NO_BACKGROUND;
    browser_document_flags[length] = style.flags |
        (style.preformatted ? BROWSER_PREFORMATTED : 0);
    browser_document_alignment[length] = style.alignment;
    browser_document_links[length] = style.link_id;
    browser_document[length + 1] = '\0';
    browser_document_length = length + 1;
    return true;
}

static bool browser_append_blank_lines(uint8_t count, const browser_style& style)
{
    while (count-- > 0)
    {
        const int length = browser_document_length;
        if (length >= BROWSER_DOCUMENT_CAPACITY - 1) return false;
        browser_document[length] = '\n';
        browser_document[length + 1] = '\0';
        browser_document_colors[length] = style.color;
        browser_document_backgrounds[length] = style.background;
        browser_document_borders[length] = style.border_enabled
            ? style.border_color : BROWSER_NO_BACKGROUND;
        browser_document_flags[length] = style.flags;
        browser_document_alignment[length] = style.alignment;
        browser_document_links[length] = style.link_id;
        browser_document_length = length + 1;
    }
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
        || browser_equal_ignore_case(tag, "header")
        || browser_equal_ignore_case(tag, "footer")
        || browser_equal_ignore_case(tag, "main")
        || browser_equal_ignore_case(tag, "nav")
        || browser_equal_ignore_case(tag, "form")
        || browser_equal_ignore_case(tag, "figure")
        || browser_equal_ignore_case(tag, "figcaption")
        || browser_equal_ignore_case(tag, "table")
        || browser_equal_ignore_case(tag, "thead")
        || browser_equal_ignore_case(tag, "tbody")
        || browser_equal_ignore_case(tag, "tfoot")
        || browser_equal_ignore_case(tag, "caption")
        || browser_equal_ignore_case(tag, "dl")
        || browser_equal_ignore_case(tag, "dt")
        || browser_equal_ignore_case(tag, "dd")
        || browser_equal_ignore_case(tag, "blockquote")
        || browser_equal_ignore_case(tag, "pre")
        || browser_equal_ignore_case(tag, "address")
        || browser_equal_ignore_case(tag, "details")
        || browser_equal_ignore_case(tag, "summary")
        || browser_equal_ignore_case(tag, "fieldset")
        || browser_equal_ignore_case(tag, "legend")
        || browser_equal_ignore_case(tag, "hgroup");
}

static int browser_create_box(int start_offset, const browser_style& style)
{
    if (style.background_specificity == 0 && !style.border_enabled &&
        !style.width_set && style.max_width_pixels == 0 &&
        style.padding_left_chars == 0 && style.padding_right_chars == 0 &&
        style.layout_mode == BROWSER_LAYOUT_NORMAL &&
        !(style.margin_left_auto && style.margin_right_auto))
    {
        return -1;
    }
    if (browser_box_count >= BROWSER_MAX_BOXES)
    {
        browser_box_truncated = true;
        return -1;
    }

    const int index = browser_box_count++;
    browser_box& box = browser_boxes[index];
    box.start_offset = start_offset;
    box.end_offset = -1;
    box.first_line = -1;
    box.last_line = -1;
    box.background = style.background_specificity > 0
        ? style.background : BROWSER_NO_BACKGROUND;
    box.border = style.border_enabled ? style.border_color : BROWSER_NO_BACKGROUND;
    box.dom_node = -1;
    box.width_pixels = style.width_pixels;
    box.width_cap_pixels = style.width_cap_pixels;
    box.max_width_pixels = style.max_width_pixels;
    box.width_percent = style.width_percent;
    box.padding_left_chars = style.padding_left_chars;
    box.padding_right_chars = style.padding_right_chars;
    box.layout_mode = style.layout_mode;
    box.grid_columns = style.grid_columns;
    box.grid_gap_chars = style.grid_gap_chars;
    box.width_set = style.width_set;
    box.margin_left_auto = style.margin_left_auto;
    box.margin_right_auto = style.margin_right_auto;
    return index;
}

static void browser_close_box(int box_index, int end_offset)
{
    if (box_index < 0 || box_index >= browser_box_count) return;
    browser_boxes[box_index].end_offset = end_offset;
}

static bool browser_box_contains_offset(const browser_box& box, int offset)
{
    return box.end_offset > box.start_offset &&
           offset >= box.start_offset && offset < box.end_offset;
}

static void browser_box_outer_geometry(const browser_box& box, int parent_x,
                                       int parent_width, int& box_x,
                                       int& box_width)
{
    box_width = parent_width;
    if (box.width_set)
    {
        box_width = box.width_percent > 0
            ? parent_width * box.width_percent / 100
            : box.width_pixels;
        if (box.width_cap_pixels > 0 &&
            box_width > box.width_cap_pixels)
            box_width = box.width_cap_pixels;
    }
    if (box.max_width_pixels > 0 &&
        box_width > box.max_width_pixels)
        box_width = box.max_width_pixels;
    if (box_width > parent_width) box_width = parent_width;
    if (box_width < 9) box_width = 9;
    box_x = parent_x;
    if (box.margin_left_auto && box.margin_right_auto &&
        box_width < parent_width)
        box_x += (parent_width - box_width) / 2;
}

static void browser_get_content_geometry_linear(int document_offset,
                                                int first_box, int box_limit,
                                                int& content_x,
                                                int& content_width)
{
    for (int index = first_box; index < box_limit; ++index)
    {
        const browser_box& box = browser_boxes[index];
        if (!browser_box_contains_offset(box, document_offset)) continue;

        int outer_x = content_x;
        int outer_width = content_width;
        browser_box_outer_geometry(box, content_x, content_width,
                                   outer_x, outer_width);
        content_x = outer_x + box.padding_left_chars * 9;
        content_width = outer_width -
            (box.padding_left_chars + box.padding_right_chars) * 9;
        if (content_width < 9) content_width = 9;
    }
}

static bool browser_find_grid_item(int document_offset, int& grid_node_index,
                                   int& item_column)
{
    for (int node_index = 1; node_index < browser_dom_node_count; ++node_index)
    {
        const browser_dom_node& grid = browser_dom_nodes[node_index];
        if (grid.kind != BROWSER_DOM_ELEMENT ||
            grid.layout_mode != BROWSER_LAYOUT_GRID ||
            grid.hidden ||
            grid.grid_columns < 2 ||
            document_offset < static_cast<int>(grid.document_start) ||
            document_offset >= static_cast<int>(grid.document_end))
            continue;

        bool nested_grid = false;
        for (int ancestor = grid.parent; ancestor > 0;
             ancestor = browser_dom_nodes[ancestor].parent)
        {
            if (browser_dom_nodes[ancestor].layout_mode == BROWSER_LAYOUT_GRID)
            {
                nested_grid = true;
                break;
            }
        }
        if (nested_grid) continue;

        int column = 0;
        for (int child = grid.first_child; child >= 0;
             child = browser_dom_nodes[child].next_sibling)
        {
            const browser_dom_node& item = browser_dom_nodes[child];
            if (item.kind != BROWSER_DOM_ELEMENT) continue;
            if (item.hidden) continue;
            if (document_offset >= static_cast<int>(item.document_start) &&
                document_offset < static_cast<int>(item.document_end))
            {
                grid_node_index = node_index;
                item_column = column;
                return true;
            }
            ++column;
        }
    }
    return false;
}

static void browser_get_content_geometry(int document_offset, int box_limit,
                                        int viewport_width, int& content_x,
                                        int& content_width)
{
    int grid_node_index = -1;
    int item_column = -1;
    int grid_box_index = -1;
    if (browser_find_grid_item(document_offset, grid_node_index, item_column))
    {
        for (int index = 0; index < browser_box_count; ++index)
        {
            if (browser_boxes[index].dom_node == grid_node_index)
            {
                grid_box_index = index;
                break;
            }
        }
    }
    if (grid_box_index < 0 || grid_box_index >= box_limit)
    {
        content_x = 0;
        content_width = viewport_width;
        browser_get_content_geometry_linear(document_offset, 0, box_limit,
                                            content_x,
                                            content_width);
        return;
    }

    content_x = 0;
    content_width = viewport_width;
    browser_get_content_geometry_linear(document_offset, 0,
                                        grid_box_index + 1,
                                        content_x, content_width);
    const browser_dom_node& grid = browser_dom_nodes[grid_node_index];
    const int column_count = grid.grid_columns;
    const int gap_pixels = grid.grid_gap_chars * 9;
    int available_width = content_width -
        gap_pixels * (column_count - 1);
    if (available_width < column_count * 9)
        available_width = column_count * 9;
    const int cell_width = available_width / column_count;
    content_x += (item_column % column_count) *
                 (cell_width + gap_pixels);
    content_width = cell_width;
    browser_get_content_geometry_linear(document_offset, grid_box_index + 1,
                                        box_limit,
                                        content_x, content_width);
}

static void browser_get_box_geometry(int box_index, int viewport_width,
                                     int& box_x, int& box_width)
{
    if (box_index < 0 || box_index >= browser_box_count)
    {
        box_x = 0;
        box_width = viewport_width;
        return;
    }

    int parent_x = 0;
    int parent_width = viewport_width;
    browser_get_content_geometry(browser_boxes[box_index].start_offset,
                                 box_index, viewport_width, parent_x,
                                 parent_width);
    browser_box_outer_geometry(browser_boxes[box_index], parent_x,
                               parent_width, box_x, box_width);
}

static int browser_create_dom_node(int parent, uint8_t kind, const char* tag,
                                  const char* class_name, const char* id,
                                  int source_start, int source_end)
{
    if (browser_dom_node_count >= BROWSER_MAX_DOM_NODES)
    {
        browser_dom_truncated = true;
        return -1;
    }

    if (parent < 0 || parent >= browser_dom_node_count) parent = 0;
    const int index = browser_dom_node_count++;
    browser_dom_node& node = browser_dom_nodes[index];
    node = {};
    node.kind = kind;
    node.parent = parent;
    node.first_child = -1;
    node.last_child = -1;
    node.next_sibling = -1;
    node.source_start = static_cast<uint32_t>(source_start);
    node.source_end = static_cast<uint32_t>(source_end);
    for (int character = 0; tag != nullptr && tag[character] != '\0' &&
         character < static_cast<int>(sizeof(node.tag) - 1); ++character)
        node.tag[character] = tag[character];
    for (int character = 0; class_name != nullptr &&
         class_name[character] != '\0' &&
         character < static_cast<int>(sizeof(node.class_name) - 1); ++character)
        node.class_name[character] = class_name[character];
    for (int character = 0; id != nullptr && id[character] != '\0' &&
         character < static_cast<int>(sizeof(node.id) - 1); ++character)
        node.id[character] = id[character];

    browser_dom_node& parent_node = browser_dom_nodes[parent];
    if (parent_node.first_child < 0)
        parent_node.first_child = index;
    else
        browser_dom_nodes[parent_node.last_child].next_sibling = index;
    parent_node.last_child = index;
    return index;
}

static void browser_append_dom_text(int parent, int source_start, int source_end)
{
    if (parent < 0 || parent >= browser_dom_node_count) parent = 0;
    browser_dom_node& parent_node = browser_dom_nodes[parent];
    if (parent_node.last_child >= 0)
    {
        browser_dom_node& last_child =
            browser_dom_nodes[parent_node.last_child];
        if (last_child.kind == BROWSER_DOM_TEXT &&
            last_child.source_end == static_cast<uint32_t>(source_start))
        {
            last_child.source_end = static_cast<uint32_t>(source_end);
            return;
        }
    }
    browser_create_dom_node(parent, BROWSER_DOM_TEXT, nullptr, nullptr, nullptr,
                           source_start, source_end);
}

static void browser_close_dom_node(int node_index, int source_end,
                                   int document_end)
{
    if (node_index < 0 || node_index >= browser_dom_node_count) return;
    browser_dom_nodes[node_index].source_end =
        static_cast<uint32_t>(source_end);
    browser_dom_nodes[node_index].document_end =
        static_cast<uint32_t>(document_end);
}

static char browser_unicode_to_ascii(uint32_t codepoint)
{
    if (codepoint >= 0x20 && codepoint <= 0x7E)
        return static_cast<char>(codepoint);
    if (codepoint == 0xA0 || codepoint == 0x2002 ||
        codepoint == 0x2003 || codepoint == 0x2009)
        return ' ';
    if (codepoint == 0xC4 || codepoint == 0xC6)
        return 'A';
    if (codepoint == 0xE4 || codepoint == 0xE6)
        return 'a';
    if (codepoint == 0x0104 || codepoint == 0x0106 ||
        codepoint == 0x0118 || codepoint == 0x0143 ||
        codepoint == 0x015A || codepoint == 0x0179 ||
        codepoint == 0x017B || codepoint == 0x017D)
        return codepoint == 0x0104 ? 'A' :
               codepoint == 0x0106 ? 'C' :
               codepoint == 0x0118 ? 'E' :
               codepoint == 0x0143 ? 'N' :
               codepoint == 0x015A ? 'S' : 'Z';
    if (codepoint == 0x0105 || codepoint == 0x0107 ||
        codepoint == 0x0119 || codepoint == 0x0144 ||
        codepoint == 0x015B || codepoint == 0x017A ||
        codepoint == 0x017C || codepoint == 0x017E)
        return codepoint == 0x0105 ? 'a' :
               codepoint == 0x0107 ? 'c' :
               codepoint == 0x0119 ? 'e' :
               codepoint == 0x0144 ? 'n' :
               codepoint == 0x015B ? 's' : 'z';
    if (codepoint == 0xC7) return 'C';
    if (codepoint == 0xE7) return 'c';
    if (codepoint == 0xD0 || codepoint == 0xD2 || codepoint == 0xD3 ||
        codepoint == 0xD5 || codepoint == 0xD6 || codepoint == 0xD8)
        return 'O';
    if (codepoint == 0xF0 || codepoint == 0xF2 || codepoint == 0xF3 ||
        codepoint == 0xF5 || codepoint == 0xF6 || codepoint == 0xF8)
        return 'o';
    if (codepoint == 0xD1) return 'N';
    if (codepoint == 0xF1) return 'n';
    if (codepoint == 0xDE) return 'T';
    if (codepoint == 0xFE) return 't';
    if (codepoint == 0xDF) return 's';
    if (codepoint == 0xC0 || codepoint == 0xC1 || codepoint == 0xC2 ||
        codepoint == 0xC3 || codepoint == 0xC5 || codepoint == 0x100 ||
        codepoint == 0x102 || codepoint == 0x104 || codepoint == 0x106 ||
        codepoint == 0x108 || codepoint == 0x10A || codepoint == 0x10C)
        return 'A';
    if (codepoint == 0xE0 || codepoint == 0xE1 || codepoint == 0xE2 ||
        codepoint == 0xE3 || codepoint == 0xE5 || codepoint == 0x101 ||
        codepoint == 0x103 || codepoint == 0x105 || codepoint == 0x107 ||
        codepoint == 0x109 || codepoint == 0x10B || codepoint == 0x10D)
        return 'a';
    if (codepoint == 0xC8 || codepoint == 0xC9 || codepoint == 0xCA ||
        codepoint == 0xCB || codepoint == 0x112 || codepoint == 0x114 ||
        codepoint == 0x116 || codepoint == 0x118 || codepoint == 0x11A)
        return 'E';
    if (codepoint == 0xE8 || codepoint == 0xE9 || codepoint == 0xEA ||
        codepoint == 0xEB || codepoint == 0x113 || codepoint == 0x115 ||
        codepoint == 0x117 || codepoint == 0x119 || codepoint == 0x11B)
        return 'e';
    if (codepoint == 0xCC || codepoint == 0xCD || codepoint == 0xCE ||
        codepoint == 0xCF || codepoint == 0x128 || codepoint == 0x12A ||
        codepoint == 0x12C || codepoint == 0x12E || codepoint == 0x130)
        return 'I';
    if (codepoint == 0xEC || codepoint == 0xED || codepoint == 0xEE ||
        codepoint == 0xEF || codepoint == 0x129 || codepoint == 0x12B ||
        codepoint == 0x12D || codepoint == 0x12F || codepoint == 0x131)
        return 'i';
    if (codepoint == 0xD9 || codepoint == 0xDA || codepoint == 0xDB ||
        codepoint == 0xDC || codepoint == 0x168 || codepoint == 0x16A ||
        codepoint == 0x16C || codepoint == 0x16E || codepoint == 0x170 ||
        codepoint == 0x172)
        return 'U';
    if (codepoint == 0xF9 || codepoint == 0xFA || codepoint == 0xFB ||
        codepoint == 0xFC || codepoint == 0x169 || codepoint == 0x16B ||
        codepoint == 0x16D || codepoint == 0x16F || codepoint == 0x171 ||
        codepoint == 0x173)
        return 'u';
    if (codepoint == 0xDD || codepoint == 0x176 || codepoint == 0x178)
        return 'Y';
    if (codepoint == 0xFD || codepoint == 0xFF)
        return 'y';
    if (codepoint == 0x141) return 'L';
    if (codepoint == 0x142) return 'l';
    if (codepoint == 0x143 || codepoint == 0x145 || codepoint == 0x147)
        return 'N';
    if (codepoint == 0x144 || codepoint == 0x146 || codepoint == 0x148)
        return 'n';
    if (codepoint == 0x15A || codepoint == 0x15C || codepoint == 0x15E ||
        codepoint == 0x160)
        return 'S';
    if (codepoint == 0x15B || codepoint == 0x15D || codepoint == 0x15F ||
        codepoint == 0x161)
        return 's';
    if (codepoint == 0x179 || codepoint == 0x17B || codepoint == 0x17D)
        return 'Z';
    if (codepoint == 0x17A || codepoint == 0x17C || codepoint == 0x17E)
        return 'z';
    if (codepoint == 0x2018 || codepoint == 0x2019 ||
        codepoint == 0x201A || codepoint == 0x201B)
        return '\'';
    if (codepoint == 0x201C || codepoint == 0x201D ||
        codepoint == 0x201E || codepoint == 0x201F)
        return '"';
    if (codepoint == 0x2013 || codepoint == 0x2014 ||
        codepoint == 0x2212)
        return '-';
    if (codepoint == 0x2022 || codepoint == 0x00B7)
        return '*';
    if (codepoint == 0x2026) return '.';
    if (codepoint == 0x2190) return '<';
    if (codepoint == 0x2192) return '>';
    if (codepoint == 0x00A9) return 'c';
    if (codepoint == 0x00AE) return 'R';
    if (codepoint == 0x2122) return 'T';
    if (codepoint == 0x20AC || codepoint == 0x00A3 ||
        codepoint == 0x00A5 || codepoint == 0x00A2)
        return '$';
    if (codepoint == 0x00B0) return 'o';
    if (codepoint == 0x00B1) return '+';
    if (codepoint == 0x00D7) return 'x';
    if (codepoint == 0x00F7) return '/';
    return '?';
}

static bool browser_decode_entity(const char* source, int& index, char& output)
{
    if (source[index] != '&')
    {
        return false;
    }

    if (source[index + 1] == '#')
    {
        int cursor = index + 2;
        int base = 10;
        if (source[cursor] == 'x' || source[cursor] == 'X')
        {
            base = 16;
            ++cursor;
        }
        uint32_t value = 0;
        int digits = 0;
        bool invalid = false;
        while (source[cursor] != '\0' && source[cursor] != ';')
        {
            uint32_t digit;
            if (source[cursor] >= '0' && source[cursor] <= '9')
            {
                digit = static_cast<uint32_t>(source[cursor] - '0');
            }
            else if (base == 16 && source[cursor] >= 'a' && source[cursor] <= 'f')
            {
                digit = static_cast<uint32_t>(source[cursor] - 'a' + 10);
            }
            else if (base == 16 && source[cursor] >= 'A' && source[cursor] <= 'F')
            {
                digit = static_cast<uint32_t>(source[cursor] - 'A' + 10);
            }
            else
            {
                break;
            }
            if (value > (0x10FFFFu - digit) / static_cast<uint32_t>(base))
            {
                invalid = true;
            }
            else if (!invalid)
            {
                value = value * static_cast<uint32_t>(base) + digit;
            }
            ++cursor;
            ++digits;
        }
        if (digits > 0)
        {
            output = invalid || value == 0 || value > 0x10FFFF ||
                     (value >= 0xD800 && value <= 0xDFFF)
                ? '?' : browser_unicode_to_ascii(value);
            index = source[cursor] == ';' ? cursor + 1 : cursor;
            return true;
        }
    }

    struct named_entity
    {
        const char* name;
        char value;
    };
    static const named_entity entities[] = {
        {"amp;", '&'}, {"lt;", '<'}, {"gt;", '>'}, {"quot;", '"'},
        {"apos;", '\''}, {"nbsp;", ' '}, {"ensp;", ' '}, {"emsp;", ' '},
        {"thinsp;", ' '}, {"ndash;", '-'}, {"mdash;", '-'},
        {"minus;", '-'}, {"hellip;", '.'}, {"bull;", '*'},
        {"middot;", '*'}, {"lsquo;", '\''}, {"rsquo;", '\''},
        {"sbquo;", '\''}, {"ldquo;", '"'}, {"rdquo;", '"'},
        {"bdquo;", '"'}, {"laquo;", '<'}, {"raquo;", '>'},
        {"copy;", 'c'}, {"reg;", 'R'}, {"trade;", 'T'},
        {"euro;", '$'}, {"pound;", '$'}, {"yen;", '$'}, {"cent;", '$'},
        {"deg;", 'o'}, {"plusmn;", '+'}, {"times;", 'x'}, {"divide;", '/'},
        {"larr;", '<'}, {"rarr;", '>'}, {"uarr;", '^'}, {"darr;", 'v'},
        {"check;", 'v'}, {"eacute;", 'e'}, {"Eacute;", 'E'},
        {"aacute;", 'a'}, {"Aacute;", 'A'}, {"ouml;", 'o'},
        {"Ouml;", 'O'}, {"uuml;", 'u'}, {"Uuml;", 'U'},
        {"ntilde;", 'n'}, {"Ntilde;", 'N'}, {"aring;", 'a'},
        {"Aring;", 'A'}, {"szlig;", 's'}, {"ccedil;", 'c'},
        {"Ccedil;", 'C'}, {"oslash;", 'o'}, {"Oslash;", 'O'},
        {"lstrok;", 'l'}, {"Lstrok;", 'L'}, {"zacute;", 'z'},
        {"Zacute;", 'Z'}, {"zdot;", 'z'}, {"Zdot;", 'Z'},
        {"cacute;", 'c'}, {"Cacute;", 'C'}, {"nacute;", 'n'},
        {"Nacute;", 'N'}, {"AMP;", '&'}, {"LT;", '<'}, {"GT;", '>'},
        {"QUOT;", '"'}, {"COPY;", 'c'}, {"REG;", 'R'}, {"TRADE;", 'T'},
        {"Eacute;", 'E'}, {"eacute;", 'e'}, {"Oacute;", 'O'},
        {"oacute;", 'o'}, {"Uacute;", 'U'}, {"uacute;", 'u'},
        {"Auml;", 'A'}, {"auml;", 'a'}, {"Euml;", 'E'},
        {"euml;", 'e'}, {"Iuml;", 'I'}, {"iuml;", 'i'},
        {"Yacute;", 'Y'}, {"yacute;", 'y'}
    };
    for (const named_entity& entity : entities)
    {
        int offset = 0;
        while (entity.name[offset] != '\0' &&
               source[index + 1 + offset] == entity.name[offset])
        {
            ++offset;
        }
        if (entity.name[offset] == '\0')
        {
            output = entity.value;
            index += offset + 1;
            return true;
        }
    }
    return false;
}

static char browser_lower(char character)
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character - 'A' + 'a') : character;
}

static bool browser_parse_color(const char* value, uint32_t& color)
{
    if (value[0] == '#')
    {
        uint32_t parsed = 0;
        int digits = 0;
        for (int index = 1; value[index] != '\0'; ++index)
        {
            char character = value[index];
            uint32_t digit;
            if (character >= '0' && character <= '9') digit = static_cast<uint32_t>(character - '0');
            else if (character >= 'a' && character <= 'f') digit = static_cast<uint32_t>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F') digit = static_cast<uint32_t>(character - 'A' + 10);
            else return false;
            parsed = (parsed << 4) | digit;
            ++digits;
        }
        if (digits == 3)
        {
            color = 0xFF000000u |
                (((parsed >> 8) & 15) * 17 << 16) |
                (((parsed >> 4) & 15) * 17 << 8) |
                ((parsed & 15) * 17);
            return true;
        }
        if (digits == 4)
        {
            const uint32_t alpha = (parsed & 15) * 17;
            const uint32_t red = ((parsed >> 12) & 15) * 17;
            const uint32_t green = ((parsed >> 8) & 15) * 17;
            const uint32_t blue = ((parsed >> 4) & 15) * 17;
            color = 0xFF000000u |
                (((red * alpha + 8 * (255 - alpha)) / 255) << 16) |
                (((green * alpha + 11 * (255 - alpha)) / 255) << 8) |
                ((blue * alpha + 18 * (255 - alpha)) / 255);
            return true;
        }
        if (digits == 6)
        {
            color = 0xFF000000u | parsed;
            return true;
        }
        if (digits == 8)
        {
            const uint32_t alpha = parsed & 0xFF;
            const uint32_t red = (parsed >> 24) & 0xFF;
            const uint32_t green = (parsed >> 16) & 0xFF;
            const uint32_t blue = (parsed >> 8) & 0xFF;
            const uint32_t background_red = (browser_page_background >> 16) & 0xFF;
            const uint32_t background_green = (browser_page_background >> 8) & 0xFF;
            const uint32_t background_blue = browser_page_background & 0xFF;
            color = 0xFF000000u |
                (((red * alpha + background_red * (255 - alpha)) / 255) << 16) |
                (((green * alpha + background_green * (255 - alpha)) / 255) << 8) |
                ((blue * alpha + background_blue * (255 - alpha)) / 255);
            return true;
        }
        return false;
    }

    if (browser_has_prefix_ignore_case(value, "rgb("))
    {
        uint32_t channels[3] = {};
        int channel = 0;
        int index = 4;
        while (channel < 3)
        {
            while (value[index] == ' ' || value[index] == '\t') ++index;
            int digits = 0;
            while (value[index] >= '0' && value[index] <= '9')
            {
                channels[channel] = channels[channel] * 10 + static_cast<uint32_t>(value[index++] - '0');
                if (channels[channel] > 255) channels[channel] = 255;
                ++digits;
            }
            if (digits == 0) return false;
            ++channel;
            while (value[index] == ' ' || value[index] == '\t') ++index;
            if (channel < 3)
            {
                if (value[index++] != ',') return false;
            }
        }
        while (value[index] == ' ' || value[index] == '\t') ++index;
        if (value[index] != ')' || value[index + 1] != '\0') return false;
        color = 0xFF000000u | (channels[0] << 16) | (channels[1] << 8) | channels[2];
        return true;
    }

    char name[16] = {};
    int length = 0;
    while (value[length] != '\0' && length < static_cast<int>(sizeof(name) - 1))
    {
        name[length] = browser_lower(value[length]);
        ++length;
    }
    if (value[length] != '\0')
    {
        return false;
    }

    if (browser_equal_ignore_case(name, "black")) color = 0xFF000000;
    else if (browser_equal_ignore_case(name, "white")) color = 0xFFFFFFFF;
    else if (browser_equal_ignore_case(name, "red")) color = 0xFFFF0000;
    else if (browser_equal_ignore_case(name, "green")) color = 0xFF008000;
    else if (browser_equal_ignore_case(name, "blue")) color = 0xFF0000FF;
    else if (browser_equal_ignore_case(name, "yellow")) color = 0xFFFFFF00;
    else if (browser_equal_ignore_case(name, "gray") || browser_equal_ignore_case(name, "grey")) color = 0xFF808080;
    else if (browser_equal_ignore_case(name, "orange")) color = 0xFFFFA500;
    else if (browser_equal_ignore_case(name, "purple")) color = 0xFF800080;
    else if (browser_equal_ignore_case(name, "teal")) color = 0xFF008080;
    else if (browser_equal_ignore_case(name, "navy")) color = 0xFF000080;
    else if (browser_equal_ignore_case(name, "silver")) color = 0xFFC0C0C0;
    else if (browser_equal_ignore_case(name, "maroon")) color = 0xFF800000;
    else if (browser_equal_ignore_case(name, "lime")) color = 0xFF00FF00;
    else if (browser_equal_ignore_case(name, "aqua") || browser_equal_ignore_case(name, "cyan")) color = 0xFF00FFFF;
    else if (browser_equal_ignore_case(name, "fuchsia") || browser_equal_ignore_case(name, "magenta")) color = 0xFFFF00FF;
    else if (browser_equal_ignore_case(name, "pink")) color = 0xFFFFC0CB;
    else if (browser_equal_ignore_case(name, "hotpink")) color = 0xFFFF69B4;
    else if (browser_equal_ignore_case(name, "brown")) color = 0xFFA52A2A;
    else if (browser_equal_ignore_case(name, "gold")) color = 0xFFFFD700;
    else if (browser_equal_ignore_case(name, "coral")) color = 0xFFFF7F50;
    else if (browser_equal_ignore_case(name, "tomato")) color = 0xFFFF6347;
    else if (browser_equal_ignore_case(name, "indigo")) color = 0xFF4B0082;
    else if (browser_equal_ignore_case(name, "violet")) color = 0xFFEE82EE;
    else if (browser_equal_ignore_case(name, "lightblue")) color = 0xFFADD8E6;
    else if (browser_equal_ignore_case(name, "lightgreen")) color = 0xFF90EE90;
    else if (browser_equal_ignore_case(name, "darkgray") || browser_equal_ignore_case(name, "darkgrey")) color = 0xFFA9A9A9;
    else if (browser_equal_ignore_case(name, "rebeccapurple")) color = 0xFF663399;
    else if (browser_equal_ignore_case(name, "transparent")) color = BROWSER_NO_BACKGROUND;
    else return false;
    return true;
}

static bool browser_parse_gradient_color(const char* value, uint32_t& color)
{
    if (!browser_has_prefix_ignore_case(value, "linear-gradient(") &&
        !browser_has_prefix_ignore_case(value, "radial-gradient("))
        return false;

    for (int index = 0; value[index] != '\0' && value[index] != ')'; ++index)
    {
        if (value[index] != '#') continue;
        char candidate[10] = {'#'};
        int length = 1;
        while (value[index + length] != '\0' && length < 9)
        {
            const char digit = value[index + length];
            if (!((digit >= '0' && digit <= '9') ||
                  (digit >= 'a' && digit <= 'f') ||
                  (digit >= 'A' && digit <= 'F')))
                break;
            candidate[length] = digit;
            ++length;
        }
        candidate[length] = '\0';
        if (browser_parse_color(candidate, color)) return true;
        index += length - 1;
    }
    return false;
}

static bool browser_value_has_word(const char* value, const char* word)
{
    int word_length = browser_length(word);
    for (int index = 0; value[index] != '\0';)
    {
        while (value[index] == ' ' || value[index] == '\t') ++index;
        const int start = index;
        while (value[index] != '\0' && value[index] != ' ' && value[index] != '\t') ++index;
        if (index - start != word_length) continue;
        int character = 0;
        while (character < word_length &&
               browser_lower(value[start + character]) == browser_lower(word[character]))
        {
            ++character;
        }
        if (character == word_length) return true;
    }
    return false;
}

static const char* browser_find_custom_property(const char* name)
{
    for (int index = 0; index < browser_custom_property_count; ++index)
    {
        if (browser_equal_ignore_case(browser_custom_properties[index].name, name))
            return browser_custom_properties[index].value;
    }
    return nullptr;
}

static void browser_collect_custom_properties(const char* start, const char* end)
{
    while (start < end)
    {
        while (start < end && (*start == ' ' || *start == '\t' ||
                               *start == '\r' || *start == '\n' || *start == ';'))
            ++start;
        const char* colon = start;
        while (colon < end && *colon != ':' && *colon != ';') ++colon;
        if (colon >= end || *colon != ':')
        {
            start = colon < end ? colon + 1 : end;
            continue;
        }
        const char* name_start = start;
        while (name_start < colon && (*name_start == ' ' || *name_start == '\t'))
            ++name_start;
        const char* name_end = colon;
        while (name_end > name_start && (name_end[-1] == ' ' || name_end[-1] == '\t'))
            --name_end;
        const char* value_start = colon + 1;
        while (value_start < end && (*value_start == ' ' || *value_start == '\t'))
            ++value_start;
        const char* value_end = value_start;
        while (value_end < end && *value_end != ';') ++value_end;
        while (value_end > value_start &&
               (value_end[-1] == ' ' || value_end[-1] == '\t' ||
                value_end[-1] == '\r' || value_end[-1] == '\n'))
            --value_end;
        if (name_end - name_start > 2 && name_start[0] == '-' && name_start[1] == '-')
        {
            char name[32] = {};
            char value[64] = {};
            int name_length = static_cast<int>(name_end - name_start);
            int value_length = static_cast<int>(value_end - value_start);
            if (name_length >= static_cast<int>(sizeof(name)))
                name_length = sizeof(name) - 1;
            if (value_length >= static_cast<int>(sizeof(value)))
                value_length = sizeof(value) - 1;
            for (int i = 0; i < name_length; ++i)
                name[i] = name_start[i];
            for (int i = 0; i < value_length; ++i)
                value[i] = value_start[i];

            int property_index = 0;
            while (property_index < browser_custom_property_count &&
                   !browser_equal_ignore_case(browser_custom_properties[property_index].name,
                                              name))
                ++property_index;
            if (property_index == browser_custom_property_count &&
                browser_custom_property_count < BROWSER_MAX_CUSTOM_PROPERTIES)
                ++browser_custom_property_count;
            if (property_index < BROWSER_MAX_CUSTOM_PROPERTIES)
            {
                browser_custom_property& property =
                    browser_custom_properties[property_index];
                int i = 0;
                while (name[i] != '\0')
                {
                    property.name[i] = browser_lower(name[i]);
                    ++i;
                }
                property.name[i] = '\0';
                i = 0;
                while (value[i] != '\0')
                {
                    property.value[i] = value[i];
                    ++i;
                }
                property.value[i] = '\0';
            }
        }
        start = value_end < end ? value_end + 1 : end;
    }
}

static uint8_t browser_css_spacing(const char* value)
{
    if (value[0] < '0' || value[0] > '9') return 0;
    uint32_t amount = 0;
    for (int index = 0; value[index] >= '0' && value[index] <= '9'; ++index)
    {
        amount = amount * 10 + static_cast<uint32_t>(value[index] - '0');
        if (amount >= 32) return 2;
    }
    return amount >= 8 ? 1 : 0;
}

static bool browser_css_parse_length(const char* value, uint16_t& pixels,
                                     uint8_t& percent)
{
    uint32_t amount = 0;
    int index = 0;
    while (value[index] >= '0' && value[index] <= '9')
    {
        amount = amount * 10 + static_cast<uint32_t>(value[index++] - '0');
        if (amount > 4096) amount = 4096;
    }
    if (index == 0) return false;

    if (value[index] == '%')
    {
        if (value[index + 1] != '\0') return false;
        pixels = 0;
        percent = static_cast<uint8_t>(amount > 255 ? 255 : amount);
        return true;
    }
    if (value[index] == '\0' ||
        browser_has_prefix_ignore_case(value + index, "px"))
    {
        if (value[index] != '\0' && value[index + 2] != '\0')
            return false;
    }
    else if (browser_has_prefix_ignore_case(value + index, "rem"))
    {
        if (value[index + 3] != '\0') return false;
        amount *= 16;
        if (amount > 4096) amount = 4096;
    }
    else if (browser_has_prefix_ignore_case(value + index, "em"))
    {
        if (value[index + 2] != '\0') return false;
        amount *= 16;
        if (amount > 4096) amount = 4096;
    }
    else
    {
        return false;
    }
    pixels = static_cast<uint16_t>(amount);
    percent = 0;
    return true;
}

static bool browser_css_parse_width(const char* value, uint16_t& pixels,
                                   uint8_t& percent, uint16_t& max_pixels,
                                   bool& is_set)
{
    if (browser_equal_ignore_case(value, "auto"))
    {
        pixels = 0;
        percent = 0;
        max_pixels = 0;
        is_set = false;
        return true;
    }
    if (browser_has_prefix_ignore_case(value, "min("))
    {
        const char* comma = value + 4;
        while (*comma != '\0' && *comma != ',' && *comma != ')') ++comma;
        if (*comma != ',') return false;
        char first[32] = {};
        char second[32] = {};
        int first_length = static_cast<int>(comma - (value + 4));
        while (first_length > 0 &&
               (value[4 + first_length - 1] == ' ' ||
                value[4 + first_length - 1] == '\t'))
            --first_length;
        int first_start = 4;
        while (first_start < 4 + first_length &&
               (value[first_start] == ' ' || value[first_start] == '\t'))
        {
            ++first_start;
            --first_length;
        }
        if (first_length <= 0 || first_length >= static_cast<int>(sizeof(first)))
            return false;
        for (int index = 0; index < first_length; ++index)
            first[index] = value[first_start + index];

        const char* second_start = comma + 1;
        while (*second_start == ' ' || *second_start == '\t') ++second_start;
        int second_length = 0;
        while (second_start[second_length] != '\0' &&
               second_start[second_length] != ')')
            ++second_length;
        while (second_length > 0 &&
               (second_start[second_length - 1] == ' ' ||
                second_start[second_length - 1] == '\t'))
            --second_length;
        if (second_length <= 0 ||
            second_length >= static_cast<int>(sizeof(second)) ||
            second_start[second_length] != ')' ||
            second_start[second_length + 1] != '\0')
            return false;
        for (int index = 0; index < second_length; ++index)
            second[index] = second_start[index];

        uint16_t first_pixels = 0;
        uint16_t second_pixels = 0;
        uint8_t first_percent = 0;
        uint8_t second_percent = 0;
        if (!browser_css_parse_length(first, first_pixels, first_percent) ||
            !browser_css_parse_length(second, second_pixels, second_percent) ||
            first_percent == second_percent)
            return false;
        pixels = first_percent == 0 ? first_pixels : second_pixels;
        percent = first_percent != 0 ? first_percent : second_percent;
        max_pixels = first_percent == 0 ? first_pixels : second_pixels;
        is_set = true;
        return true;
    }

    if (!browser_css_parse_length(value, pixels, percent)) return false;
    max_pixels = 0;
    is_set = true;
    return true;
}

static uint8_t browser_css_padding_chars(const char* value)
{
    uint16_t pixels = 0;
    uint8_t percent = 0;
    if (!browser_css_parse_length(value, pixels, percent) || percent != 0)
        return 0;
    const uint32_t characters = (pixels + 8) / 9;
    return static_cast<uint8_t>(characters > 16 ? 16 : characters);
}

static uint8_t browser_css_grid_columns(const char* value)
{
    if (browser_has_prefix_ignore_case(value, "repeat("))
    {
        uint32_t count = 0;
        int index = 7;
        while (value[index] >= '0' && value[index] <= '9')
        {
            count = count * 10 + static_cast<uint32_t>(value[index++] - '0');
            if (count > 8) count = 8;
        }
        while (value[index] == ' ' || value[index] == '\t') ++index;
        if (count == 0 || value[index] != ',') return 1;
        return static_cast<uint8_t>(count);
    }

    uint8_t count = 0;
    bool inside_token = false;
    for (int index = 0; value[index] != '\0'; ++index)
    {
        const bool whitespace = value[index] == ' ' || value[index] == '\t';
        if (!whitespace && !inside_token)
        {
            if (count < 8) ++count;
            inside_token = true;
        }
        else if (whitespace)
        {
            inside_token = false;
        }
    }
    return count > 0 ? count : 1;
}

static uint8_t browser_css_min_height_lines(const char* value)
{
    uint32_t amount = 0;
    int index = 0;
    while (value[index] >= '0' && value[index] <= '9')
    {
        amount = amount * 10 + static_cast<uint32_t>(value[index++] - '0');
        if (amount > 1000) amount = 1000;
    }
    if (index == 0) return 0;
    while (value[index] == ' ' || value[index] == '\t') ++index;

    uint32_t pixels = amount;
    if (browser_has_prefix_ignore_case(value + index, "vh"))
    {
        pixels = amount * static_cast<uint32_t>(browser_viewport_height) / 100;
    }
    else if (browser_has_prefix_ignore_case(value + index, "vw"))
    {
        pixels = amount * static_cast<uint32_t>(browser_viewport_width) / 100;
    }
    else if (browser_has_prefix_ignore_case(value + index, "em") ||
             browser_has_prefix_ignore_case(value + index, "rem"))
    {
        pixels = amount * 16;
    }
    const uint32_t lines = (pixels + browser_content_height - 1) /
                           browser_content_height;
    return static_cast<uint8_t>(lines > 64 ? 64 : lines);
}

static void browser_resolve_css_value(const char* value, char* output, int capacity)
{
    int output_length = 0;
    for (int index = 0; value[index] != '\0' && output_length < capacity - 1;)
    {
        if (browser_has_prefix_ignore_case(value + index, "var("))
        {
            int end = index + 4;
            while (value[end] != '\0' && value[end] != ')' && value[end] != ',')
                ++end;
            int name_start = index + 4;
            while (name_start < end && (value[name_start] == ' ' || value[name_start] == '\t'))
                ++name_start;
            int name_end = end;
            while (name_end > name_start && (value[name_end - 1] == ' ' ||
                                              value[name_end - 1] == '\t'))
                --name_end;
            char name[32] = {};
            int name_length = name_end - name_start;
            if (name_length >= static_cast<int>(sizeof(name)))
                name_length = sizeof(name) - 1;
            for (int i = 0; i < name_length; ++i)
                name[i] = value[name_start + i];
            const char* replacement = browser_find_custom_property(name);
            if (replacement == nullptr && value[end] == ',')
            {
                int fallback = end + 1;
                while (value[fallback] == ' ' || value[fallback] == '\t') ++fallback;
                replacement = value + fallback;
                end = fallback;
                while (value[end] != '\0' && value[end] != ')') ++end;
            }
            if (replacement != nullptr)
            {
                while (*replacement != '\0' && output_length < capacity - 1)
                    output[output_length++] = *replacement++;
                index = value[end] == ')' ? end + 1 : end;
                continue;
            }
        }
        output[output_length++] = value[index++];
    }
    output[output_length] = '\0';
}

static uint16_t browser_selector_specificity(const char* selector)
{
    uint16_t specificity = 0;
    for (int index = 0; selector[index] != '\0'; ++index)
    {
        if (selector[index] == '#') specificity += 100;
        else if (selector[index] == '.') specificity += 10;
    }
    if (specificity == 0 && selector[0] != '*') specificity = 1;
    return specificity;
}

static void browser_apply_declarations(const char* start, const char* end,
                                       browser_style& style, uint16_t specificity)
{
    while (start < end)
    {
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == ';'))
        {
            ++start;
        }
        if (start + 1 < end && start[0] == '/' && start[1] == '*')
        {
            start += 2;
            while (start + 1 < end && !(start[0] == '*' && start[1] == '/')) ++start;
            start = start + 1 < end ? start + 2 : end;
            continue;
        }
        const char* colon = start;
        while (colon < end && *colon != ':' && *colon != ';')
        {
            ++colon;
        }
        if (colon >= end || *colon != ':')
        {
            start = colon < end ? colon + 1 : end;
            continue;
        }

        char property[32] = {};
        int property_length = 0;
        for (const char* cursor = start; cursor < colon && property_length < static_cast<int>(sizeof(property) - 1); ++cursor)
        {
            if (*cursor != ' ' && *cursor != '\t')
            {
                property[property_length++] = browser_lower(*cursor);
            }
        }
        const char* value_start = colon + 1;
        const char* value_end = value_start;
        while (value_end < end && *value_end != ';')
        {
            ++value_end;
        }
        while (value_start < value_end && (*value_start == ' ' || *value_start == '\t' || *value_start == '\r' || *value_start == '\n'))
        {
            ++value_start;
        }
        while (value_end > value_start && (value_end[-1] == ' ' || value_end[-1] == '\t' || value_end[-1] == '\r' || value_end[-1] == '\n'))
        {
            --value_end;
        }
        char raw_value[96] = {};
        int value_length = 0;
        for (const char* cursor = value_start;
             cursor < value_end && value_length < static_cast<int>(sizeof(raw_value) - 1);
             ++cursor)
        {
            raw_value[value_length++] = *cursor;
        }
        char resolved_value[96] = {};
        browser_resolve_css_value(raw_value, resolved_value, sizeof(resolved_value));
        char value[96] = {};
        for (int index = 0; resolved_value[index] != '\0'; ++index)
            value[index] = browser_lower(resolved_value[index]);

        uint32_t parsed_color = 0;
        if (browser_equal_ignore_case(property, "min-height") &&
            specificity >= style.min_height_specificity)
        {
            style.min_height_lines = browser_css_min_height_lines(value);
            style.min_height_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "width") &&
                 specificity >= style.width_specificity)
        {
            uint16_t pixels = 0;
            uint16_t max_pixels = 0;
            uint8_t percent = 0;
            bool is_set = false;
            if (browser_css_parse_width(value, pixels, percent, max_pixels,
                                        is_set))
            {
                style.width_pixels = pixels;
                style.width_percent = percent;
                style.width_cap_pixels = max_pixels;
                style.width_set = is_set;
                style.width_specificity = specificity;
            }
        }
        else if (browser_equal_ignore_case(property, "max-width") &&
                 specificity >= style.max_width_specificity)
        {
            uint16_t pixels = 0;
            uint8_t percent = 0;
            if (browser_equal_ignore_case(value, "none"))
                style.max_width_pixels = 0;
            else if (!browser_css_parse_length(value, pixels, percent) ||
                     percent != 0)
            {
                start = value_end < end ? value_end + 1 : end;
                continue;
            }
            else
                style.max_width_pixels = pixels;
            style.max_width_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "padding-left") ||
                 browser_equal_ignore_case(property, "padding-right"))
        {
            const bool is_left =
                browser_equal_ignore_case(property, "padding-left");
            uint16_t pixels = 0;
            uint8_t percent = 0;
            if (browser_css_parse_length(value, pixels, percent) &&
                percent == 0)
            {
                if (is_left &&
                    specificity >= style.padding_left_specificity)
                {
                    style.padding_left_chars =
                        browser_css_padding_chars(value);
                    style.padding_left_specificity = specificity;
                }
                else if (!is_left &&
                         specificity >= style.padding_right_specificity)
                {
                    style.padding_right_chars =
                        browser_css_padding_chars(value);
                    style.padding_right_specificity = specificity;
                }
            }
        }
        else if (browser_equal_ignore_case(property, "margin-left") ||
                 browser_equal_ignore_case(property, "margin-right"))
        {
            const bool is_left =
                browser_equal_ignore_case(property, "margin-left");
            uint16_t ignored_pixels = 0;
            uint8_t ignored_percent = 0;
            const bool is_auto = browser_equal_ignore_case(value, "auto");
            const bool valid_value = is_auto ||
                browser_css_parse_length(value, ignored_pixels,
                                         ignored_percent);
            if (valid_value)
            {
                if (is_left &&
                    specificity >= style.margin_left_specificity)
                {
                    style.margin_left_auto = is_auto;
                    style.margin_left_specificity = specificity;
                }
                else if (!is_left &&
                         specificity >= style.margin_right_specificity)
                {
                    style.margin_right_auto = is_auto;
                    style.margin_right_specificity = specificity;
                }
            }
        }
        else if ((browser_equal_ignore_case(property, "margin-top") ||
             browser_equal_ignore_case(property, "padding-top")) &&
            specificity >= style.space_before_specificity)
        {
            style.space_before = browser_css_spacing(value);
            style.space_before_specificity = specificity;
        }
        else if ((browser_equal_ignore_case(property, "margin-bottom") ||
                  browser_equal_ignore_case(property, "padding-bottom") ||
                  browser_equal_ignore_case(property, "gap") ||
                  browser_equal_ignore_case(property, "row-gap")) &&
                 specificity >= style.space_after_specificity)
        {
            style.space_after = browser_css_spacing(value);
            style.space_after_specificity = specificity;
            if (browser_equal_ignore_case(property, "gap") &&
                specificity >= style.grid_gap_specificity)
            {
                style.grid_gap_chars = browser_css_padding_chars(value);
                style.grid_gap_specificity = specificity;
            }
        }
        else if (browser_equal_ignore_case(property, "column-gap") &&
                 specificity >= style.grid_gap_specificity)
        {
            style.grid_gap_chars = browser_css_padding_chars(value);
            style.grid_gap_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "grid-template-columns") &&
                 specificity >= style.grid_columns_specificity)
        {
            style.grid_columns = browser_css_grid_columns(value);
            style.grid_columns_specificity = specificity;
        }
        else if ((browser_equal_ignore_case(property, "margin") ||
                  browser_equal_ignore_case(property, "padding")))
        {
            char tokens[4][24] = {};
            int token_count = 0;
            int cursor = 0;
            while (value[cursor] != '\0' && token_count < 4)
            {
                while (value[cursor] == ' ' || value[cursor] == '\t') ++cursor;
                if (value[cursor] == '\0') break;
                int length = 0;
                while (value[cursor] != '\0' && value[cursor] != ' ' &&
                       value[cursor] != '\t')
                {
                    if (length < static_cast<int>(sizeof(tokens[0]) - 1))
                        tokens[token_count][length++] = value[cursor];
                    ++cursor;
                }
                ++token_count;
            }
            if (specificity >= style.space_before_specificity)
            {
                style.space_before = token_count > 0
                    ? browser_css_spacing(tokens[0]) : 0;
                style.space_before_specificity = specificity;
            }
            if (specificity >= style.space_after_specificity)
            {
                const int bottom_index = token_count >= 3 ? 2 : 0;
                style.space_after = token_count > 0
                    ? browser_css_spacing(tokens[bottom_index]) : 0;
                style.space_after_specificity = specificity;
            }
            if (browser_equal_ignore_case(property, "padding"))
            {
                const int right_index = token_count >= 2 ? 1 : 0;
                const int left_index = token_count >= 4 ? 3 : right_index;
                if (specificity >= style.padding_left_specificity)
                {
                    style.padding_left_chars = token_count > 0
                        ? browser_css_padding_chars(tokens[left_index]) : 0;
                    style.padding_left_specificity = specificity;
                }
                if (specificity >= style.padding_right_specificity)
                {
                    style.padding_right_chars = token_count > 0
                        ? browser_css_padding_chars(tokens[right_index]) : 0;
                    style.padding_right_specificity = specificity;
                }
            }
            else
            {
                const int right_index = token_count >= 2 ? 1 : 0;
                const int left_index = token_count >= 4 ? 3 : right_index;
                if (specificity >= style.margin_left_specificity)
                {
                    style.margin_left_auto = token_count > left_index &&
                        browser_equal_ignore_case(tokens[left_index], "auto");
                    style.margin_left_specificity = specificity;
                }
                if (specificity >= style.margin_right_specificity)
                {
                    style.margin_right_auto = token_count > right_index &&
                        browser_equal_ignore_case(tokens[right_index], "auto");
                    style.margin_right_specificity = specificity;
                }
            }
        }
        else if ((browser_equal_ignore_case(property, "border") ||
             browser_equal_ignore_case(property, "border-color")) &&
            specificity >= style.border_specificity)
        {
            if (browser_equal_ignore_case(property, "border"))
            {
                style.border_enabled = !browser_value_has_word(value, "none") &&
                                       !browser_value_has_word(value, "hidden");
                style.border_color = style.color;
                style.border_specificity = specificity;
            }
            int token_start = 0;
            while (value[token_start] != '\0')
            {
                while (value[token_start] == ' ' || value[token_start] == '\t')
                    ++token_start;
                int token_end = token_start;
                while (value[token_end] != '\0' && value[token_end] != ' ' &&
                       value[token_end] != '\t')
                {
                    ++token_end;
                }
                const char saved = value[token_end];
                value[token_end] = '\0';
                uint32_t border_color = 0;
                const bool is_color = browser_parse_color(value + token_start,
                                                          border_color);
                value[token_end] = saved;
                if (is_color)
                {
                    style.border_color = border_color;
                    if (browser_equal_ignore_case(property, "border-color"))
                        style.border_enabled = true;
                    style.border_specificity = specificity;
                    break;
                }
                if (token_end == token_start) break;
                token_start = token_end;
            }
        }
        else if (browser_equal_ignore_case(property, "border-style") &&
                 specificity >= style.border_specificity)
        {
            style.border_enabled = !browser_equal_ignore_case(value, "none") &&
                                   !browser_equal_ignore_case(value, "hidden");
            if (style.border_enabled && style.border_color == BROWSER_NO_BACKGROUND)
                style.border_color = style.color;
            style.border_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "border-width") &&
                 specificity >= style.border_specificity)
        {
            style.border_enabled = value[0] != '0';
            if (style.border_enabled && style.border_color == BROWSER_NO_BACKGROUND)
                style.border_color = style.color;
            style.border_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "color") ||
                 browser_equal_ignore_case(property, "background") ||
                 browser_equal_ignore_case(property, "background-color"))
        {
            const bool is_background = !browser_equal_ignore_case(property, "color");
            const bool has_color = browser_parse_color(value, parsed_color) ||
                (is_background && browser_parse_gradient_color(value, parsed_color));
            if (has_color && is_background &&
                specificity >= style.background_specificity)
            {
                style.background = parsed_color;
                style.background_specificity = specificity;
            }
            else if (has_color && !is_background &&
                     specificity >= style.color_specificity)
            {
                style.color = parsed_color;
                style.color_specificity = specificity;
            }
        }
        else if (browser_equal_ignore_case(property, "font-weight") &&
                 specificity >= style.font_weight_specificity)
        {
            if (browser_equal_ignore_case(value, "bold") ||
                browser_equal_ignore_case(value, "bolder") ||
                (value[0] >= '6' && value[0] <= '9'))
            {
                style.flags |= BROWSER_BOLD;
            }
            else if (browser_equal_ignore_case(value, "normal") || value[0] == '4' || value[0] == '5')
            {
                style.flags &= static_cast<uint8_t>(~BROWSER_BOLD);
            }
            style.font_weight_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "text-decoration") &&
                 specificity >= style.text_decoration_specificity)
        {
            const uint8_t decoration_mask =
                BROWSER_UNDERLINE | BROWSER_STRIKETHROUGH | BROWSER_OVERLINE;
            style.flags &= static_cast<uint8_t>(~decoration_mask);
            if (browser_value_has_word(value, "underline"))
                style.flags |= BROWSER_UNDERLINE;
            if (browser_value_has_word(value, "line-through"))
                style.flags |= BROWSER_STRIKETHROUGH;
            if (browser_value_has_word(value, "overline"))
                style.flags |= BROWSER_OVERLINE;
            style.text_decoration_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "text-transform") &&
                 specificity >= style.text_transform_specificity)
        {
            const uint8_t transform_mask =
                BROWSER_UPPERCASE | BROWSER_LOWERCASE | BROWSER_CAPITALIZE;
            style.flags &= static_cast<uint8_t>(~transform_mask);
            if (browser_equal_ignore_case(value, "uppercase"))
                style.flags |= BROWSER_UPPERCASE;
            else if (browser_equal_ignore_case(value, "lowercase"))
                style.flags |= BROWSER_LOWERCASE;
            else if (browser_equal_ignore_case(value, "capitalize"))
                style.flags |= BROWSER_CAPITALIZE;
            style.text_transform_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "white-space") &&
                 specificity >= style.white_space_specificity)
        {
            style.preformatted = browser_equal_ignore_case(value, "pre") ||
                                browser_equal_ignore_case(value, "pre-wrap");
            style.white_space_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "text-align") &&
                 specificity >= style.alignment_specificity)
        {
            if (browser_equal_ignore_case(value, "center")) style.alignment = BROWSER_ALIGN_CENTER;
            else if (browser_equal_ignore_case(value, "right")) style.alignment = BROWSER_ALIGN_RIGHT;
            else style.alignment = BROWSER_ALIGN_LEFT;
            style.alignment_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "align-items") &&
                 specificity >= style.align_items_specificity)
        {
            style.align_items_center =
                browser_equal_ignore_case(value, "center");
            style.align_items_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "display") &&
                 specificity >= style.display_specificity)
        {
            style.hidden = browser_equal_ignore_case(value, "none");
            style.hidden_specificity = specificity;
            if (browser_equal_ignore_case(value, "grid"))
                style.layout_mode = BROWSER_LAYOUT_GRID;
            else if (browser_equal_ignore_case(value, "flex"))
                style.layout_mode = BROWSER_LAYOUT_FLEX;
            else
                style.layout_mode = BROWSER_LAYOUT_NORMAL;
            style.display_specificity = specificity;
        }
        else if (browser_equal_ignore_case(property, "visibility") &&
                 specificity >= style.hidden_specificity)
        {
            style.hidden = browser_equal_ignore_case(value, "hidden");
            style.hidden_specificity = specificity;
        }

        start = value_end;
        while (start < end && *start != ';') ++start;
        if (start < end) ++start;
    }
}

static void browser_add_css_rule(const char* selector_start, const char* selector_end,
                                 const char* declarations_start, const char* declarations_end)
{
    while (selector_start < selector_end && (*selector_start == ' ' || *selector_start == '\t' || *selector_start == '\n'))
    {
        ++selector_start;
    }
    while (selector_end > selector_start && (selector_end[-1] == ' ' || selector_end[-1] == '\t' || selector_end[-1] == '\n'))
    {
        --selector_end;
    }
    if (selector_start == selector_end || browser_css_rule_count >= BROWSER_MAX_CSS_RULES)
    {
        return;
    }

    browser_css_rule& rule = browser_css_rules[browser_css_rule_count];
    rule = {};
    int selector_length = 0;
    while (selector_start + selector_length < selector_end &&
           selector_length < static_cast<int>(sizeof(rule.selector) - 1))
    {
        rule.selector[selector_length] = browser_lower(selector_start[selector_length]);
        ++selector_length;
    }
    rule.selector[selector_length] = '\0';

    browser_style parsed = {};
    parsed.color = BROWSER_TEXT;
    parsed.background = BROWSER_NO_BACKGROUND;
    parsed.border_color = BROWSER_TEXT;
    browser_apply_declarations(declarations_start, declarations_end, parsed, 1);
    rule.color = parsed.color;
    rule.background = parsed.background;
    rule.flags = parsed.flags;
    rule.alignment = parsed.alignment;
    rule.hidden = parsed.hidden;
    rule.preformatted = parsed.preformatted;
    rule.border_enabled = parsed.border_enabled;
    rule.border_color = parsed.border_color;
    rule.space_before = parsed.space_before;
    rule.space_after = parsed.space_after;
    rule.min_height_lines = parsed.min_height_lines;
    rule.width_pixels = parsed.width_pixels;
    rule.width_cap_pixels = parsed.width_cap_pixels;
    rule.max_width_pixels = parsed.max_width_pixels;
    rule.width_percent = parsed.width_percent;
    rule.padding_left_chars = parsed.padding_left_chars;
    rule.padding_right_chars = parsed.padding_right_chars;
    rule.layout_mode = parsed.layout_mode;
    rule.grid_columns = parsed.grid_columns;
    rule.grid_gap_chars = parsed.grid_gap_chars;
    rule.align_items_center = parsed.align_items_center;
    rule.width_set = parsed.width_set;
    rule.margin_left_auto = parsed.margin_left_auto;
    rule.margin_right_auto = parsed.margin_right_auto;
    if (parsed.color_specificity != 0) rule.declarations |= 1;
    if (parsed.background_specificity != 0) rule.declarations |= 2;
    if (parsed.font_weight_specificity != 0) rule.declarations |= 4;
    if (parsed.alignment_specificity != 0) rule.declarations |= 8;
    if (parsed.hidden_specificity != 0) rule.declarations |= 16;
    if (parsed.white_space_specificity != 0) rule.declarations |= 32;
    if (parsed.text_decoration_specificity != 0) rule.declarations |= 64;
    if (parsed.text_transform_specificity != 0) rule.declarations |= 128;
    if (parsed.border_specificity != 0) rule.declarations |= 256;
    if (parsed.space_before_specificity != 0) rule.declarations |= 1024;
    if (parsed.space_after_specificity != 0) rule.declarations |= 2048;
    if (parsed.min_height_specificity != 0) rule.declarations |= 4096;
    if (parsed.width_specificity != 0) rule.declarations |= 8192;
    if (parsed.max_width_specificity != 0) rule.declarations |= 16384;
    if (parsed.padding_left_specificity != 0) rule.declarations |= 32768;
    if (parsed.padding_right_specificity != 0) rule.declarations |= 65536;
    if (parsed.margin_left_specificity != 0) rule.declarations |= 131072;
    if (parsed.margin_right_specificity != 0) rule.declarations |= 262144;
    if (parsed.display_specificity != 0) rule.declarations |= 524288;
    if (parsed.grid_columns_specificity != 0) rule.declarations |= 1048576;
    if (parsed.grid_gap_specificity != 0) rule.declarations |= 2097152;
    if (parsed.align_items_specificity != 0) rule.declarations |= 4194304;
    ++browser_css_rule_count;
}

static int browser_read_decoded_attribute(const char* source, int start, int end,
                                          const char* name, char* output,
                                          int capacity);
static bool browser_load_stylesheet(const char* href, uint8_t redirects = 0);

static bool browser_media_query_matches(const char* start, const char* end)
{
    while (start < end && (*start == ' ' || *start == '\t' ||
                           *start == '\r' || *start == '\n'))
        ++start;
    if (start >= end) return true;

    bool negate = false;
    if (end - start >= 4 && browser_has_prefix_ignore_case(start, "not "))
    {
        negate = true;
        start += 4;
    }
    if (end - start >= 5 && browser_has_prefix_ignore_case(start, "print"))
        return negate;

    bool matches = true;
    bool has_width_constraint = false;
    for (const char* cursor = start; cursor < end; ++cursor)
    {
        bool maximum = false;
        int property_length = 0;
        if (end - cursor >= 9 &&
            browser_has_prefix_ignore_case(cursor, "max-width"))
        {
            maximum = true;
            property_length = 9;
        }
        else if (end - cursor >= 9 &&
                 browser_has_prefix_ignore_case(cursor, "min-width"))
        {
            property_length = 9;
        }
        if (property_length == 0) continue;

        const char* value = cursor + property_length;
        while (value < end && (*value == ' ' || *value == '\t')) ++value;
        if (value >= end || *value++ != ':') continue;
        while (value < end && (*value == ' ' || *value == '\t')) ++value;
        uint32_t amount = 0;
        bool has_digits = false;
        while (value < end && *value >= '0' && *value <= '9')
        {
            has_digits = true;
            amount = amount * 10 + static_cast<uint32_t>(*value++ - '0');
            if (amount > 100000) amount = 100000;
        }
        if (!has_digits) continue;
        while (value < end && (*value == ' ' || *value == '\t')) ++value;

        uint32_t width = static_cast<uint32_t>(browser_viewport_width);
        if (end - value >= 2 && browser_has_prefix_ignore_case(value, "vw"))
        {
            has_width_constraint = true;
            amount = amount * width / 100;
        }
        else if (end - value >= 2 &&
                 browser_has_prefix_ignore_case(value, "em"))
            amount *= 16;
        else if (end - value >= 3 &&
                 browser_has_prefix_ignore_case(value, "rem"))
            amount *= 16;

        if (!(end - value >= 2 && browser_has_prefix_ignore_case(value, "vw")) &&
            amount <= 65535 && browser_media_breakpoint_count <
                               static_cast<int>(sizeof(browser_media_breakpoints) /
                                                sizeof(browser_media_breakpoints[0])))
        {
            bool already_recorded = false;
            for (int i = 0; i < browser_media_breakpoint_count; ++i)
            {
                if (browser_media_breakpoints[i].width == amount &&
                    browser_media_breakpoints[i].maximum == maximum)
                {
                    already_recorded = true;
                    break;
                }
            }
            if (!already_recorded)
            {
                browser_media_breakpoints[browser_media_breakpoint_count++] = {
                    static_cast<uint16_t>(amount), maximum
                };
            }
        }

        has_width_constraint = true;
        if ((maximum && width > amount) || (!maximum && width < amount))
            matches = false;
    }
    if (!has_width_constraint)
        matches = !browser_has_prefix_ignore_case(start, "not all");
    return negate ? !matches : matches;
}

static bool browser_media_query_list_matches(const char* start, const char* end)
{
    const char* query = start;
    while (query < end)
    {
        const char* query_end = query;
        while (query_end < end && *query_end != ',') ++query_end;
        if (browser_media_query_matches(query, query_end)) return true;
        query = query_end < end ? query_end + 1 : end;
    }
    return false;
}

static void browser_parse_css_rules(const char* css, const char* css_end,
                                    uint8_t nesting_depth = 0)
{
    while (css < css_end && browser_css_rule_count < BROWSER_MAX_CSS_RULES)
    {
        if (css + 1 < css_end && css[0] == '/' && css[1] == '*')
        {
            css += 2;
            while (css + 1 < css_end && !(css[0] == '*' && css[1] == '/')) ++css;
            css = css + 1 < css_end ? css + 2 : css_end;
            continue;
        }
        while (css < css_end && (*css == ' ' || *css == '\t' ||
                                 *css == '\r' || *css == '\n' || *css == '}'))
        {
            ++css;
        }
        if (css >= css_end) break;

        if (*css == '@')
        {
            const char* name = ++css;
            while (css < css_end &&
                   ((*css >= 'a' && *css <= 'z') ||
                    (*css >= 'A' && *css <= 'Z') || *css == '-'))
            {
                ++css;
            }
            const int name_length = static_cast<int>(css - name);
            const char* delimiter = css;
            while (delimiter < css_end && *delimiter != ';' && *delimiter != '{')
                ++delimiter;
            if (delimiter >= css_end) break;
            if (*delimiter == ';')
            {
                css = delimiter + 1;
                continue;
            }

            int nesting = 1;
            const char* rule_end = delimiter + 1;
            while (rule_end < css_end && nesting > 0)
            {
                if (*rule_end == '{') ++nesting;
                else if (*rule_end == '}') --nesting;
                ++rule_end;
            }
            const bool conditional_group =
                (name_length == 5 && browser_has_prefix_ignore_case(name, "media")) ||
                (name_length == 8 && browser_has_prefix_ignore_case(name, "supports")) ||
                (name_length == 9 && browser_has_prefix_ignore_case(name, "container")) ||
                (name_length == 5 && browser_has_prefix_ignore_case(name, "layer"));
            const bool group_matches =
                name_length == 5 && browser_has_prefix_ignore_case(name, "media")
                    ? browser_media_query_list_matches(name + name_length, delimiter)
                    : true;
            if (conditional_group && group_matches && nesting == 0 &&
                nesting_depth < 4)
                browser_parse_css_rules(delimiter + 1, rule_end - 1,
                                        nesting_depth + 1);
            css = rule_end;
            continue;
        }

        const char* open = css;
        while (open < css_end && *open != '{' && *open != ';') ++open;
        if (open >= css_end) break;
        if (*open == ';')
        {
            css = open + 1;
            continue;
        }
        const char* close = open + 1;
        int nesting = 1;
        while (close < css_end && nesting > 0)
        {
            if (*close == '{') ++nesting;
            else if (*close == '}') --nesting;
            ++close;
        }
        if (nesting != 0) break;
        const char* selector = css;
        while (selector < open && browser_css_rule_count < BROWSER_MAX_CSS_RULES)
        {
            const char* comma = selector;
            while (comma < open && *comma != ',') ++comma;
            const char* trimmed_start = selector;
            const char* trimmed_end = comma;
            while (trimmed_start < trimmed_end &&
                   (*trimmed_start == ' ' || *trimmed_start == '\t' ||
                    *trimmed_start == '\r' || *trimmed_start == '\n'))
                ++trimmed_start;
            while (trimmed_end > trimmed_start &&
                   (trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t' ||
                    trimmed_end[-1] == '\r' || trimmed_end[-1] == '\n'))
                --trimmed_end;
            if (trimmed_end - trimmed_start == 5 &&
                browser_has_prefix_ignore_case(trimmed_start, ":root"))
            {
                browser_collect_custom_properties(open + 1, close - 1);
            }
            else
            {
                browser_add_css_rule(selector, comma, open + 1, close - 1);
            }
            selector = comma < open ? comma + 1 : open;
        }
        css = close;
    }
}

static void browser_parse_css(const char* source)
{
    browser_css_rule_count = 0;
    browser_stylesheet_count = 0;
    browser_media_breakpoint_count = 0;
    browser_custom_property_count = 0;
    browser_stylesheet_failed = false;
    for (int index = 0; index < BROWSER_MAX_CUSTOM_PROPERTIES; ++index)
        browser_custom_properties[index] = {};
    browser_page_background = BROWSER_BACKGROUND;

    for (int index = 0; source[index] != '\0' &&
                         browser_css_rule_count < BROWSER_MAX_CSS_RULES;)
    {
        if (browser_has_prefix_ignore_case(source + index, "<!--"))
        {
            index += 4;
            while (source[index] != '\0' &&
                   !browser_has_prefix_ignore_case(source + index, "-->"))
            {
                ++index;
            }
            if (source[index] != '\0') index += 3;
            continue;
        }
        if (source[index] != '<')
        {
            ++index;
            continue;
        }

        const bool is_link =
            browser_has_prefix_ignore_case(source + index, "<link") &&
            (source[index + 5] == '>' || source[index + 5] == '/' ||
             source[index + 5] == ' ' || source[index + 5] == '\t' ||
             source[index + 5] == '\r' || source[index + 5] == '\n');
        const bool is_style =
            browser_has_prefix_ignore_case(source + index, "<style") &&
            (source[index + 6] == '>' || source[index + 6] == '/' ||
             source[index + 6] == ' ' || source[index + 6] == '\t' ||
             source[index + 6] == '\r' || source[index + 6] == '\n');
        if (is_link)
        {
            int end = index + 5;
            char quote = '\0';
            while (source[end] != '\0' && end - index < BROWSER_TAG_MAX)
            {
                if (quote != '\0')
                {
                    if (source[end] == quote) quote = '\0';
                }
                else if (source[end] == '"' || source[end] == '\'')
                {
                    quote = source[end];
                }
                else if (source[end] == '>')
                {
                    break;
                }
                ++end;
            }
            if (source[end] != '>' || quote != '\0')
            {
                ++index;
                continue;
            }
            char rel[80] = {};
            char href[256] = {};
            browser_read_decoded_attribute(source, index + 5, end, "rel",
                                           rel, sizeof(rel));
            if (browser_value_has_word(rel, "stylesheet"))
            {
                if (browser_stylesheet_count >= BROWSER_MAX_STYLESHEETS)
                {
                    browser_stylesheet_failed = true;
                }
                else if (browser_read_decoded_attribute(source, index + 5, end,
                                                        "href", href,
                                                        sizeof(href)) <= 0 ||
                         !browser_load_stylesheet(href))
                {
                    browser_stylesheet_failed = true;
                }
                ++browser_stylesheet_count;
            }
            index = end + 1;
            continue;
        }

        if (is_style)
        {
            while (source[index] != '\0' && source[index] != '>') ++index;
            if (source[index] == '\0') break;
            const char* css = source + ++index;
            while (source[index] != '\0' &&
                   !browser_has_prefix_ignore_case(source + index, "</style"))
            {
                ++index;
            }
            browser_parse_css_rules(css, source + index);
            if (source[index] != '\0') index += 7;
            continue;
        }
        ++index;
    }
}

static bool browser_selector_compound_matches(const char* selector, int selector_length,
                                              const char* tag, const char* class_name,
                                              const char* id)
{
    if (selector_length == 1 && selector[0] == '*')
    {
        return true;
    }

    int index = 0;
    if (selector[index] != '.' && selector[index] != '#')
    {
        const int tag_start = index;
        while (index < selector_length && selector[index] != '.' && selector[index] != '#')
            ++index;
        const int tag_length = index - tag_start;
        if (tag_length != browser_length(tag)) return false;
        for (int character = 0; character < tag_length; ++character)
        {
            if (browser_lower(selector[tag_start + character]) != browser_lower(tag[character]))
                return false;
        }
    }

    while (index < selector_length)
    {
        const char marker = selector[index++];
        const int token_start = index;
        while (index < selector_length && selector[index] != '.' && selector[index] != '#')
            ++index;
        const int token_length = index - token_start;
        if (token_length == 0) return false;

        if (marker == '#')
        {
            if (id == nullptr || browser_length(id) != token_length) return false;
            for (int character = 0; character < token_length; ++character)
            {
                if (browser_lower(id[character]) != selector[token_start + character])
                    return false;
            }
        }
        else if (marker == '.')
        {
            if (class_name == nullptr) return false;
            bool found = false;
            const char* class_cursor = class_name;
            while (*class_cursor != '\0')
            {
                while (*class_cursor == ' ' || *class_cursor == '\t') ++class_cursor;
                const char* class_end = class_cursor;
                while (*class_end != '\0' && *class_end != ' ' && *class_end != '\t') ++class_end;
                if (class_end - class_cursor == token_length)
                {
                    int character = 0;
                    while (character < token_length &&
                           browser_lower(class_cursor[character]) ==
                               selector[token_start + character])
                    {
                        ++character;
                    }
                    if (character == token_length)
                    {
                        found = true;
                        break;
                    }
                }
                class_cursor = class_end;
            }
            if (!found) return false;
        }
        else
        {
            return false;
        }
    }
    return true;
}

static bool browser_selector_matches(const browser_css_rule& rule, const char* tag,
                                     const char* class_name, const char* id,
                                     const browser_element_context* ancestors,
                                     int ancestor_count)
{
    const char* segments[8] = {};
    int segment_lengths[8] = {};
    char relations[7] = {};
    int segment_count = 0;
    int index = 0;
    const int selector_length = browser_length(rule.selector);
    while (index < selector_length && segment_count < 8)
    {
        while (index < selector_length &&
               (rule.selector[index] == ' ' || rule.selector[index] == '\t'))
        {
            ++index;
        }
        if (index >= selector_length) break;

        const int start = index;
        while (index < selector_length && rule.selector[index] != '>' &&
               rule.selector[index] != ' ' && rule.selector[index] != '\t')
        {
            ++index;
        }
        if (index == start) return false;
        segments[segment_count] = rule.selector + start;
        segment_lengths[segment_count] = index - start;
        ++segment_count;

        bool had_space = false;
        while (index < selector_length &&
               (rule.selector[index] == ' ' || rule.selector[index] == '\t'))
        {
            had_space = true;
            ++index;
        }
        if (index < selector_length)
        {
            if (rule.selector[index] == '>')
            {
                relations[segment_count - 1] = '>';
                ++index;
            }
            else if (had_space)
            {
                relations[segment_count - 1] = ' ';
            }
            else
            {
                return false;
            }
        }
    }
    if (segment_count == 0 || index < selector_length) return false;
    if (!browser_selector_compound_matches(segments[segment_count - 1],
                                          segment_lengths[segment_count - 1],
                                          tag, class_name, id))
    {
        return false;
    }

    int ancestor_index = ancestor_count - 1;
    for (int segment = segment_count - 2; segment >= 0; --segment)
    {
        if (relations[segment] == '>')
        {
            if (ancestor_index < 0 ||
                !browser_selector_compound_matches(
                    segments[segment], segment_lengths[segment],
                    ancestors[ancestor_index].tag,
                    ancestors[ancestor_index].class_name,
                    ancestors[ancestor_index].id))
            {
                return false;
            }
            --ancestor_index;
            continue;
        }

        bool found = false;
        while (ancestor_index >= 0)
        {
            if (browser_selector_compound_matches(
                    segments[segment], segment_lengths[segment],
                    ancestors[ancestor_index].tag,
                    ancestors[ancestor_index].class_name,
                    ancestors[ancestor_index].id))
            {
                found = true;
                --ancestor_index;
                break;
            }
            --ancestor_index;
        }
        if (!found) return false;
    }
    return true;
}

static void browser_apply_css(browser_style& style, const char* tag,
                              const char* class_name, const char* id,
                              const browser_element_context* ancestors,
                              int ancestor_count)
{
    for (int index = 0; index < browser_css_rule_count; ++index)
    {
        const browser_css_rule& rule = browser_css_rules[index];
        if (!browser_selector_matches(rule, tag, class_name, id,
                                      ancestors, ancestor_count)) continue;
        const uint16_t specificity = browser_selector_specificity(rule.selector);
        if ((rule.declarations & 1) != 0 && specificity >= style.color_specificity)
        {
            style.color = rule.color;
            style.color_specificity = specificity;
        }
        if ((rule.declarations & 2) != 0 && specificity >= style.background_specificity)
        {
            style.background = rule.background;
            style.background_specificity = specificity;
        }
        if ((rule.declarations & 4) != 0 &&
            specificity >= style.font_weight_specificity)
        {
            style.flags = static_cast<uint8_t>(
                (style.flags & ~BROWSER_BOLD) | (rule.flags & BROWSER_BOLD));
            style.font_weight_specificity = specificity;
        }
        if ((rule.declarations & 64) != 0 &&
            specificity >= style.text_decoration_specificity)
        {
            const uint8_t mask =
                BROWSER_UNDERLINE | BROWSER_STRIKETHROUGH | BROWSER_OVERLINE;
            style.flags = static_cast<uint8_t>(
                (style.flags & ~mask) | (rule.flags & mask));
            style.text_decoration_specificity = specificity;
        }
        if ((rule.declarations & 128) != 0 &&
            specificity >= style.text_transform_specificity)
        {
            const uint8_t mask =
                BROWSER_UPPERCASE | BROWSER_LOWERCASE | BROWSER_CAPITALIZE;
            style.flags = static_cast<uint8_t>(
                (style.flags & ~mask) | (rule.flags & mask));
            style.text_transform_specificity = specificity;
        }
        if ((rule.declarations & 8) != 0 && specificity >= style.alignment_specificity)
        {
            style.alignment = rule.alignment;
            style.alignment_specificity = specificity;
        }
        if ((rule.declarations & 16) != 0 && specificity >= style.hidden_specificity)
        {
            style.hidden = rule.hidden;
            style.hidden_specificity = specificity;
        }
        if ((rule.declarations & 32) != 0 &&
            specificity >= style.white_space_specificity)
        {
            style.preformatted = rule.preformatted;
            style.white_space_specificity = specificity;
        }
        if ((rule.declarations & 256) != 0 &&
            specificity >= style.border_specificity)
        {
            style.border_enabled = rule.border_enabled;
            style.border_color = rule.border_color;
            style.border_specificity = specificity;
        }
        if ((rule.declarations & 1024) != 0 &&
            specificity >= style.space_before_specificity)
        {
            style.space_before = rule.space_before;
            style.space_before_specificity = specificity;
        }
        if ((rule.declarations & 2048) != 0 &&
            specificity >= style.space_after_specificity)
        {
            style.space_after = rule.space_after;
            style.space_after_specificity = specificity;
        }
        if ((rule.declarations & 4096) != 0 &&
            specificity >= style.min_height_specificity)
        {
            style.min_height_lines = rule.min_height_lines;
            style.min_height_specificity = specificity;
        }
        if ((rule.declarations & 8192) != 0 &&
            specificity >= style.width_specificity)
        {
            style.width_pixels = rule.width_pixels;
            style.width_cap_pixels = rule.width_cap_pixels;
            style.width_percent = rule.width_percent;
            style.width_set = rule.width_set;
            style.width_specificity = specificity;
        }
        if ((rule.declarations & 16384) != 0 &&
            specificity >= style.max_width_specificity)
        {
            style.max_width_pixels = rule.max_width_pixels;
            style.max_width_specificity = specificity;
        }
        if ((rule.declarations & 32768) != 0 &&
            specificity >= style.padding_left_specificity)
        {
            style.padding_left_chars = rule.padding_left_chars;
            style.padding_left_specificity = specificity;
        }
        if ((rule.declarations & 65536) != 0 &&
            specificity >= style.padding_right_specificity)
        {
            style.padding_right_chars = rule.padding_right_chars;
            style.padding_right_specificity = specificity;
        }
        if ((rule.declarations & 131072) != 0 &&
            specificity >= style.margin_left_specificity)
        {
            style.margin_left_auto = rule.margin_left_auto;
            style.margin_left_specificity = specificity;
        }
        if ((rule.declarations & 262144) != 0 &&
            specificity >= style.margin_right_specificity)
        {
            style.margin_right_auto = rule.margin_right_auto;
            style.margin_right_specificity = specificity;
        }
        if ((rule.declarations & 524288) != 0 &&
            specificity >= style.display_specificity)
        {
            style.layout_mode = rule.layout_mode;
            style.display_specificity = specificity;
        }
        if ((rule.declarations & 1048576) != 0 &&
            specificity >= style.grid_columns_specificity)
        {
            style.grid_columns = rule.grid_columns;
            style.grid_columns_specificity = specificity;
        }
        if ((rule.declarations & 2097152) != 0 &&
            specificity >= style.grid_gap_specificity)
        {
            style.grid_gap_chars = rule.grid_gap_chars;
            style.grid_gap_specificity = specificity;
        }
        if ((rule.declarations & 4194304) != 0 &&
            specificity >= style.align_items_specificity)
        {
            style.align_items_center = rule.align_items_center;
            style.align_items_specificity = specificity;
        }
    }
}

static int browser_read_attribute(const char* source, int start, int end,
                                  const char* name, char* output, int capacity)
{
    int index = start;
    while (index < end)
    {
        while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                               source[index] == '\r' || source[index] == '\n' ||
                               source[index] == '/'))
        {
            ++index;
        }
        int key_start = index;
        while (index < end && source[index] != '=' && source[index] != ' ' &&
               source[index] != '\t' && source[index] != '\r' &&
               source[index] != '\n')
        {
            ++index;
        }
        int key_end = index;
        while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                               source[index] == '\r' || source[index] == '\n')) ++index;
        if (index >= end || source[index] != '=')
        {
            while (index < end && source[index] != ' ' && source[index] != '\t' &&
                   source[index] != '\r' && source[index] != '\n') ++index;
            continue;
        }
        ++index;
        while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                               source[index] == '\r' || source[index] == '\n')) ++index;
        char quote = '\0';
        if (index < end && (source[index] == '"' || source[index] == '\''))
        {
            quote = source[index++];
        }
        int value_start = index;
        if (quote != '\0')
        {
            while (index < end && source[index] != quote) ++index;
        }
        else
        {
            while (index < end && source[index] != ' ' && source[index] != '\t' &&
                   source[index] != '\r' && source[index] != '\n' &&
                   source[index] != '>') ++index;
        }
        int value_end = index;
        if (quote != '\0' && index < end) ++index;

        int key_length = key_end - key_start;
        if (key_length == browser_length(name))
        {
            int character = 0;
            while (character < key_length && browser_lower(source[key_start + character]) == browser_lower(name[character])) ++character;
            if (character == key_length)
            {
                int length = value_end - value_start;
                if (length >= capacity) length = capacity - 1;
                for (int copy = 0; copy < length; ++copy) output[copy] = source[value_start + copy];
                output[length] = '\0';
                return length;
            }
        }
    }
    if (capacity > 0) output[0] = '\0';
    return 0;
}

static int browser_read_decoded_attribute(const char* source, int start, int end,
                                          const char* name, char* output,
                                          int capacity)
{
    if (capacity <= 0) return -1;
    char raw[512] = {};
    const int raw_length = browser_read_attribute(source, start, end, name,
                                                  raw, sizeof(raw));
    if (raw_length >= static_cast<int>(sizeof(raw)) - 1)
    {
        if (capacity > 0) output[0] = '\0';
        return -1;
    }

    int source_index = 0;
    int output_length = 0;
    while (source_index < raw_length)
    {
        char decoded = raw[source_index];
        if (raw[source_index] == '&' &&
            browser_decode_entity(raw, source_index, decoded))
        {
            if (output_length >= capacity - 1)
            {
                output[0] = '\0';
                return -1;
            }
            output[output_length++] = decoded;
            continue;
        }
        if (output_length >= capacity - 1)
        {
            output[0] = '\0';
            return -1;
        }
        output[output_length++] = raw[source_index++];
    }
    output[output_length] = '\0';
    return output_length;
}

static bool browser_has_attribute(const char* source, int start, int end,
                                  const char* name)
{
    int index = start;
    while (index < end)
    {
        while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                               source[index] == '\r' || source[index] == '\n' ||
                               source[index] == '/'))
        {
            ++index;
        }
        const int key_start = index;
        while (index < end && source[index] != '=' && source[index] != ' ' &&
               source[index] != '\t' && source[index] != '\r' &&
               source[index] != '\n' && source[index] != '/')
        {
            ++index;
        }
        const int key_length = index - key_start;
        if (key_length == browser_length(name))
        {
            int character = 0;
            while (character < key_length &&
                   browser_lower(source[key_start + character]) == browser_lower(name[character]))
            {
                ++character;
            }
            if (character == key_length) return true;
        }

        while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                               source[index] == '\r' || source[index] == '\n'))
        {
            ++index;
        }
        if (index < end && source[index] == '=')
        {
            ++index;
            while (index < end && (source[index] == ' ' || source[index] == '\t' ||
                                   source[index] == '\r' || source[index] == '\n')) ++index;
            if (index < end && (source[index] == '"' || source[index] == '\''))
            {
                const char quote = source[index++];
                while (index < end && source[index] != quote) ++index;
                if (index < end) ++index;
            }
            else
            {
                while (index < end && source[index] != ' ' && source[index] != '\t' &&
                       source[index] != '\r' && source[index] != '\n' &&
                       source[index] != '>' && source[index] != '/') ++index;
            }
        }
    }
    return false;
}

static bool browser_copy_text(char* destination, int capacity, const char* source);

static bool browser_decode_utf8(const char* source, int& index, char& output)
{
    const uint8_t first = static_cast<uint8_t>(source[index]);
    if (first < 0x80) return false;

    const int sequence_length = first >= 0xF0 && first <= 0xF4 ? 4 :
                                first >= 0xE0 && first <= 0xEF ? 3 :
                                first >= 0xC2 && first <= 0xDF ? 2 : 1;
    if (sequence_length == 1)
    {
        output = '?';
        ++index;
        return true;
    }
    uint32_t codepoint = first & (sequence_length == 2 ? 0x1F :
                                  sequence_length == 3 ? 0x0F : 0x07);
    for (int offset = 1; offset < sequence_length; ++offset)
    {
        const uint8_t continuation = static_cast<uint8_t>(source[index + offset]);
        if (continuation == 0 || (continuation & 0xC0) != 0x80)
        {
            output = '?';
            ++index;
            return true;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
    if ((sequence_length == 2 && codepoint < 0x80) ||
        (sequence_length == 3 && codepoint < 0x800) ||
        (sequence_length == 4 && codepoint < 0x10000) ||
        codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF))
    {
        output = '?';
        ++index;
        return true;
    }

    output = browser_unicode_to_ascii(codepoint);
    index += sequence_length;
    return true;
}

static void browser_parse_html(const char* source)
{
    browser_parse_css(source);
    const int source_length = browser_length(source);
    browser_link_count = 0;
    browser_fragment_count = 0;
    browser_box_count = 0;
    browser_box_truncated = false;
    browser_dom_node_count = 1;
    browser_dom_truncated = false;
    browser_dom_nodes[0] = {};
    browser_dom_nodes[0].kind = BROWSER_DOM_DOCUMENT;
    browser_dom_nodes[0].parent = -1;
    browser_dom_nodes[0].first_child = -1;
    browser_dom_nodes[0].last_child = -1;
    browser_dom_nodes[0].next_sibling = -1;
    browser_dom_nodes[0].source_end = static_cast<uint32_t>(source_length);
    browser_dom_nodes[0].tag[0] = '#';
    browser_dom_nodes[0].tag[1] = 'd';
    browser_dom_nodes[0].tag[2] = 'o';
    browser_dom_nodes[0].tag[3] = 'c';
    browser_dom_nodes[0].tag[4] = 'u';
    browser_dom_nodes[0].tag[5] = 'm';
    browser_dom_nodes[0].tag[6] = 'e';
    browser_dom_nodes[0].tag[7] = 'n';
    browser_dom_nodes[0].tag[8] = 't';
    browser_document[0] = '\0';
    browser_document_length = 0;
    browser_dom_nodes[0].document_end = 0;
    bool pending_space = false;
    bool skip_head = false;
    bool skip_script = false;
    bool skip_style = false;
    int template_depth = 0;
    bool truncated = browser_source_truncated;
    browser_style style_stack[BROWSER_MAX_STYLE_DEPTH] = {};
    browser_element_context element_stack[BROWSER_MAX_STYLE_DEPTH] = {};
    char list_types[BROWSER_MAX_STYLE_DEPTH] = {};
    uint32_t list_items[BROWSER_MAX_STYLE_DEPTH] = {};
    int stack_depth = 0;
    int list_depth = 0;
    int table_cell_count = 0;
    browser_style current_style = {};
    current_style.color = BROWSER_TEXT;
    current_style.background = BROWSER_NO_BACKGROUND;
    current_style.grid_columns = 1;

    for (int index = 0; source[index] != '\0';)
    {
        if (skip_script || skip_style)
        {
            const char* closing_tag = skip_script ? "</script" : "</style";
            const int closing_tag_length = skip_script ? 8 : 7;
            if (source[index] != '<' ||
                !browser_has_prefix_ignore_case(source + index, closing_tag) ||
                (source[index + closing_tag_length] != '>' &&
                 source[index + closing_tag_length] != ' ' &&
                 source[index + closing_tag_length] != '\t' &&
                 source[index + closing_tag_length] != '\r' &&
                 source[index + closing_tag_length] != '\n'))
            {
                ++index;
                continue;
            }
        }
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
            char attribute_quote = '\0';
            while (source[end] != '\0' && end - index < BROWSER_TAG_MAX)
            {
                if (attribute_quote != '\0')
                {
                    if (source[end] == attribute_quote) attribute_quote = '\0';
                }
                else if (source[end] == '"' || source[end] == '\'')
                {
                    attribute_quote = source[end];
                }
                else if (source[end] == '>')
                {
                    break;
                }
                ++end;
            }
            if (source[end] != '>' || attribute_quote != '\0')
            {
                if (!(skip_head || skip_script || skip_style))
                {
                    const int text_parent = stack_depth > 0
                        ? element_stack[stack_depth - 1].dom_node : 0;
                    browser_append_dom_text(text_parent, index, index + 1);
                    if (!browser_append_document('<', pending_space, current_style))
                    {
                        truncated = true;
                        break;
                    }
                }
                ++index;
                continue;
            }

            char tag[32] = {};
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
                      (character >= '0' && character <= '9') ||
                      character == '-' || character == ':' || character == '_'))
                {
                    break;
                }
                tag[tag_length++] = character;
                ++tag_index;
            }
            const bool closing = source[index + 1] == '/';
            char class_name[64] = {};
            char id[64] = {};
            char inline_style[160] = {};
            char href[256] = {};
            char value[80] = {};
            char placeholder[80] = {};
            char input_type[24] = {};
            browser_read_attribute(source, tag_index, end, "class", class_name, sizeof(class_name));
            browser_read_decoded_attribute(source, tag_index, end, "id",
                                           id, sizeof(id));
            browser_read_attribute(source, tag_index, end, "style", inline_style, sizeof(inline_style));
            browser_read_decoded_attribute(source, tag_index, end, "href",
                                           href, sizeof(href));
            browser_read_decoded_attribute(source, tag_index, end, "value",
                                           value, sizeof(value));
            browser_read_decoded_attribute(source, tag_index, end, "placeholder",
                                           placeholder, sizeof(placeholder));
            browser_read_attribute(source, tag_index, end, "type", input_type, sizeof(input_type));

            if (!(skip_head || skip_script || skip_style) &&
                browser_equal_ignore_case(tag, "template"))
            {
                if (closing)
                {
                    if (template_depth > 0) --template_depth;
                }
                else if (template_depth < BROWSER_MAX_STYLE_DEPTH)
                {
                    ++template_depth;
                }
                else
                {
                    truncated = true;
                    break;
                }
            }
            else if (template_depth == 0 &&
                     browser_equal_ignore_case(tag, "head"))
            {
                skip_head = !closing;
            }
            else if (template_depth == 0 &&
                     browser_equal_ignore_case(tag, "script"))
            {
                skip_script = !closing;
            }
            else if (template_depth == 0 &&
                     browser_equal_ignore_case(tag, "style"))
            {
                skip_style = !closing;
            }
            else if (template_depth == 0 &&
                     !(skip_head || skip_script || skip_style))
            {
                if (closing)
                {
                    if (browser_tag_is_block(tag) &&
                        !browser_append_document('\n', pending_space, current_style))
                    {
                        truncated = true;
                        break;
                    }
                    if (browser_tag_is_block(tag) &&
                        !browser_append_blank_lines(current_style.space_after,
                                                    current_style))
                    {
                        truncated = true;
                        break;
                    }
                    int matching = stack_depth - 1;
                    while (matching >= 0 &&
                           !browser_equal_ignore_case(element_stack[matching].tag, tag))
                    {
                        --matching;
                    }
                    if (matching >= 0 && browser_tag_is_block(tag))
                    {
                        const browser_element_context& closing_element =
                            element_stack[matching];
                        uint8_t trailing_min_height_lines =
                            current_style.min_height_lines;
                        if (closing_element.centered_min_height)
                            trailing_min_height_lines = 0;
                        if (!browser_append_blank_lines(
                                trailing_min_height_lines, current_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    const int closing_content_end = browser_document_length;
                    if (browser_equal_ignore_case(tag, "q") &&
                        !browser_append_document('"', pending_space, current_style))
                    {
                        truncated = true;
                        break;
                    }
                    if (matching >= 0)
                    {
                        for (int open = stack_depth - 1; open >= matching; --open)
                        {
                            browser_close_box(element_stack[open].box_index,
                                              closing_content_end);
                            browser_close_dom_node(element_stack[open].dom_node,
                                                   end + 1,
                                                   closing_content_end);
                        }
                        current_style = style_stack[matching];
                        stack_depth = matching;
                    }
                    if ((browser_equal_ignore_case(tag, "ul") ||
                         browser_equal_ignore_case(tag, "ol")) &&
                        list_depth > 0)
                    {
                        int matching_list = list_depth - 1;
                        const char expected = browser_equal_ignore_case(tag, "ol") ? 'o' : 'u';
                        while (matching_list >= 0 && list_types[matching_list] != expected)
                            --matching_list;
                        if (matching_list >= 0) list_depth = matching_list;
                    }
                }
                else
                {
                    const bool block = browser_tag_is_block(tag);
                    const auto close_open_tag = [&](const char* target)
                    {
                        int matching = stack_depth - 1;
                        while (matching >= 0 &&
                               !browser_equal_ignore_case(element_stack[matching].tag,
                                                          target))
                        {
                            --matching;
                        }
                        if (matching >= 0)
                        {
                            for (int open = stack_depth - 1; open >= matching; --open)
                            {
                                browser_close_box(element_stack[open].box_index,
                                                  browser_document_length);
                                browser_close_dom_node(element_stack[open].dom_node,
                                                       index,
                                                       browser_document_length);
                            }
                            current_style = style_stack[matching];
                            stack_depth = matching;
                        }
                    };
                    if (block)
                        close_open_tag("p");
                    if (browser_equal_ignore_case(tag, "p"))
                        close_open_tag("p");
                    if (browser_equal_ignore_case(tag, "li"))
                    {
                        for (int open = stack_depth - 1; open >= 0; --open)
                        {
                            if (browser_equal_ignore_case(element_stack[open].tag, "li"))
                            {
                                for (int close = stack_depth - 1; close >= open; --close)
                                {
                                    browser_close_box(element_stack[close].box_index,
                                                      browser_document_length);
                                    browser_close_dom_node(element_stack[close].dom_node,
                                                           index,
                                                           browser_document_length);
                                }
                                current_style = style_stack[open];
                                stack_depth = open;
                                break;
                            }
                            if (browser_equal_ignore_case(element_stack[open].tag, "ul") ||
                                browser_equal_ignore_case(element_stack[open].tag, "ol"))
                                break;
                        }
                    }
                    if (browser_equal_ignore_case(tag, "dt") ||
                        browser_equal_ignore_case(tag, "dd"))
                    {
                        close_open_tag("dt");
                        close_open_tag("dd");
                    }
                    if (browser_equal_ignore_case(tag, "td") ||
                        browser_equal_ignore_case(tag, "th"))
                    {
                        close_open_tag("td");
                        close_open_tag("th");
                    }
                    if (browser_equal_ignore_case(tag, "tr"))
                    {
                        close_open_tag("td");
                        close_open_tag("th");
                        close_open_tag("tr");
                    }
                    if (browser_equal_ignore_case(tag, "option"))
                        close_open_tag("option");
                    if (browser_equal_ignore_case(tag, "a"))
                        close_open_tag("a");
                    if (browser_equal_ignore_case(tag, "button"))
                        close_open_tag("button");
                    if (browser_equal_ignore_case(tag, "h1") ||
                        browser_equal_ignore_case(tag, "h2") ||
                        browser_equal_ignore_case(tag, "h3") ||
                        browser_equal_ignore_case(tag, "h4") ||
                        browser_equal_ignore_case(tag, "h5") ||
                        browser_equal_ignore_case(tag, "h6"))
                    {
                        close_open_tag("h1");
                        close_open_tag("h2");
                        close_open_tag("h3");
                        close_open_tag("h4");
                        close_open_tag("h5");
                        close_open_tag("h6");
                    }
                    if (block && !browser_append_document('\n', pending_space, current_style))
                    {
                        truncated = true;
                        break;
                    }
                    const int parent_dom_node = stack_depth > 0
                        ? element_stack[stack_depth - 1].dom_node : 0;
                    const int element_dom_index = browser_create_dom_node(
                        parent_dom_node, BROWSER_DOM_ELEMENT, tag, class_name, id,
                        index, 0);

                    browser_style element_style = current_style;
                    element_style.color_specificity = 0;
                    element_style.background_specificity = 0;
                    element_style.border_specificity = 0;
                    element_style.space_before_specificity = 0;
                    element_style.space_after_specificity = 0;
                    element_style.min_height_specificity = 0;
                    element_style.font_weight_specificity = 0;
                    element_style.text_decoration_specificity = 0;
                    element_style.text_transform_specificity = 0;
                    element_style.alignment_specificity = 0;
                    element_style.align_items_specificity = 0;
                    element_style.hidden_specificity = 0;
                    element_style.white_space_specificity = 0;
                    element_style.width_specificity = 0;
                    element_style.max_width_specificity = 0;
                    element_style.padding_left_specificity = 0;
                    element_style.padding_right_specificity = 0;
                    element_style.margin_left_specificity = 0;
                    element_style.margin_right_specificity = 0;
                    element_style.display_specificity = 0;
                    element_style.grid_columns_specificity = 0;
                    element_style.grid_gap_specificity = 0;
                    element_style.space_before = 0;
                    element_style.space_after = 0;
                    element_style.min_height_lines = 0;
                    element_style.width_pixels = 0;
                    element_style.width_cap_pixels = 0;
                    element_style.max_width_pixels = 0;
                    element_style.width_percent = 0;
                    element_style.padding_left_chars = 0;
                    element_style.padding_right_chars = 0;
                    element_style.layout_mode = BROWSER_LAYOUT_NORMAL;
                    element_style.align_items_center = false;
                    element_style.grid_columns = 1;
                    element_style.grid_gap_chars = 0;
                    element_style.width_set = false;
                    element_style.margin_left_auto = false;
                    element_style.margin_right_auto = false;
                    element_style.border_enabled = false;
                    element_style.border_color = element_style.color;
                    element_style.preformatted = browser_equal_ignore_case(tag, "pre");

                    if (browser_equal_ignore_case(tag, "b") ||
                        browser_equal_ignore_case(tag, "strong") ||
                        browser_equal_ignore_case(tag, "th") ||
                        browser_equal_ignore_case(tag, "h1") ||
                        browser_equal_ignore_case(tag, "h2") ||
                        browser_equal_ignore_case(tag, "h3") ||
                        browser_equal_ignore_case(tag, "h4") ||
                        browser_equal_ignore_case(tag, "h5") ||
                        browser_equal_ignore_case(tag, "h6"))
                    {
                        element_style.flags |= BROWSER_BOLD;
                    }
                    if (browser_equal_ignore_case(tag, "del") ||
                        browser_equal_ignore_case(tag, "s") ||
                        browser_equal_ignore_case(tag, "strike"))
                    {
                        element_style.flags |= BROWSER_STRIKETHROUGH;
                    }
                    if (browser_equal_ignore_case(tag, "ins") ||
                        browser_equal_ignore_case(tag, "u"))
                    {
                        element_style.flags |= BROWSER_UNDERLINE;
                    }
                    if (browser_equal_ignore_case(tag, "mark"))
                    {
                        element_style.background = 0xFF665522;
                    }
                    if (browser_equal_ignore_case(tag, "small"))
                    {
                        element_style.color = 0xFFB9C3D0;
                    }
                    if (browser_equal_ignore_case(tag, "blockquote"))
                    {
                        element_style.color = 0xFFB9C3D0;
                    }
                    if (browser_equal_ignore_case(tag, "dt"))
                    {
                        element_style.flags |= BROWSER_BOLD;
                    }
                    if (browser_equal_ignore_case(tag, "a"))
                    {
                        element_style.color = current_style.color;
                        element_style.flags |= BROWSER_UNDERLINE;
                        if (href[0] != '\0' && browser_link_count < BROWSER_MAX_LINKS)
                        {
                            browser_link& link = browser_links[browser_link_count];
                            int href_length = browser_length(href);
                            if (href_length >= static_cast<int>(sizeof(link.href)))
                            {
                                href_length = sizeof(link.href) - 1;
                            }
                            for (int character = 0; character < href_length; ++character)
                            {
                                link.href[character] = href[character];
                            }
                            link.href[href_length] = '\0';
                            element_style.link_id = static_cast<uint8_t>(browser_link_count + 1);
                            ++browser_link_count;
                        }
                    }
                    if (browser_equal_ignore_case(tag, "code") ||
                        browser_equal_ignore_case(tag, "pre"))
                    {
                        element_style.background = 0xFF283240;
                    }
                    if (browser_equal_ignore_case(tag, "h1"))
                    {
                        element_style.color = 0xFF9DCBFF;
                    }
                    else if (browser_equal_ignore_case(tag, "h2"))
                    {
                        element_style.color = 0xFFB6D8F2;
                    }
                    else if (browser_equal_ignore_case(tag, "h3") ||
                             browser_equal_ignore_case(tag, "h4") ||
                             browser_equal_ignore_case(tag, "h5") ||
                             browser_equal_ignore_case(tag, "h6"))
                    {
                        element_style.color = 0xFFC6DDF1;
                    }

                    browser_apply_css(element_style, tag, class_name, id,
                                      element_stack, stack_depth);
                    if (inline_style[0] != '\0')
                    {
                        browser_apply_declarations(inline_style, inline_style + browser_length(inline_style),
                                                   element_style, 1000);
                    }
                    if (browser_equal_ignore_case(tag, "body") &&
                        element_style.background != BROWSER_NO_BACKGROUND)
                        browser_page_background = element_style.background;
                    if (block && element_style.space_before > 0 &&
                        !browser_append_blank_lines(element_style.space_before,
                                                    element_style))
                    {
                        truncated = true;
                        break;
                    }
                    const bool centered_min_height =
                        block &&
                        element_style.layout_mode == BROWSER_LAYOUT_FLEX &&
                        element_style.align_items_center &&
                        element_style.min_height_lines > 0;
                    const uint8_t min_height_leading_lines =
                        centered_min_height
                            ? element_style.min_height_lines / 10 : 0;
                    const int element_start_offset = browser_document_length;
                    if (element_dom_index >= 0)
                    {
                        browser_dom_node& node =
                            browser_dom_nodes[element_dom_index];
                        node.document_start =
                            static_cast<uint32_t>(element_start_offset);
                        node.layout_mode = element_style.layout_mode;
                        node.grid_columns = element_style.grid_columns;
                        node.grid_gap_chars = element_style.grid_gap_chars;
                        node.grid_row_gap_lines = element_style.space_after;
                        node.hidden = element_style.hidden;
                    }
                    int element_box_index = -1;
                    if (block && !browser_equal_ignore_case(tag, "body"))
                    {
                        element_box_index = browser_create_box(
                            element_start_offset, element_style);
                        if (element_box_index >= 0)
                        {
                            browser_boxes[element_box_index].dom_node =
                                element_dom_index;
                            element_style.border_enabled = false;
                        }
                    }
                    if (min_height_leading_lines > 0 &&
                        !browser_append_blank_lines(min_height_leading_lines,
                                                    element_style))
                    {
                        browser_close_box(element_box_index,
                                          browser_document_length);
                        browser_close_dom_node(element_dom_index, end + 1,
                                               browser_document_length);
                        truncated = true;
                        break;
                    }
                    if (id[0] != '\0' && browser_fragment_count < BROWSER_MAX_LINKS)
                    {
                        browser_fragment& fragment =
                            browser_fragments[browser_fragment_count++];
                        browser_copy_text(fragment.id, sizeof(fragment.id), id);
                        fragment.document_offset = browser_document_length;
                    }

                    if (browser_equal_ignore_case(tag, "li"))
                    {
                        if (list_depth > 0 && list_types[list_depth - 1] == 'o')
                        {
                            uint32_t number = ++list_items[list_depth - 1];
                            char digits[12] = {};
                            int digit_count = 0;
                            do
                            {
                                digits[digit_count++] = static_cast<char>('0' + number % 10);
                                number /= 10;
                            } while (number != 0 && digit_count < 10);
                            while (digit_count > 0)
                            {
                                if (!browser_append_document(digits[--digit_count], pending_space,
                                                             element_style))
                                {
                                    truncated = true;
                                    break;
                                }
                            }
                            if (!browser_append_document('.', pending_space, element_style) ||
                                !browser_append_document(' ', pending_space, element_style))
                            {
                                truncated = true;
                                break;
                            }
                        }
                        else if (!browser_append_document('-', pending_space, element_style) ||
                                 !browser_append_document(' ', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "blockquote"))
                    {
                        if (!browser_append_document('>', pending_space, element_style) ||
                            !browser_append_document(' ', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "dd"))
                    {
                        if (!browser_append_document(' ', pending_space, element_style) ||
                            !browser_append_document(' ', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "td") ||
                             browser_equal_ignore_case(tag, "th"))
                    {
                        if (table_cell_count > 0 &&
                            (!browser_append_document(' ', pending_space, element_style) ||
                             !browser_append_document('|', pending_space, element_style) ||
                             !browser_append_document(' ', pending_space, element_style)))
                        {
                            truncated = true;
                            break;
                        }
                        ++table_cell_count;
                    }
                    else if (browser_equal_ignore_case(tag, "q"))
                    {
                        if (!browser_append_document('"', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "hr"))
                    {
                        if (!browser_append_document('-', pending_space, element_style) ||
                            !browser_append_document('-', pending_space, element_style) ||
                            !browser_append_document('-', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "img"))
                    {
                        char alt[80] = {};
                        browser_read_decoded_attribute(source, tag_index, end, "alt",
                                                       alt, sizeof(alt));
                        for (int alt_index = 0; alt[alt_index] != '\0'; ++alt_index)
                        {
                            if (!browser_append_document(alt[alt_index], pending_space, element_style))
                            {
                                truncated = true;
                                break;
                            }
                        }
                    }
                    else if (browser_equal_ignore_case(tag, "input"))
                    {
                        if (browser_equal_ignore_case(input_type, "checkbox") ||
                            browser_equal_ignore_case(input_type, "radio"))
                        {
                            const bool checked = browser_has_attribute(source, tag_index, end,
                                                                      "checked");
                            const char* marker = checked
                                ? (browser_equal_ignore_case(input_type, "radio") ? "(*)" : "[x]")
                                : (browser_equal_ignore_case(input_type, "radio") ? "( )" : "[ ]");
                            for (int marker_index = 0; marker[marker_index] != '\0'; ++marker_index)
                            {
                                if (!browser_append_document(marker[marker_index], pending_space,
                                                             element_style))
                                {
                                    truncated = true;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            const char* shown_value = value[0] != '\0' ? value : placeholder;
                            for (int value_index = 0; shown_value[value_index] != '\0'; ++value_index)
                            {
                                if (!browser_append_document(shown_value[value_index],
                                                             pending_space, element_style))
                                {
                                    truncated = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (browser_equal_ignore_case(tag, "ol") ||
                        browser_equal_ignore_case(tag, "ul"))
                    {
                        if (list_depth < BROWSER_MAX_STYLE_DEPTH)
                        {
                            list_types[list_depth] =
                                browser_equal_ignore_case(tag, "ol") ? 'o' : 'u';
                            list_items[list_depth] = 0;
                            ++list_depth;
                        }
                    }

                    if ((block &&
                         !browser_equal_ignore_case(tag, "li") &&
                         !browser_equal_ignore_case(tag, "blockquote") &&
                         !browser_equal_ignore_case(tag, "dd")) ||
                        browser_equal_ignore_case(tag, "br") ||
                        browser_equal_ignore_case(tag, "hr"))
                    {
                        if (!browser_append_document('\n', pending_space, element_style))
                        {
                            truncated = true;
                            break;
                        }
                    }
                    if (browser_equal_ignore_case(tag, "tr"))
                        table_cell_count = 0;

                    const bool self_closing = end > index && source[end - 1] == '/';
                    const bool void_element = browser_equal_ignore_case(tag, "br") ||
                        browser_equal_ignore_case(tag, "hr") ||
                        browser_equal_ignore_case(tag, "img") ||
                        browser_equal_ignore_case(tag, "meta") ||
                        browser_equal_ignore_case(tag, "link") ||
                        browser_equal_ignore_case(tag, "input") ||
                        browser_equal_ignore_case(tag, "area") ||
                        browser_equal_ignore_case(tag, "base") ||
                        browser_equal_ignore_case(tag, "col") ||
                        browser_equal_ignore_case(tag, "embed") ||
                        browser_equal_ignore_case(tag, "param") ||
                        browser_equal_ignore_case(tag, "source") ||
                        browser_equal_ignore_case(tag, "track") ||
                        browser_equal_ignore_case(tag, "wbr");
                    if (!self_closing && !void_element &&
                        stack_depth >= BROWSER_MAX_STYLE_DEPTH)
                    {
                        browser_close_box(element_box_index, browser_document_length);
                        browser_close_dom_node(element_dom_index, end + 1,
                                               browser_document_length);
                        truncated = true;
                        break;
                    }
                    if (!self_closing && !void_element)
                    {
                        browser_copy_text(element_stack[stack_depth].tag,
                                          sizeof(element_stack[stack_depth].tag), tag);
                        browser_copy_text(element_stack[stack_depth].class_name,
                                          sizeof(element_stack[stack_depth].class_name),
                                          class_name);
                        browser_copy_text(element_stack[stack_depth].id,
                                          sizeof(element_stack[stack_depth].id), id);
                        element_stack[stack_depth].box_index = element_box_index;
                        element_stack[stack_depth].dom_node = element_dom_index;
                        element_stack[stack_depth].centered_min_height =
                            centered_min_height;
                        style_stack[stack_depth] = current_style;
                        ++stack_depth;
                        current_style = element_style;
                    }
                    else
                    {
                        browser_close_dom_node(element_dom_index, end + 1,
                                               browser_document_length);
                    }
                }
            }
            index = end + 1;
            continue;
        }

        if (skip_head || skip_script || skip_style || template_depth > 0)
        {
            ++index;
            continue;
        }
        const int text_parent = stack_depth > 0
            ? element_stack[stack_depth - 1].dom_node : 0;
        if (current_style.hidden)
        {
            browser_append_dom_text(text_parent, index, index + 1);
            ++index;
            continue;
        }

        if (!current_style.preformatted &&
            (source[index] == ' ' || source[index] == '\t' ||
             source[index] == '\r' || source[index] == '\n'))
        {
            browser_append_dom_text(text_parent, index, index + 1);
            pending_space = true;
            ++index;
            continue;
        }

        const int text_source_start = index;
        char character = source[index];
        if (browser_decode_utf8(source, index, character))
        {
            browser_append_dom_text(text_parent, text_source_start, index);
            if (!browser_append_document(character, pending_space, current_style))
            {
                truncated = true;
                break;
            }
            continue;
        }
        if (browser_decode_entity(source, index, character))
        {
            browser_append_dom_text(text_parent, text_source_start, index);
            if (!browser_append_document(character, pending_space, current_style))
            {
                truncated = true;
                break;
            }
            continue;
        }
        if (!browser_append_document(character, pending_space, current_style))
        {
            truncated = true;
            break;
        }
        browser_append_dom_text(text_parent, text_source_start, index + 1);
        ++index;
    }

    for (int open = stack_depth - 1; open >= 0; --open)
    {
        browser_close_box(element_stack[open].box_index, browser_document_length);
        browser_close_dom_node(element_stack[open].dom_node,
                               source_length, browser_document_length);
    }

    int length = browser_document_length;
    while (length > 0 && (browser_document[length - 1] == ' ' || browser_document[length - 1] == '\n'))
    {
        browser_document[--length] = '\0';
    }
    browser_document_length = length;
    browser_dom_nodes[0].document_end = static_cast<uint32_t>(length);
    browser_source_truncated = truncated;
    browser_truncated = truncated;
    browser_columns = 0;
    browser_scroll = 0;
}

static void browser_add_line(const char* text, const uint32_t* offsets, int length,
                             int document_start, int document_end)
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
        browser_line_offsets[browser_line_count][index] = offsets[index];
    }
    browser_lines[browser_line_count][length] = '\0';
    browser_line_alignment[browser_line_count] = BROWSER_ALIGN_LEFT;
    for (int index = 0; index < length; ++index)
    {
        if (text[index] != ' ')
        {
            browser_line_alignment[browser_line_count] =
                browser_document_alignment[offsets[index]];
            break;
        }
    }
    browser_line_document_starts[browser_line_count] =
        static_cast<uint32_t>(document_start);
    browser_line_document_ends[browser_line_count] =
        static_cast<uint32_t>(document_end);
    ++browser_line_count;
}

static bool browser_find_line_range(uint32_t document_start,
                                    uint32_t document_end, int& first_line,
                                    int& last_line)
{
    first_line = -1;
    last_line = -1;
    for (int line = 0; line < browser_line_count; ++line)
    {
        if (browser_line_document_ends[line] <= document_start ||
            browser_line_document_starts[line] >= document_end)
            continue;
        if (first_line < 0) first_line = line;
        last_line = line;
    }
    return first_line >= 0;
}

static bool browser_is_inside_grid(int node_index)
{
    for (int ancestor = browser_dom_nodes[node_index].parent; ancestor > 0;
         ancestor = browser_dom_nodes[ancestor].parent)
    {
        if (browser_dom_nodes[ancestor].layout_mode == BROWSER_LAYOUT_GRID)
            return true;
    }
    return false;
}

static void browser_layout_grid_rows()
{
    for (int line = 0; line < browser_line_count; ++line)
    {
        browser_line_visual_rows[line] = line;
        browser_line_grid_assigned[line] = false;
    }

    for (int grid_index = 1; grid_index < browser_dom_node_count; ++grid_index)
    {
        const browser_dom_node& grid = browser_dom_nodes[grid_index];
        if (grid.kind != BROWSER_DOM_ELEMENT ||
            grid.layout_mode != BROWSER_LAYOUT_GRID ||
            grid.hidden ||
            grid.grid_columns < 2 || browser_is_inside_grid(grid_index))
            continue;

        int item_count = 0;
        int grid_first_line = BROWSER_MAX_LINES;
        int grid_last_line = -1;
        for (int child = grid.first_child; child >= 0;
             child = browser_dom_nodes[child].next_sibling)
        {
            const browser_dom_node& item = browser_dom_nodes[child];
            if (item.kind != BROWSER_DOM_ELEMENT) continue;
            if (item.hidden) continue;
            int first = -1;
            int last = -1;
            const bool has_lines =
                browser_find_line_range(item.document_start, item.document_end,
                                        first, last);
            if (item_count >= BROWSER_MAX_DOM_NODES) break;
            browser_grid_item_first_lines[item_count] =
                has_lines ? first : -1;
            browser_grid_item_last_lines[item_count] =
                has_lines ? last : -1;
            if (has_lines)
            {
                if (first < grid_first_line) grid_first_line = first;
                if (last > grid_last_line) grid_last_line = last;
            }
            ++item_count;
        }
        if (item_count < 2 || grid_last_line < grid_first_line) continue;

        int grid_base_row = browser_line_visual_rows[grid_first_line];
        int old_last_row = grid_base_row;
        for (int line = grid_first_line; line <= grid_last_line; ++line)
        {
            if (browser_line_visual_rows[line] > old_last_row)
                old_last_row = browser_line_visual_rows[line];
        }

        int next_row = grid_base_row;
        for (int first_item = 0; first_item < item_count;
             first_item += grid.grid_columns)
        {
            int row_height = 0;
            const int row_end = first_item + grid.grid_columns < item_count
                ? first_item + grid.grid_columns : item_count;
            for (int item = first_item; item < row_end; ++item)
            {
                if (browser_grid_item_first_lines[item] < 0) continue;
                const int height = browser_grid_item_last_lines[item] -
                                   browser_grid_item_first_lines[item] + 1;
                if (height > row_height) row_height = height;
            }
            for (int item = first_item; item < row_end; ++item)
            {
                const int first = browser_grid_item_first_lines[item];
                const int last = browser_grid_item_last_lines[item];
                if (first < 0) continue;
                for (int line = first; line <= last; ++line)
                {
                    browser_line_visual_rows[line] =
                        next_row + line - first;
                    browser_line_grid_assigned[line] = true;
                }
            }
            next_row += row_height;
            if (row_end < item_count)
                next_row += grid.grid_row_gap_lines;
        }

        const int new_last_row = next_row > grid_base_row
            ? next_row - 1 : grid_base_row;
        for (int line = grid_first_line; line <= grid_last_line; ++line)
        {
            if (!browser_line_grid_assigned[line])
            {
                browser_line_visual_rows[line] = new_last_row;
                browser_line_grid_assigned[line] = true;
            }
        }
        const int delta = new_last_row - old_last_row;
        for (int line = grid_last_line + 1; line < browser_line_count; ++line)
            browser_line_visual_rows[line] += delta;
    }

    browser_visual_line_count = 0;
    for (int line = 0; line < browser_line_count; ++line)
    {
        if (browser_line_visual_rows[line] + 1 > browser_visual_line_count)
            browser_visual_line_count = browser_line_visual_rows[line] + 1;
    }
}

static int browser_columns_at_offset(int document_offset, int maximum_columns)
{
    int content_x = 0;
    int content_width = browser_viewport_width;
    browser_get_content_geometry(document_offset, browser_box_count,
                                 browser_viewport_width, content_x,
                                 content_width);
    int columns = content_width / 9;
    if (columns < 1) columns = 1;
    return columns < maximum_columns ? columns : maximum_columns;
}

static void browser_layout_document(int columns)
{
    browser_line_count = 0;
    browser_truncated = browser_source_truncated;
    char line[BROWSER_LINE_MAX] = {};
    uint32_t line_offsets[BROWSER_LINE_MAX] = {};
    int line_length = 0;
    int line_document_start = 0;

    const auto flush_line = [&](int document_end)
    {
        browser_add_line(line, line_offsets, line_length, line_document_start,
                         document_end);
        line_length = 0;
        line_document_start = document_end;
    };

    for (int index = 0; browser_document[index] != '\0';)
    {
        if (browser_document[index] == '\n')
        {
            flush_line(index + 1);
            ++index;
            continue;
        }
        if (browser_document[index] == ' ')
        {
            if ((browser_document_flags[index] & BROWSER_PREFORMATTED) != 0)
            {
                const int available_columns =
                    browser_columns_at_offset(index, columns);
                if (line_length >= available_columns ||
                    line_length >= BROWSER_LINE_MAX - 1)
                {
                    flush_line(index);
                }
                line[line_length] = ' ';
                line_offsets[line_length] = static_cast<uint32_t>(index);
                ++line_length;
            }
            ++index;
            continue;
        }

        const int available_columns =
            browser_columns_at_offset(index, columns);
        if (line_length > 0 && line_length + 1 >= available_columns)
        {
            flush_line(index);
        }
        if (line_length > 0 && line[line_length - 1] != ' ')
        {
            const int document_index = index - 1;
            line[line_length] = ' ';
            line_offsets[line_length] = static_cast<uint32_t>(document_index);
            ++line_length;
        }
        while (browser_document[index] != '\0' &&
               browser_document[index] != ' ' && browser_document[index] != '\n')
        {
            if (line_length >= available_columns)
            {
                flush_line(index);
            }
            if (line_length >= BROWSER_LINE_MAX - 1)
            {
                flush_line(index);
            }
            line[line_length] = browser_document[index];
            line_offsets[line_length] = static_cast<uint32_t>(index);
            ++line_length;
            ++index;
        }
    }

    if (line_length > 0 || browser_line_count == 0)
    {
        flush_line(browser_document_length);
    }
    for (int box_index = 0; box_index < browser_box_count; ++box_index)
    {
        browser_box& box = browser_boxes[box_index];
        if (box.end_offset < 0) box.end_offset = browser_document_length;
        for (int line_index = 0; line_index < browser_line_count; ++line_index)
        {
            if (browser_line_document_ends[line_index] <=
                    static_cast<uint32_t>(box.start_offset) ||
                browser_line_document_starts[line_index] >=
                    static_cast<uint32_t>(box.end_offset))
                continue;
            if (box.first_line < 0) box.first_line = line_index;
            box.last_line = line_index;
        }
    }
    browser_layout_grid_rows();
    for (int box_index = 0; box_index < browser_box_count; ++box_index)
    {
        browser_box& box = browser_boxes[box_index];
        box.first_line = -1;
        box.last_line = -1;
        for (int line = 0; line < browser_line_count; ++line)
        {
            if (browser_line_document_ends[line] <=
                    static_cast<uint32_t>(box.start_offset) ||
                browser_line_document_starts[line] >=
                    static_cast<uint32_t>(box.end_offset))
                continue;
            const int visual_row = browser_line_visual_rows[line];
            if (box.first_line < 0 || visual_row < box.first_line)
                box.first_line = visual_row;
            if (visual_row > box.last_line) box.last_line = visual_row;
        }
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

static bool browser_copy_text(char* destination, int capacity, const char* source)
{
    int index = 0;
    while (source[index] != '\0' && index < capacity - 1)
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
    return source[index] == '\0';
}

static bool browser_looks_like_file_reference(const char* text)
{
    int end = 0;
    while (text[end] != '\0' && text[end] != '?' && text[end] != '#') ++end;
    int extension = end;
    while (extension > 0 && text[extension - 1] != '/' && text[extension - 1] != '.') --extension;
    if (extension == 0 || text[extension - 1] != '.') return false;

    char suffix[8] = {};
    int length = end - extension;
    if (length <= 0 || length >= static_cast<int>(sizeof(suffix))) return false;
    for (int i = 0; i < length; ++i) suffix[i] = browser_lower(text[extension + i]);
    return browser_equal_ignore_case(suffix, "html") ||
           browser_equal_ignore_case(suffix, "htm") ||
           browser_equal_ignore_case(suffix, "xhtml") ||
           browser_equal_ignore_case(suffix, "css") ||
           browser_equal_ignore_case(suffix, "txt") ||
           browser_equal_ignore_case(suffix, "md") ||
           browser_equal_ignore_case(suffix, "json") ||
           browser_equal_ignore_case(suffix, "xml") ||
           browser_equal_ignore_case(suffix, "php") ||
           browser_equal_ignore_case(suffix, "asp");
}

static bool browser_normalize_local_path(const char* path, char* output, int capacity)
{
    int segment_starts[128];
    int segment_count = 0;
    int output_length = 0;
    const bool absolute = path[0] == '/';
    if (absolute)
    {
        output[output_length++] = '/';
    }

    int index = 0;
    while (path[index] != '\0')
    {
        while (path[index] == '/') ++index;
        int start = index;
        while (path[index] != '\0' && path[index] != '/') ++index;
        int length = index - start;
        if (length == 0 || (length == 1 && path[start] == '.'))
        {
            continue;
        }
        if (length == 2 && path[start] == '.' && path[start + 1] == '.')
        {
            if (segment_count > 0)
            {
                output_length = segment_starts[--segment_count];
                if (output_length > (absolute ? 1 : 0) && output[output_length - 1] == '/')
                {
                    --output_length;
                }
                output[output_length] = '\0';
            }
            continue;
        }
        if (segment_count >= static_cast<int>(sizeof(segment_starts) / sizeof(segment_starts[0])))
        {
            return false;
        }
        if (output_length > 0 && output[output_length - 1] != '/')
        {
            if (output_length >= capacity - 1) return false;
            output[output_length++] = '/';
        }
        segment_starts[segment_count++] = output_length;
        while (length-- > 0)
        {
            if (output_length >= capacity - 1) return false;
            output[output_length++] = path[start++];
        }
        output[output_length] = '\0';
    }
    if (output_length == 0)
    {
        output[output_length++] = absolute ? '/' : '.';
    }
    output[output_length] = '\0';
    return true;
}

static bool browser_resolve_link(const char* href, char* resolved, int capacity)
{
    if (href[0] == '\0' || href[0] == '#')
    {
        return false;
    }
    if (browser_has_prefix_ignore_case(href, "http://") ||
        browser_has_prefix_ignore_case(href, "https://"))
    {
        return browser_copy_text(resolved, capacity, href);
    }
    if (browser_has_prefix_ignore_case(href, "javascript:") ||
        browser_has_prefix_ignore_case(href, "mailto:"))
    {
        return false;
    }

    const bool current_https = browser_has_prefix_ignore_case(browser_address, "https://");
    const bool current_http = browser_has_prefix_ignore_case(browser_address, "http://");
    if (href[0] == '/' && href[1] == '/' && (current_http || current_https))
    {
        char scheme_href[256];
        const char* scheme = current_https ? "https:" : "http:";
        int scheme_length = browser_length(scheme);
        if (scheme_length + browser_length(href) >= static_cast<int>(sizeof(scheme_href))) return false;
        browser_copy_text(scheme_href, sizeof(scheme_href), scheme);
        browser_copy_text(scheme_href + scheme_length, sizeof(scheme_href) - scheme_length, href);
        return browser_copy_text(resolved, capacity, scheme_href);
    }

    int href_authority_end = 0;
    while (href[href_authority_end] != '\0' &&
           href[href_authority_end] != '/' &&
           href[href_authority_end] != '?' &&
           href[href_authority_end] != '#')
    {
        ++href_authority_end;
    }
    bool external_domain = href[0] != '/' && href[0] != '.' && href[0] != '?' &&
                           href[0] != '#' && !browser_looks_like_file_reference(href);
    bool has_domain_dot = false;
    for (int i = 0; i < href_authority_end; ++i)
    {
        if (href[i] == '.') has_domain_dot = true;
        if (href[i] == ':' || href[i] == ' ' || href[i] == '@') external_domain = false;
    }
    external_domain = external_domain && has_domain_dot;
    if (external_domain)
    {
        char absolute[256] = {};
        if (browser_length(href) + 8 >= static_cast<int>(sizeof(absolute))) return false;
        browser_copy_text(absolute, sizeof(absolute), "https://");
        browser_copy_text(absolute + 8, sizeof(absolute) - 8, href);
        return browser_copy_text(resolved, capacity, absolute);
    }

    char joined[512] = {};
    int joined_length = 0;
    if (current_http || current_https)
    {
        int origin_end = current_https ? 8 : 7;
        while (browser_address[origin_end] != '\0' &&
               browser_address[origin_end] != '/' &&
               browser_address[origin_end] != '?' &&
               browser_address[origin_end] != '#')
        {
            ++origin_end;
        }
        if (origin_end >= static_cast<int>(sizeof(joined)) - 1) return false;
        for (int i = 0; i < origin_end; ++i) joined[joined_length++] = browser_address[i];

        if (href[0] == '/')
        {
            for (int i = 0; href[i] != '\0' && joined_length < static_cast<int>(sizeof(joined)) - 1; ++i)
            {
                joined[joined_length++] = href[i];
            }
        }
        else if (href[0] == '?')
        {
            int path_end = origin_end;
            while (browser_address[path_end] != '\0' &&
                   browser_address[path_end] != '?' &&
                   browser_address[path_end] != '#')
            {
                if (joined_length >= static_cast<int>(sizeof(joined)) - 1) return false;
                joined[joined_length++] = browser_address[path_end++];
            }
            for (int i = 0; href[i] != '\0' && joined_length < static_cast<int>(sizeof(joined)) - 1; ++i)
            {
                joined[joined_length++] = href[i];
            }
        }
        else
        {
            int path_end = origin_end;
            while (browser_address[path_end] != '\0' &&
                   browser_address[path_end] != '?' &&
                   browser_address[path_end] != '#')
            {
                ++path_end;
            }
            int directory_end = path_end;
            while (directory_end > origin_end && browser_address[directory_end - 1] != '/')
            {
                --directory_end;
            }
            for (int i = origin_end; i < directory_end; ++i)
            {
                if (joined_length >= static_cast<int>(sizeof(joined)) - 1) return false;
                joined[joined_length++] = browser_address[i];
            }
            for (int i = 0; href[i] != '\0' && joined_length < static_cast<int>(sizeof(joined)) - 1; ++i)
            {
                joined[joined_length++] = href[i];
            }
        }
        joined[joined_length] = '\0';
        return joined_length > 0 && browser_copy_text(resolved, capacity, joined);
    }

    if (href[0] == '/')
    {
        return browser_normalize_local_path(href, resolved, capacity);
    }
    int address_length = browser_length(browser_address);
    while (address_length > 0 && browser_address[address_length - 1] != '/')
    {
        --address_length;
    }
    if (address_length + browser_length(href) >= static_cast<int>(sizeof(joined)))
    {
        return false;
    }
    for (int i = 0; i < address_length; ++i) joined[joined_length++] = browser_address[i];
    for (int i = 0; href[i] != '\0'; ++i) joined[joined_length++] = href[i];
    joined[joined_length] = '\0';
    return browser_normalize_local_path(joined, resolved, capacity);
}

static bool browser_find_http_header(const char* response, uint32_t header_end,
                                     const char* name, char* output, int capacity)
{
    uint32_t line_start = 0;
    while (line_start < header_end && response[line_start] != '\n') ++line_start;
    if (line_start < header_end) ++line_start;
    while (line_start < header_end)
    {
        uint32_t line_end = line_start;
        while (line_end < header_end && response[line_end] != '\n') ++line_end;
        uint32_t value_start = line_start;
        while (value_start < line_end && response[value_start] != ':') ++value_start;
        if (value_start < line_end)
        {
            uint32_t key_length = value_start - line_start;
            if (key_length == static_cast<uint32_t>(browser_length(name)))
            {
                uint32_t i = 0;
                while (i < key_length && browser_lower(response[line_start + i]) == browser_lower(name[i])) ++i;
                if (i == key_length)
                {
                    ++value_start;
                    while (value_start < line_end &&
                           (response[value_start] == ' ' || response[value_start] == '\t'))
                    {
                        ++value_start;
                    }
                    if (line_end > value_start && response[line_end - 1] == '\r') --line_end;
                    int length = static_cast<int>(line_end - value_start);
                    if (length >= capacity) length = capacity - 1;
                    for (int copy = 0; copy < length; ++copy) output[copy] = response[value_start + copy];
                    output[length] = '\0';
                    return length > 0;
                }
            }
        }
        line_start = line_end + 1;
    }
    if (capacity > 0) output[0] = '\0';
    return false;
}

static bool browser_load_stylesheet(const char* href, uint8_t redirects)
{
    char address[BROWSER_ADDRESS_MAX] = {};
    if (!browser_resolve_link(href, address, sizeof(address)))
        return false;

    if (browser_has_prefix_ignore_case(address, "http://") ||
        browser_has_prefix_ignore_case(address, "https://"))
    {
        const bool secure = browser_has_prefix_ignore_case(address, "https://");
        if (browser_api == nullptr ||
            (secure && browser_api->https_get == nullptr) ||
            (!secure && browser_api->http_get == nullptr))
        {
            return false;
        }
        char hostname[128] = {};
        char path[256] = {};
        uint16_t port = secure ? 443 : 80;
        if (!browser_parse_url(address, secure, hostname, sizeof(hostname),
                               port, path, sizeof(path)))
        {
            return false;
        }
        const int32_t response_length = secure
            ? browser_api->https_get(hostname, port, path, browser_response,
                                     BROWSER_RESPONSE_CAPACITY, 12000)
            : browser_api->http_get(hostname, port, path, browser_response,
                                    BROWSER_RESPONSE_CAPACITY, 10000);
        if (response_length <= 0 ||
            response_length >= BROWSER_RESPONSE_CAPACITY)
        {
            return false;
        }
        uint32_t body_offset = 0;
        if (!browser_find_http_body(browser_response,
                                    static_cast<uint32_t>(response_length),
                                    body_offset))
        {
            return false;
        }
        const int status_code = browser_http_status_code(
            browser_response, static_cast<uint32_t>(response_length));
        if (status_code >= 300 && status_code < 400)
        {
            char location[256] = {};
            char resolved[BROWSER_ADDRESS_MAX] = {};
            char page_address[BROWSER_ADDRESS_MAX] = {};
            if (redirects >= 3 ||
                !browser_find_http_header(browser_response, body_offset, "location",
                                          location, sizeof(location)) ||
                !browser_copy_text(page_address, sizeof(page_address), browser_address) ||
                !browser_copy_text(browser_address, sizeof(browser_address), address))
            {
                return false;
            }
            const bool resolved_ok =
                browser_resolve_link(location, resolved, sizeof(resolved));
            browser_copy_text(browser_address, sizeof(browser_address), page_address);
            return resolved_ok && browser_load_stylesheet(resolved, redirects + 1);
        }
        if (status_code < 200 || status_code >= 300)
            return false;

        uint32_t css_length = 0;
        const uint32_t body_length =
            static_cast<uint32_t>(response_length) - body_offset;
        if (browser_http_is_chunked(browser_response, body_offset))
        {
            if (!browser_decode_chunked(browser_response + body_offset, body_length,
                                        browser_response,
                                        BROWSER_RESPONSE_CAPACITY, css_length))
            {
                return false;
            }
        }
        else
        {
            css_length = body_length;
            browser_response[css_length] = '\0';
        }
        browser_parse_css_rules(browser_response, browser_response + css_length);
        return true;
    }

    if (browser_api == nullptr || browser_api->clawfs_read_file == nullptr)
        return false;
    uint32_t css_length = 0;
    if (browser_api->clawfs_get_file_size != nullptr &&
        browser_api->clawfs_read_file_range != nullptr)
    {
        uint32_t file_size = 0;
        if (!browser_api->clawfs_get_file_size(address, &file_size))
            return false;
        if (file_size >= BROWSER_RESPONSE_CAPACITY)
            return false;
        css_length = file_size;
        if (css_length > 0 &&
            !browser_api->clawfs_read_file_range(address, 0, browser_response,
                                                 css_length))
        {
            return false;
        }
    }
    else if (!browser_api->clawfs_read_file(address, browser_response,
                                            BROWSER_RESPONSE_CAPACITY - 1,
                                            &css_length))
    {
        return false;
    }
    browser_response[css_length] = '\0';
    browser_parse_css_rules(browser_response, browser_response + css_length);
    return true;
}

static bool browser_normalize_address()
{
    if (browser_address[0] == '\0' ||
        browser_has_prefix_ignore_case(browser_address, "http://") ||
        browser_has_prefix_ignore_case(browser_address, "https://") ||
        browser_address[0] == '/' || browser_looks_like_file_reference(browser_address))
    {
        return true;
    }

    int authority_end = 0;
    while (browser_address[authority_end] != '\0' &&
           browser_address[authority_end] != '/' &&
           browser_address[authority_end] != '?' &&
           browser_address[authority_end] != '#')
    {
        ++authority_end;
    }
    bool looks_like_domain = false;
    for (int i = 0; i < authority_end; ++i)
    {
        if (browser_address[i] == '.') looks_like_domain = true;
        if (browser_address[i] == ' ' || browser_address[i] == '\t' ||
            browser_address[i] == '\\' || browser_address[i] == '@')
        {
            return true;
        }
    }
    if (!looks_like_domain) return true;

    char normalized[BROWSER_ADDRESS_MAX] = {};
    const int prefix_length = 8;
    if (browser_length(browser_address) + prefix_length >= static_cast<int>(sizeof(normalized)))
    {
        browser_status = "URL is too long";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    browser_copy_text(normalized, sizeof(normalized), "https://");
    browser_copy_text(normalized + prefix_length, sizeof(normalized) - prefix_length, browser_address);
    browser_copy_text(browser_address, sizeof(browser_address), normalized);
    return true;
}

static bool browser_load_page(int redirects = 0)
{
    if (browser_address[0] == '\0')
    {
        browser_status = "Enter a URL or local HTML file path";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    if (!browser_normalize_address())
    {
        return false;
    }

    char hostname[128] = {};
    char path[256] = {};
    uint16_t port = 80;
    const bool is_https_url = browser_has_prefix_ignore_case(browser_address, "https://");
    const bool is_http_url = browser_has_prefix_ignore_case(browser_address, "http://");
    if (is_https_url || is_http_url)
    {
        if (browser_api == nullptr ||
            (is_https_url && browser_api->https_get == nullptr) ||
            (is_http_url && browser_api->http_get == nullptr))
        {
            browser_status = is_https_url ? "HTTPS network service is unavailable" :
                                            "HTTP network service is unavailable";
            browser_status_color = 0xFFFF7777;
            return false;
        }
        if (!browser_parse_url(browser_address, is_https_url, hostname, sizeof(hostname),
                               port, path, sizeof(path)))
        {
            browser_status = "Invalid web URL";
            browser_status_color = 0xFFFF7777;
            return false;
        }

        const int32_t response_length = is_https_url
            ? browser_api->https_get(hostname, port, path, browser_response,
                                     BROWSER_RESPONSE_CAPACITY, 20000)
            : browser_api->http_get(hostname, port, path, browser_response,
                                    BROWSER_RESPONSE_CAPACITY, 15000);
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
                    browser_status = "HTTP response exceeds the 1 MiB limit";
                    break;
                case NAPP_HTTPS_ERROR_TLS:
                    browser_status = "HTTPS/TLS handshake failed";
                    break;
                case NAPP_HTTPS_ERROR_CERTIFICATE:
                    browser_status = "HTTPS certificate rejected";
                    break;
                case NAPP_HTTPS_ERROR_CLOCK:
                    browser_status = "Set a valid system date for HTTPS";
                    break;
                case NAPP_HTTPS_ERROR_ENTROPY:
                    browser_status = "Secure random source unavailable";
                    break;
                default:
                    browser_status = "Web request failed";
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
            if (redirects >= 5)
            {
                browser_status = "Too many redirects";
                browser_status_color = 0xFFFF7777;
                return false;
            }
            char location[256] = {};
            char resolved[ BROWSER_ADDRESS_MAX ] = {};
            if (!browser_find_http_header(browser_response, body_offset, "location",
                                          location, sizeof(location)) ||
                !browser_resolve_link(location, resolved, sizeof(resolved)) ||
                !browser_copy_text(browser_address, sizeof(browser_address), resolved))
            {
                browser_set_status_code("Invalid redirect: ", status_code);
                browser_status_color = 0xFFFF7777;
                return false;
            }
            return browser_load_page(redirects + 1);
        }

        browser_source_truncated = false;
        const uint32_t body_length = static_cast<uint32_t>(response_length) - body_offset;
        uint32_t page_source_length = 0;
        if (browser_http_is_chunked(browser_response, body_offset))
        {
            if (!browser_decode_chunked(browser_response + body_offset, body_length,
                                        browser_page_source, BROWSER_RESPONSE_CAPACITY,
                                        page_source_length))
            {
                browser_status = "Invalid chunked HTTP response";
                browser_status_color = 0xFFFF7777;
                return false;
            }
        }
        else
        {
            page_source_length = body_length;
            if (page_source_length >= BROWSER_RESPONSE_CAPACITY)
            {
                page_source_length = BROWSER_RESPONSE_CAPACITY - 1;
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
        else if (browser_stylesheet_failed)
        {
            browser_status = browser_truncated
                ? "Styles unavailable; page truncated"
                : "Page loaded (some styles unavailable)";
            browser_status_color = 0xFFFFCC66;
        }
        else if ((browser_box_truncated || browser_dom_truncated) &&
                 !browser_truncated)
        {
            browser_status = "Page loaded (layout structure limited)";
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

    if (browser_api == nullptr || browser_api->clawfs_read_file == nullptr)
    {
        browser_status = "Local filesystem access is unavailable";
        browser_status_color = 0xFFFF7777;
        return false;
    }

    uint32_t source_length = 0;
    browser_source_truncated = false;
    if (browser_api->clawfs_get_file_size != nullptr &&
        browser_api->clawfs_read_file_range != nullptr)
    {
        uint32_t file_size = 0;
        if (!browser_api->clawfs_get_file_size(browser_address, &file_size))
        {
            browser_status = "Could not read local file";
            browser_status_color = 0xFFFF7777;
            return false;
        }
        source_length = file_size;
        if (source_length >= BROWSER_RESPONSE_CAPACITY)
        {
            source_length = BROWSER_RESPONSE_CAPACITY - 1;
            browser_source_truncated = true;
        }
        if (source_length > 0 &&
            !browser_api->clawfs_read_file_range(browser_address, 0,
                                                 browser_page_source, source_length))
        {
            browser_status = "Could not read local file";
            browser_status_color = 0xFFFF7777;
            return false;
        }
    }
    else if (!browser_api->clawfs_read_file(browser_address, browser_page_source,
                                            BROWSER_RESPONSE_CAPACITY - 1,
                                            &source_length))
    {
        browser_status = "Local file exceeds the 1 MiB limit or could not be read";
        browser_status_color = 0xFFFF7777;
        return false;
    }
    browser_page_source[source_length] = '\0';
    browser_parse_html(browser_page_source);
    if (browser_stylesheet_failed)
    {
        browser_status = browser_truncated
            ? "Styles unavailable; page truncated"
            : "Page opened (some styles unavailable)";
        browser_status_color = 0xFFFFCC66;
    }
    else if ((browser_box_truncated || browser_dom_truncated) &&
             !browser_truncated)
    {
        browser_status = "Page opened (layout structure limited)";
        browser_status_color = 0xFFFFCC66;
    }
    else
    {
        browser_status = browser_truncated ? "Page opened (content truncated)" : "Page opened";
        browser_status_color = browser_truncated ? 0xFFFFCC66 : BROWSER_ACCENT;
    }
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
    const int viewport_width = window->width - 20;
    const int previous_viewport_width = browser_viewport_width;
    bool crossed_media_breakpoint = false;
    for (int index = 0; index < browser_media_breakpoint_count; ++index)
    {
        const browser_media_breakpoint& breakpoint =
            browser_media_breakpoints[index];
        const bool previously_matched = breakpoint.maximum
            ? previous_viewport_width <= breakpoint.width
            : previous_viewport_width >= breakpoint.width;
        const bool now_matches = breakpoint.maximum
            ? viewport_width <= breakpoint.width
            : viewport_width >= breakpoint.width;
        if (previously_matched != now_matches)
            crossed_media_breakpoint = true;
    }
    browser_viewport_width = viewport_width;
    if (previous_viewport_width != viewport_width)
        browser_columns = 0;
    const bool viewport_height_changed = browser_viewport_height != body_height;
    browser_viewport_height = body_height;
    if ((crossed_media_breakpoint ||
         (viewport_height_changed && browser_stylesheet_count == 0)) &&
        browser_page_source != nullptr && browser_page_source[0] != '\0')
        browser_parse_html(browser_page_source);
    const int columns = (window->width - 20) / 9;
    const int address_width = window->width - 88;
    const int address_characters = (address_width - 12) / 9;
    const int status_characters = (window->width - 20) / 9;

    browser_gui->fill_block(window->pos_x, content_top, browser_page_background,
                            window->width, window->height - window->title_height);
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

    const int visible_rows = body_height > 0 ? body_height / browser_content_height : 1;
    int max_scroll = browser_visual_line_count - visible_rows;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }
    if (browser_scroll > max_scroll)
    {
        browser_scroll = max_scroll;
    }

    for (int box_index = 0; box_index < browser_box_count; ++box_index)
    {
        const browser_box& box = browser_boxes[box_index];
        if (box.first_line < 0 || box.last_line < browser_scroll ||
            box.first_line >= browser_scroll + visible_rows)
            continue;

        const int visible_first = box.first_line > browser_scroll
            ? box.first_line : browser_scroll;
        const int visible_last = box.last_line < browser_scroll + visible_rows
            ? box.last_line : browser_scroll + visible_rows - 1;
        const int box_y = body_y +
            (visible_first - browser_scroll) * browser_content_height;
        const int box_height =
            (visible_last - visible_first + 1) * browser_content_height;
        int box_x = 0;
        int box_width = window->width - 20;
        browser_get_box_geometry(box_index, window->width - 20,
                                 box_x, box_width);
        if (box.background != BROWSER_NO_BACKGROUND)
        {
            browser_gui->fill_block(left + box_x, box_y, box.background, box_width,
                                    box_height);
        }
    }

    for (int line = 0; line < browser_line_count; ++line)
    {
        const int visual_row = browser_line_visual_rows[line];
        if (visual_row < browser_scroll ||
            visual_row >= browser_scroll + visible_rows)
            continue;
        int text_length = browser_length(browser_lines[line]);
        int content_x = 0;
        int content_width = window->width - 20;
        const int line_document_offset = text_length > 0
            ? static_cast<int>(browser_line_offsets[line][0])
            : static_cast<int>(browser_line_document_starts[line]);
        browser_get_content_geometry(line_document_offset, browser_box_count,
                                     window->width - 20, content_x,
                                     content_width);
        int draw_x = left + content_x;
        if (browser_line_alignment[line] == BROWSER_ALIGN_CENTER)
        {
            draw_x += (content_width - text_length * 9) / 2;
        }
        else if (browser_line_alignment[line] == BROWSER_ALIGN_RIGHT)
        {
            draw_x += content_width - text_length * 9;
        }

        const int line_y = body_y +
            (visual_row - browser_scroll) * browser_content_height;
        for (int start = 0; start < text_length;)
        {
            const int document_offset = browser_line_offsets[line][start];
            int end = start + 1;
            const uint32_t background = browser_document_backgrounds[document_offset];
            while (end < text_length &&
                   browser_document_backgrounds[browser_line_offsets[line][end]] == background)
            {
                ++end;
            }
            if (background != BROWSER_NO_BACKGROUND)
            {
                browser_gui->fill_block(draw_x + start * 9, line_y, background,
                                        (end - start) * 9, browser_content_height);
            }
            start = end;
        }

        int start = 0;
        while (start < text_length)
        {
            const int document_offset = browser_line_offsets[line][start];
            const uint32_t color = browser_document_colors[document_offset];
            const uint8_t flags = browser_document_flags[document_offset];
            const uint8_t link_id = browser_document_links[document_offset];
            const uint32_t border = browser_document_borders[document_offset];
            int end = start + 1;
            while (end < text_length &&
                   browser_document_colors[browser_line_offsets[line][end]] == color &&
                   browser_document_flags[browser_line_offsets[line][end]] == flags &&
                   browser_document_links[browser_line_offsets[line][end]] == link_id &&
                   browser_document_backgrounds[browser_line_offsets[line][end]] ==
                       browser_document_backgrounds[document_offset] &&
                   browser_document_borders[browser_line_offsets[line][end]] == border)
            {
                ++end;
            }
            char run[BROWSER_LINE_MAX] = {};
            int run_length = end - start;
            for (int character = 0; character < run_length; ++character)
            {
                run[character] = browser_lines[line][start + character];
            }
            run[run_length] = '\0';
            const int run_x = draw_x + start * 9;
            if (border != BROWSER_NO_BACKGROUND)
            {
                browser_gui->fill_block(run_x, line_y, border, run_length * 9, 1);
                browser_gui->fill_block(run_x, line_y + browser_content_height - 1,
                                        border, run_length * 9, 1);
                browser_gui->fill_block(run_x, line_y, border, 1,
                                        browser_content_height);
                browser_gui->fill_block(run_x + run_length * 9 - 1, line_y, border,
                                        1, browser_content_height);
            }
            browser_gui->draw_text(run, run_x, line_y, color);
            if ((flags & BROWSER_BOLD) != 0)
            {
                browser_gui->draw_text(run, run_x + 1, line_y, color);
            }
            if ((flags & BROWSER_UNDERLINE) != 0)
            {
                browser_gui->fill_block(run_x, line_y + 9, color, run_length * 9, 1);
            }
            if ((flags & BROWSER_STRIKETHROUGH) != 0)
            {
                browser_gui->fill_block(run_x, line_y + 5, color, run_length * 9, 1);
            }
            if ((flags & BROWSER_OVERLINE) != 0)
            {
                browser_gui->fill_block(run_x, line_y, color, run_length * 9, 1);
            }
            start = end;
        }
    }
    for (int box_index = 0; box_index < browser_box_count; ++box_index)
    {
        const browser_box& box = browser_boxes[box_index];
        if (box.border == BROWSER_NO_BACKGROUND || box.first_line < 0 ||
            box.last_line < browser_scroll ||
            box.first_line >= browser_scroll + visible_rows)
            continue;

        const int visible_first = box.first_line > browser_scroll
            ? box.first_line : browser_scroll;
        const int visible_last = box.last_line < browser_scroll + visible_rows
            ? box.last_line : browser_scroll + visible_rows - 1;
        const int box_y = body_y +
            (visible_first - browser_scroll) * browser_content_height;
        const int box_height =
            (visible_last - visible_first + 1) * browser_content_height;
        int box_x = 0;
        int box_width = window->width - 20;
        browser_get_box_geometry(box_index, window->width - 20,
                                 box_x, box_width);
        if (box.first_line >= browser_scroll)
            browser_gui->fill_block(left + box_x, box_y, box.border,
                                    box_width, 1);
        if (box.last_line < browser_scroll + visible_rows)
        {
            browser_gui->fill_block(
                left + box_x, box_y + box_height - 1, box.border, box_width, 1);
        }
        browser_gui->fill_block(left + box_x, box_y, box.border, 1, box_height);
        browser_gui->fill_block(left + box_x + box_width - 1, box_y,
                                box.border, 1,
                                box_height);
    }
    const int remaining_rows = browser_visual_line_count - browser_scroll;
    if (browser_truncated && remaining_rows >= 0 &&
        remaining_rows < visible_rows)
    {
        browser_draw_text_clipped("[Document truncated]", left,
            body_y + remaining_rows * browser_content_height,
            0xFFAA5500, columns);
    }
}

static int browser_find_fragment(const char* id)
{
    for (int index = 0; index < browser_fragment_count; ++index)
    {
        const char* stored = browser_fragments[index].id;
        int character = 0;
        while (stored[character] != '\0' && id[character] != '\0' &&
               stored[character] == id[character])
        {
            ++character;
        }
        if (stored[character] == '\0' && id[character] == '\0')
            return browser_fragments[index].document_offset;
    }
    return -1;
}

static void browser_scroll_to_offset(int offset)
{
    int target_line = -1;
    for (int line = 0; line < browser_line_count; ++line)
    {
        const int length = browser_length(browser_lines[line]);
        if (length == 0) continue;
        if (offset <= static_cast<int>(browser_line_offsets[line][length - 1]))
        {
            target_line = line;
            break;
        }
        if (offset < static_cast<int>(browser_line_offsets[line][0]))
        {
            target_line = line;
            break;
        }
    }
    if (target_line < 0 && browser_line_count > 0)
        target_line = browser_line_count - 1;
    if (target_line >= 0)
        browser_scroll = browser_line_visual_rows[target_line];
}

static void browser_key(napp_window* window, char key)
{
    const int viewport_height = window->height - window->title_height - 72;
    if (viewport_height > 0) browser_viewport_height = viewport_height;
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

    const int page_rows = (window->height - window->title_height - 72) / browser_content_height;
    if (key == 'e' || key == 'g' || key == '\n')
    {
        browser_address_focused = true;
        if (key == '\n')
        {
            browser_status = "Address selected - type a URL, then press Enter to open";
            browser_status_color = BROWSER_ACCENT;
        }
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
    const int viewport_height = window->height - window->title_height - 72;
    if (viewport_height > 0) browser_viewport_height = viewport_height;
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
        const int body_y = content_top + 58;
        const int body_height = window->height - window->title_height - 72;
        const int columns = (window->width - 20) / 9;
        if (mouse_y < body_y || body_height <= 0 ||
            mouse_y >= body_y + body_height || mouse_x < window->pos_x + 10)
        {
            return;
        }
        if (columns > BROWSER_LINE_MAX - 1)
        {
            browser_layout_document(BROWSER_LINE_MAX - 1);
        }
        else if (columns >= 12 && columns != browser_columns)
        {
            browser_layout_document(columns);
        }

        const int target_row =
            browser_scroll + (mouse_y - body_y) / browser_content_height;
        if (target_row < 0 || target_row >= browser_visual_line_count) return;
        int document_offset = -1;
        for (int row = 0; row < browser_line_count; ++row)
        {
            if (browser_line_visual_rows[row] != target_row) continue;
            const int length = browser_length(browser_lines[row]);
            const int line_document_offset = length > 0
                ? static_cast<int>(browser_line_offsets[row][0])
                : static_cast<int>(browser_line_document_starts[row]);
            int content_x = 0;
            int content_width = window->width - 20;
            browser_get_content_geometry(
                line_document_offset, browser_box_count, window->width - 20,
                content_x, content_width);
            int draw_x = window->pos_x + 10 + content_x;
            if (browser_line_alignment[row] == BROWSER_ALIGN_CENTER)
                draw_x += (content_width - length * 9) / 2;
            else if (browser_line_alignment[row] == BROWSER_ALIGN_RIGHT)
                draw_x += content_width - length * 9;
            const int column = (mouse_x - draw_x) / 9;
            if (column >= 0 && column < length)
                document_offset = browser_line_offsets[row][column];
            if (document_offset >= 0) break;
        }
        if (document_offset < 0) return;

        const uint8_t link_id = browser_document_links[document_offset];
        if (link_id == 0 || link_id > browser_link_count) return;

        const char* href = browser_links[link_id - 1].href;
        if (href[0] == '#')
        {
            if (href[1] == '\0')
            {
                browser_scroll = 0;
                return;
            }
            const int target_offset = browser_find_fragment(href + 1);
            if (target_offset < 0)
            {
                browser_status = "Anchor not found";
                browser_status_color = 0xFFFFCC66;
                return;
            }
            browser_scroll_to_offset(target_offset);
            browser_status = "Moved to page section";
            browser_status_color = BROWSER_ACCENT;
            return;
        }

        char resolved[BROWSER_ADDRESS_MAX] = {};
        if (!browser_resolve_link(href, resolved, sizeof(resolved)))
        {
            browser_status = "This link type is not supported";
            browser_status_color = 0xFFFFCC66;
            return;
        }
        if (!browser_copy_text(browser_address, sizeof(browser_address), resolved))
        {
            browser_status = "Link address is too long";
            browser_status_color = 0xFFFF7777;
            return;
        }
        browser_load_page();
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION ||
        api->gui == nullptr || api->gui->open_window == nullptr ||
        api->gui->draw_text == nullptr || api->gui->fill_block == nullptr ||
        api->allocate == nullptr || api->deallocate == nullptr)
    {
        return 1;
    }

    browser_api = api;
    browser_gui = api->gui;
    if (!browser_allocate_page_buffers())
    {
        browser_status = "Not enough memory for browser page buffers";
        browser_status_color = 0xFFFF7777;
        return 1;
    }
    browser_address[0] = '\0';
    browser_response[0] = '\0';
    browser_page_source[0] = '\0';
    browser_source_truncated = false;
    browser_truncated = false;
    browser_set_document(
        "Welcome to WebBrowser\n"
        "\n"
        "Open a URL, local HTML file, or click a link to navigate between pages.\n"
        "Bare domains such as google.com are opened over HTTPS automatically.\n"
        "\n"
        "CSS supports tag/class/ID selectors, inline styles, colors, backgrounds,\n"
        "text decorations, transforms, alignment, visibility, and preformatted text.\n"
        "HTML supports headings, semantic text, lists, links, tables, forms, and image alt text.\n"
        "Use j/k to scroll, Page Up/Down to move a page, or e to edit the address.\n"
        "\n"
        "HTTPS requires a valid system clock and CPU hardware random support.");

    napp_window_config config = {};
    config.title = "WebBrowser";
    config.width = 820;
    config.height = 620;
    config.resizable = true;
    config.can_maximize = true;
    config.draw = browser_draw;
    config.key = browser_key;
    config.mouse = browser_mouse;

    return api->gui->open_window(&config) ? 0 : 1;
}
