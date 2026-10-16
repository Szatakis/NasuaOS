#include <napp.h>

NAPP_APPLICATION("file explorer", "Graphical file explorer", true);

static constexpr uint32_t FILES_BACKGROUND = 0xFF11151D;
static constexpr uint32_t FILES_SURFACE = 0xFF1B222D;
static constexpr uint32_t FILES_INPUT = 0xFF283240;
static constexpr uint32_t FILES_TEXT = 0xFFE1E7EF;
static constexpr uint32_t FILES_MUTED = 0xFF9BA8B8;
static constexpr uint32_t FILES_ACCENT = 0xFF59E81B;
static constexpr uint32_t FILES_SELECTED = 0xFF354554;
static constexpr int FILES_ENTRY_COUNT = 12;
static constexpr int FILES_NAME_LENGTH = 28;
static constexpr int FILES_PREVIEW_CAPACITY = 1024;
static constexpr int FILES_PATH_LIMIT = 128;

struct files_entry
{
    char name[FILES_NAME_LENGTH];
    uint32_t type;
    uint32_t data_sector;
    uint32_t size;
} __attribute__((packed));

struct files_state
{
    char path[256];
    char status[64];
    char preview[FILES_PREVIEW_CAPACITY + 1];
    files_entry entries[FILES_ENTRY_COUNT];
    int entry_count;
    int selected;
    int previous_click;
    uint64_t previous_click_tick;
};

static const napp_api* files_api = nullptr;
static const napp_gui* files_gui = nullptr;
static files_state state = {};

static int files_length(const char* text)
{
    int length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}

static void files_copy(char* destination, const char* source, int capacity)
{
    int index = 0;
    while (index < capacity - 1 && source[index] != '\0')
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static bool files_append(char* destination, const char* source, int capacity)
{
    int length = files_length(destination);
    int index = 0;
    while (source[index] != '\0' && length + index < capacity - 1)
    {
        destination[length + index] = source[index];
        ++index;
    }
    destination[length + index] = '\0';
    return source[index] == '\0';
}

static void files_set_status(const char* text)
{
    files_copy(state.status, text, sizeof(state.status));
}

static bool files_build_entry_path(char* output, const char* name)
{
    files_copy(output, state.path, 256);
    if (files_length(output) > 1 && !files_append(output, "/", 256))
    {
        return false;
    }
    return files_append(output, name, 256) && files_length(output) < FILES_PATH_LIMIT;
}

static void files_load_preview()
{
    state.preview[0] = '\0';
    if (state.selected < 0 || state.selected >= state.entry_count)
    {
        return;
    }
    if (state.entries[state.selected].type != 0)
    {
        files_set_status("Folder selected");
        return;
    }

    char file_path[256];
    if (!files_build_entry_path(file_path, state.entries[state.selected].name))
    {
        files_set_status("File path is too long");
        return;
    }

    uint32_t length = 0;
    if (!files_api->clawfs_read_file(file_path, state.preview, FILES_PREVIEW_CAPACITY, &length))
    {
        state.preview[0] = '\0';
        files_set_status(length > FILES_PREVIEW_CAPACITY ? "Preview limited to 1 KB" : "Could not read file");
        return;
    }

    state.preview[length] = '\0';
    for (uint32_t index = 0; index < length; ++index)
    {
        const char character = state.preview[index];
        if ((character < ' ' && character != '\n' && character != '\t' && character != '\r') ||
            character == '\x7f')
        {
            state.preview[index] = '.';
        }
    }
    files_set_status(length == 0 ? "Empty file" : "Text preview");
}

static bool files_load_directory(const char* path)
{
    if (path == nullptr || files_length(path) >= FILES_PATH_LIMIT)
    {
        files_set_status("Folder path is too long");
        return false;
    }

    const uint32_t sector = files_api->clawfs_get_sector(path);
    if (sector == 0)
    {
        files_set_status("Folder not found or disk not formatted");
        return false;
    }

    uint8_t buffer[512];
    if (!files_api->clawfs_read_sector(sector, buffer))
    {
        files_set_status("Could not read folder");
        return false;
    }

    files_copy(state.path, path, sizeof(state.path));
    state.entry_count = 0;
    state.selected = -1;
    state.preview[0] = '\0';

    const files_entry* entries = (const files_entry*)buffer;
    for (int index = 0; index < FILES_ENTRY_COUNT; ++index)
    {
        if (entries[index].name[0] == '\0' || entries[index].data_sector == 0)
        {
            continue;
        }
        state.entries[state.entry_count] = entries[index];
        state.entries[state.entry_count].name[FILES_NAME_LENGTH - 1] = '\0';
        ++state.entry_count;
    }

    files_set_status(state.entry_count == 0 ? "This folder is empty" : "Select a file or folder");
    return true;
}

static void files_open_selected()
{
    if (state.selected < 0 || state.selected >= state.entry_count)
    {
        return;
    }

    if (state.entries[state.selected].type != 1)
    {
        files_load_preview();
        return;
    }

    char path[256];
    if (!files_build_entry_path(path, state.entries[state.selected].name))
    {
        files_set_status("Folder path is too long");
        return;
    }
    files_load_directory(path);
}

static void files_go_up()
{
    if (state.path[0] == '/' && state.path[1] == '\0')
    {
        files_set_status("Already at the top level");
        return;
    }

    char parent[256];
    files_copy(parent, state.path, sizeof(parent));
    int length = files_length(parent);
    while (length > 1 && parent[length - 1] == '/')
    {
        parent[--length] = '\0';
    }
    while (length > 1 && parent[length - 1] != '/')
    {
        parent[--length] = '\0';
    }
    if (length > 1)
    {
        parent[length - 1] = '\0';
    }
    files_load_directory(parent);
}

static void files_draw_button(int x, int y, int width, const char* label, bool active)
{
    files_gui->fill_block(x, y, active ? FILES_SELECTED : FILES_INPUT, width, 26);
    files_gui->draw_text(label, x + 8, y + 9, active ? FILES_ACCENT : FILES_TEXT);
}

static void files_draw(napp_window* window)
{
    const int x = window->pos_x;
    const int y = window->pos_y + window->title_height;
    const int content_width = window->width;
    const int content_height = window->height - window->title_height;
    if (content_width <= 0 || content_height <= 0)
    {
        return;
    }

    files_gui->fill_block(x, y, FILES_BACKGROUND, content_width, content_height);

    files_draw_button(x + 12, y + 10, 54, "Up", true);
    files_draw_button(x + 72, y + 10, 78, "Refresh", false);
    files_gui->fill_block(x + 160, y + 10, FILES_INPUT, content_width - 172, 26);
    files_gui->draw_text(state.path, x + 170, y + 19, FILES_TEXT);

    const int sidebar_x = x + 12;
    const int sidebar_y = y + 48;
    const int sidebar_width = 132;
    const int list_x = x + 156;
    const int list_y = y + 48;
    const int list_width = content_width - 168;
    files_gui->fill_block(sidebar_x, sidebar_y, FILES_SURFACE, sidebar_width, content_height - 60);
    files_gui->fill_block(list_x, list_y, FILES_SURFACE, list_width, content_height - 60);

    files_gui->draw_text("LOCATIONS", sidebar_x + 10, sidebar_y + 10, FILES_MUTED);
    files_gui->draw_text("Home", sidebar_x + 12, sidebar_y + 38, FILES_TEXT);
    files_gui->draw_text("Documents", sidebar_x + 12, sidebar_y + 66, FILES_TEXT);
    files_gui->draw_text("Downloads", sidebar_x + 12, sidebar_y + 94, FILES_TEXT);
    files_gui->draw_text("Root", sidebar_x + 12, sidebar_y + 122, FILES_TEXT);

    files_gui->draw_text("NAME", list_x + 12, list_y + 10, FILES_MUTED);
    files_gui->draw_text("TYPE", list_x + list_width - 90, list_y + 10, FILES_MUTED);
    files_gui->fill_block(list_x + 8, list_y + 29, FILES_INPUT, list_width - 16, 1);

    const int row_height = 24;
    const int rows_y = list_y + 36;
    const int visible_rows = (content_height - 110) / row_height;
    for (int index = 0; index < state.entry_count && index < visible_rows; ++index)
    {
        const int row_y = rows_y + index * row_height;
        if (index == state.selected)
        {
            files_gui->fill_block(list_x + 4, row_y - 3, FILES_SELECTED, list_width - 8, row_height);
        }
        const bool directory = state.entries[index].type == 1;
        files_gui->draw_text(directory ? "[D]" : "[F]", list_x + 12, row_y + 3,
                             directory ? FILES_ACCENT : FILES_MUTED);
        files_gui->draw_text(state.entries[index].name, list_x + 42, row_y + 3, FILES_TEXT);
        files_gui->draw_text(directory ? "Folder" : "File", list_x + list_width - 90,
                             row_y + 3, FILES_MUTED);
    }

    const int footer_y = y + content_height - 38;
    files_gui->fill_block(x + 12, footer_y - 6, FILES_SURFACE, content_width - 24, 32);
    if (state.preview[0] == '\0')
    {
        files_gui->draw_text(state.status, x + 22, footer_y + 2, FILES_MUTED);
    }
    else
    {
        files_gui->draw_text("Preview:", x + 22, footer_y + 2, FILES_ACCENT);
        const int preview_x = x + 92;
        const int preview_width = (content_width - 110) / 8;
        int preview_line = 0;
        int preview_column = 0;
        char line[64];
        for (int index = 0; state.preview[index] != '\0' && preview_line < 2; ++index)
        {
            const char character = state.preview[index];
            if (character == '\n' || character == '\r' || preview_column >= preview_width ||
                preview_column >= (int)sizeof(line) - 1)
            {
                line[preview_column] = '\0';
                if (line[0] != '\0')
                {
                    files_gui->draw_text(line, preview_x, footer_y + 2 + preview_line * 12, FILES_TEXT);
                }
                ++preview_line;
                preview_column = 0;
                if (character == '\n' || character == '\r')
                {
                    continue;
                }
                if (preview_line >= 2)
                {
                    break;
                }
            }
            line[preview_column++] = character;
        }
        if (preview_line < 2 && preview_column > 0)
        {
            line[preview_column] = '\0';
            files_gui->draw_text(line, preview_x, footer_y + 2 + preview_line * 12, FILES_TEXT);
        }
    }
}

static void files_key(napp_window*, char key)
{
    if (key == NAPP_KEY_F5)
    {
        files_load_directory(state.path);
    }
    else if (key == '\b' || key == 127)
    {
        files_go_up();
    }
    else if (key == 'j' || key == 's' || key == '2')
    {
        if (state.entry_count > 0)
        {
            state.selected = state.selected < 0 ? 0 : (state.selected + 1) % state.entry_count;
            files_load_preview();
        }
    }
    else if (key == 'k' || key == 'w' || key == '8')
    {
        if (state.entry_count > 0)
        {
            state.selected = state.selected <= 0 ? state.entry_count - 1 : state.selected - 1;
            files_load_preview();
        }
    }
    else if (key == '\n' || key == '\r')
    {
        files_open_selected();
    }
}

static void files_mouse(napp_window* window, int mouse_x, int mouse_y)
{
    const int x = window->pos_x;
    const int y = window->pos_y + window->title_height;
    const int relative_x = mouse_x - x;
    const int relative_y = mouse_y - y;

    if (relative_y >= 10 && relative_y < 36)
    {
        if (relative_x >= 12 && relative_x < 66)
        {
            files_go_up();
            return;
        }
        if (relative_x >= 72 && relative_x < 150)
        {
            files_load_directory(state.path);
            return;
        }
    }

    const int sidebar_top = 48;
    if (relative_x >= 12 && relative_x < 144 && relative_y >= sidebar_top + 28)
    {
        const int location = (relative_y - sidebar_top - 28) / 28;
        if (location == 0)
        {
            files_load_directory("/home/user");
        }
        else if (location == 1)
        {
            files_load_directory("/home/user/documents");
        }
        else if (location == 2)
        {
            files_load_directory("/home/user/downloads");
        }
        else if (location == 3)
        {
            files_load_directory("/");
        }
        return;
    }

    const int list_x = 156;
    const int list_y = 48;
    const int row_y = list_y + 36;
    const int row_height = 24;
    if (relative_x < list_x || relative_y < row_y)
    {
        return;
    }

    const int selected = (relative_y - row_y) / row_height;
    if (selected < 0 || selected >= state.entry_count || selected >= FILES_ENTRY_COUNT)
    {
        return;
    }

    if (state.selected == selected)
    {
        const uint64_t ticks = files_api->get_ticks != nullptr ? files_api->get_ticks() : 0;
        if (state.previous_click == selected && ticks - state.previous_click_tick <= 40)
        {
            files_open_selected();
        }
        else
        {
            files_load_preview();
        }
        state.previous_click = selected;
        state.previous_click_tick = ticks;
    }
    else
    {
        state.selected = selected;
        state.previous_click = selected;
        state.previous_click_tick = files_api->get_ticks != nullptr ? files_api->get_ticks() : 0;
        files_load_preview();
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr ||
        api->clawfs_get_sector == nullptr || api->clawfs_read_sector == nullptr ||
        api->clawfs_read_file == nullptr)
    {
        if (api != nullptr && api->print_error != nullptr)
        {
            api->print_error("Files requires NAPP GUI and ClawFS services.\n");
        }
        return 1;
    }

    files_api = api;
    files_gui = api->gui;
    state = {};
    state.selected = -1;
    state.previous_click = -1;
    files_load_directory("/home/user");

    napp_window_config config = {};
    config.title = "Files";
    config.width = 680;
    config.height = 470;
    config.resizable = true;
    config.can_maximize = true;
    config.userdata = &state;
    config.draw = files_draw;
    config.key = files_key;
    config.mouse = files_mouse;

    if (!files_gui->open_window(&config))
    {
        api->print_error("Files could not open its window.\n");
        return 1;
    }
    return 0;
}
