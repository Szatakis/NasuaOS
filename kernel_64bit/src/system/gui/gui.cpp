#include "gui.hpp"

#include "./icons/icons.hpp"
#include "drivers/gpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/keyboard/driver.hpp"
#include "drivers/mouse/driver.hpp"
#include "drivers/disk/driver.hpp"
#include "drivers/timer/driver.hpp"
#include "./vars/colors.hpp"
#include "system/applications/napp/napp.hpp"
#include "system/authentication/authentication.hpp"
#include "system/filesystem/clawfs/clawfs.hpp"
#include "libs/libc/libc.hpp"

static constexpr uint32_t DESKTOP_ENTRY_COUNT = 12;
static constexpr uint64_t DESKTOP_REFRESH_TICKS = 100;
static constexpr int DESKTOP_ICON_COLUMNS = 5;
static constexpr int DESKTOP_ICON_WIDTH = 112;
static constexpr int DESKTOP_ICON_HEIGHT = 104;
static constexpr int DESKTOP_ICON_TOP = 160;
static constexpr int DESKTOP_MENU_ROW_HEIGHT = 22;

enum desktop_prompt_kind
{
    DESKTOP_PROMPT_NONE,
    DESKTOP_PROMPT_NEW_FILE,
    DESKTOP_PROMPT_NEW_FOLDER,
    DESKTOP_PROMPT_RENAME,
    DESKTOP_PROMPT_DELETE
};

static CLAWFSEntry desktop_entries[DESKTOP_ENTRY_COUNT];
static uint32_t desktop_entry_count = 0;
static char desktop_directory[256] = {};
static char desktop_selected_path[256] = {};
static char desktop_prompt_text[28] = {};
static char desktop_prompt_target_name[28] = {};
static char desktop_status[96] = {};
static int desktop_selected_entry = -1;
static uint32_t desktop_prompt_target_type = CLAWFS_FILE;
static int desktop_menu_x = 0;
static int desktop_menu_y = 0;
static int desktop_menu_submenu = 0;
static int desktop_last_click_entry = -1;
static uint64_t desktop_last_click_tick = 0;
static uint64_t desktop_last_refresh_tick = 0;
static desktop_prompt_kind desktop_prompt = DESKTOP_PROMPT_NONE;
static bool desktop_menu_open = false;
static bool desktop_properties_open = false;

static bool desktop_build_path(char* output, size_t capacity, const char* parent, const char* name)
{
    if (output == nullptr || parent == nullptr || name == nullptr || capacity == 0)
    {
        return false;
    }

    const size_t parent_length = strlen(parent);
    const size_t name_length = strlen(name);
    const bool needs_separator = parent_length > 0 && parent[parent_length - 1] != '/';
    const size_t required = parent_length + (needs_separator ? 1 : 0) + name_length + 1;
    if (required > capacity)
    {
        return false;
    }

    strcpy(output, parent);
    if (needs_separator)
    {
        strcat(output, "/");
    }
    strcat(output, name);
    return true;
}

static bool desktop_build_parent_path(char* output, size_t capacity, const char* path)
{
    if (output == nullptr || path == nullptr || path[0] != '/')
    {
        return false;
    }

    size_t length = strlen(path);
    while (length > 1 && path[length - 1] == '/')
    {
        --length;
    }
    size_t separator = length;
    while (separator > 0 && path[separator - 1] != '/')
    {
        --separator;
    }
    if (separator == 0 || separator > capacity)
    {
        return false;
    }
    if (separator == 1)
    {
        strcpy(output, "/");
        return true;
    }
    for (size_t index = 0; index < separator - 1; ++index)
    {
        output[index] = path[index];
    }
    output[separator - 1] = '\0';
    return true;
}

static bool desktop_valid_name(const char* name)
{
    if (name == nullptr || name[0] == '\0' || strlen(name) >= sizeof(desktop_prompt_text) ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        return false;
    }

    for (size_t index = 0; name[index] != '\0'; ++index)
    {
        if (name[index] == '/' || name[index] == '\\')
        {
            return false;
        }
    }
    return true;
}

static bool desktop_entry_name_valid(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }
    bool terminated = false;
    for (size_t index = 0; index < sizeof(CLAWFSEntry::name); ++index)
    {
        if (name[index] == '\0')
        {
            terminated = true;
            break;
        }
    }
    return terminated && desktop_valid_name(name);
}

static void desktop_refresh_entries(bool force)
{
    const char* username = auth_current_username();
    if (username == nullptr)
    {
        desktop_entry_count = 0;
        desktop_directory[0] = '\0';
        return;
    }

    char home_path[256];
    char path[256];
    if (!desktop_build_path(home_path, sizeof(home_path), "/home", username) ||
        !desktop_build_path(path, sizeof(path), home_path, "desktop"))
    {
        desktop_entry_count = 0;
        return;
    }

    const uint64_t ticks = Timer::pit_get_ticks();
    const bool directory_changed = strcmp(path, desktop_directory) != 0;
    if (directory_changed)
    {
        desktop_selected_entry = -1;
        desktop_selected_path[0] = '\0';
        desktop_last_click_entry = -1;
        desktop_menu_open = false;
        desktop_prompt = DESKTOP_PROMPT_NONE;
        desktop_properties_open = false;
    }
    if (!force && strcmp(path, desktop_directory) == 0 &&
        ticks - desktop_last_refresh_tick < DESKTOP_REFRESH_TICKS)
    {
        return;
    }

    strcpy(desktop_directory, path);
    desktop_last_refresh_tick = ticks;
    desktop_entry_count = 0;
    if (!auth_check_path_permission(home_path, false))
    {
        return;
    }

    const uint32_t home_sector = get_sector_by_path(home_path);
    if (home_sector == 0)
    {
        return;
    }

    CLAWFSEntry desktop_directory_entry;
    uint32_t directory_sector = find_entry_in_dir(home_sector, "desktop", &desktop_directory_entry);
    if (directory_sector == 0)
    {
        if (!auth_check_path_permission(home_path, true))
        {
            return;
        }
        clawfs_mkdir(home_path, "desktop");
        directory_sector = find_entry_in_dir(home_sector, "desktop", &desktop_directory_entry);
    }
    if (directory_sector == 0 || desktop_directory_entry.type != CLAWFS_DIRECTORY ||
        !auth_check_path_permission(desktop_directory, false))
    {
        return;
    }
    directory_sector = desktop_directory_entry.data_sector;
    uint8_t buffer[512];
    if (!Disk::read_sector(directory_sector, buffer))
    {
        return;
    }

    const CLAWFSEntry* entries = reinterpret_cast<const CLAWFSEntry*>(buffer);
    for (uint32_t index = 0; index < DESKTOP_ENTRY_COUNT; ++index)
    {
        if ((entries[index].type != CLAWFS_FILE && entries[index].type != CLAWFS_DIRECTORY) ||
            !desktop_entry_name_valid(entries[index].name))
        {
            continue;
        }

        char entry_path[256];
        if (!desktop_build_path(entry_path, sizeof(entry_path), desktop_directory, entries[index].name) ||
            !auth_check_path_permission(entry_path, false))
        {
            continue;
        }

        desktop_entries[desktop_entry_count++] = entries[index];
    }

    if (desktop_selected_entry >= static_cast<int>(desktop_entry_count))
    {
        desktop_selected_entry = -1;
        desktop_selected_path[0] = '\0';
    }
}

static void desktop_set_status(const char* message)
{
    if (message == nullptr)
    {
        desktop_status[0] = '\0';
        return;
    }
    size_t index = 0;
    while (message[index] != '\0' && index + 1 < sizeof(desktop_status))
    {
        desktop_status[index] = message[index];
        ++index;
    }
    desktop_status[index] = '\0';
}

static void desktop_open_selected(bool with_file_explorer)
{
    if (desktop_selected_path[0] == '\0' || desktop_selected_entry < 0 ||
        desktop_selected_entry >= static_cast<int>(desktop_entry_count))
    {
        return;
    }

    const int type = desktop_entries[desktop_selected_entry].type == CLAWFS_DIRECTORY
        ? CLAWFS_DIRECTORY
        : CLAWFS_FILE;

    bool started = false;
    if (type == CLAWFS_DIRECTORY)
    {
        started = napp_open_directory_in_file_explorer(desktop_selected_path);
    }
    else if (with_file_explorer)
    {
        char parent[256];
        started = desktop_build_parent_path(parent, sizeof(parent), desktop_selected_path) &&
            napp_open_directory_in_file_explorer(parent);
    }
    else
    {
        started = napp_open_file_in_suaedit(desktop_selected_path);
    }

    desktop_set_status(started ? "Opened" : "Could not open this item");
    desktop_menu_open = false;
    desktop_menu_submenu = 0;
}

static void desktop_start_prompt(desktop_prompt_kind kind)
{
    desktop_prompt = kind;
    desktop_prompt_text[0] = '\0';
    desktop_prompt_target_name[0] = '\0';
    if ((kind == DESKTOP_PROMPT_RENAME || kind == DESKTOP_PROMPT_DELETE) &&
        desktop_selected_entry >= 0 &&
        desktop_selected_entry < static_cast<int>(desktop_entry_count))
    {
        strcpy(desktop_prompt_target_name, desktop_entries[desktop_selected_entry].name);
        desktop_prompt_target_type = desktop_entries[desktop_selected_entry].type;
    }
    desktop_menu_open = false;
    desktop_menu_submenu = 0;
    desktop_properties_open = false;
}

static bool desktop_directory_is_full()
{
    if (!auth_check_path_permission(desktop_directory, true))
    {
        return false;
    }

    const uint32_t directory_sector = get_sector_by_path(desktop_directory);
    uint8_t buffer[512];
    if (directory_sector == 0 || !Disk::read_sector(directory_sector, buffer))
    {
        return false;
    }

    const CLAWFSEntry* entries = reinterpret_cast<const CLAWFSEntry*>(buffer);
    for (uint32_t index = 0; index < DESKTOP_ENTRY_COUNT; ++index)
    {
        if (entries[index].name[0] == '\0')
        {
            return false;
        }
    }
    return true;
}

static void desktop_report_directory_full()
{
    desktop_prompt = DESKTOP_PROMPT_NONE;
    desktop_refresh_entries(true);
    if (napp_print_to_terminal("Directory is full!\n"))
    {
        desktop_set_status(nullptr);
    }
    else
    {
        desktop_set_status("Directory is full; could not open Terminal");
    }
}

static void desktop_finish_prompt()
{
    if (desktop_prompt == DESKTOP_PROMPT_DELETE)
    {
        desktop_prompt = DESKTOP_PROMPT_NONE;
        if (desktop_prompt_text[0] == 'y' || desktop_prompt_text[0] == 'Y')
        {
            if (desktop_prompt_target_name[0] != '\0')
            {
                char deleted_path[256];
                desktop_build_path(deleted_path, sizeof(deleted_path),
                                   desktop_directory, desktop_prompt_target_name);
                clawfs_rm(desktop_directory, desktop_prompt_target_name, desktop_prompt_target_type);
                desktop_refresh_entries(true);
                desktop_set_status(get_sector_by_path(deleted_path) == 0
                    ? "Deleted" : "Delete failed; check permissions or folder contents");
            }
        }
        else
        {
            desktop_set_status("Delete cancelled");
        }
        return;
    }

    if (desktop_prompt_text[0] == '\0')
    {
        desktop_prompt = DESKTOP_PROMPT_NONE;
        return;
    }

    if (!desktop_valid_name(desktop_prompt_text))
    {
        desktop_set_status("Invalid name (use 1-27 characters)");
        return;
    }

    bool succeeded = false;
    if (desktop_prompt == DESKTOP_PROMPT_NEW_FILE)
    {
        char target[256];
        if (!desktop_build_path(target, sizeof(target), desktop_directory, desktop_prompt_text) ||
            get_sector_by_path(target) != 0)
        {
            desktop_set_status("Name already exists or path is too long");
        }
        else
        {
            if (desktop_directory_is_full())
            {
                desktop_report_directory_full();
                return;
            }
            clawfs_create_file_in(desktop_directory, desktop_prompt_text);
            succeeded = get_sector_by_path(target) != 0;
        }
    }
    else if (desktop_prompt == DESKTOP_PROMPT_NEW_FOLDER)
    {
        char target[256];
        if (!desktop_build_path(target, sizeof(target), desktop_directory, desktop_prompt_text) ||
            get_sector_by_path(target) != 0)
        {
            desktop_set_status("Name already exists or path is too long");
        }
        else
        {
            if (desktop_directory_is_full())
            {
                desktop_report_directory_full();
                return;
            }
            clawfs_mkdir(desktop_directory, desktop_prompt_text);
            succeeded = get_sector_by_path(target) != 0;
        }
    }
    else if (desktop_prompt == DESKTOP_PROMPT_RENAME && desktop_prompt_target_name[0] != '\0')
    {
        succeeded = clawfs_rename(desktop_directory,
                                  desktop_prompt_target_name,
                                  desktop_prompt_text,
                                  desktop_prompt_target_type);
        if (succeeded)
        {
            desktop_build_path(desktop_selected_path, sizeof(desktop_selected_path),
                               desktop_directory, desktop_prompt_text);
        }
    }

    desktop_prompt = DESKTOP_PROMPT_NONE;
    desktop_refresh_entries(true);
    if (succeeded)
    {
        desktop_set_status("Operation completed");
    }
    else if (desktop_status[0] == '\0')
    {
        desktop_set_status("Operation failed; check permissions or available space");
    }
}

static const char* desktop_menu_item(int index)
{
    static const char* const item_menu[] = {
        "Open", "Open with...", "New...", "Rename", "Delete", "Properties"
    };
    static const char* const blank_menu[] = { "New..." };
    static const char* const new_menu[] = { "File", "Folder" };
    static const char* const open_with_menu[] = { "SuaEdit", "File Explorer" };

    if (desktop_menu_submenu == 1)
    {
        return index >= 0 && index < 2 ? new_menu[index] : nullptr;
    }
    if (desktop_menu_submenu == 2)
    {
        if (desktop_selected_entry >= 0 &&
            desktop_entries[desktop_selected_entry].type == CLAWFS_DIRECTORY)
        {
            return index == 0 ? open_with_menu[1] : nullptr;
        }
        return index >= 0 && index < 2 ? open_with_menu[index] : nullptr;
    }
    if (desktop_selected_entry < 0)
    {
        return index == 0 ? blank_menu[0] : nullptr;
    }
    return index >= 0 && index < 6 ? item_menu[index] : nullptr;
}

static int desktop_menu_item_count()
{
    if (desktop_menu_submenu == 1)
    {
        return 2;
    }
    if (desktop_menu_submenu == 2)
    {
        return desktop_selected_entry >= 0 &&
            desktop_entries[desktop_selected_entry].type == CLAWFS_DIRECTORY ? 1 : 2;
    }
    return desktop_selected_entry < 0 ? 1 : 6;
}

static int desktop_icon_x(int index)
{
    return 24 + (index % DESKTOP_ICON_COLUMNS) * DESKTOP_ICON_WIDTH;
}

static int desktop_icon_y(int index)
{
    return DESKTOP_ICON_TOP +
        (index / DESKTOP_ICON_COLUMNS) * DESKTOP_ICON_HEIGHT;
}

static int desktop_entry_at(int x, int y)
{
    for (uint32_t index = 0; index < desktop_entry_count; ++index)
    {
        const int icon_x = desktop_icon_x(static_cast<int>(index));
        const int icon_y = desktop_icon_y(static_cast<int>(index));
        if (x >= icon_x && x < icon_x + 88 && y >= icon_y && y < icon_y + 88)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool last_start_hover = false;

const int panel_width = 156;
static constexpr uint32_t START_MENU_MAX_WIDTH = 620;
static constexpr uint32_t START_MENU_MAX_HEIGHT = 400;
uint32_t menu_w = START_MENU_MAX_WIDTH;
uint32_t menu_h = START_MENU_MAX_HEIGHT;
uint32_t menu_x = 0;
uint32_t menu_y = 0;

extern bool start_hover;

static uint32_t start_menu_width()
{
    const uint32_t responsive_width = Gpu::fb->width / 2;
    return responsive_width < START_MENU_MAX_WIDTH
        ? responsive_width
        : START_MENU_MAX_WIDTH;
}

static void update_start_menu_size()
{
    menu_w = start_menu_width();
    const uint32_t available_height = Gpu::fb->height > bar_h_scaled
        ? Gpu::fb->height - bar_h_scaled
        : 0;
    menu_h = available_height < START_MENU_MAX_HEIGHT
        ? available_height
        : START_MENU_MAX_HEIGHT;
    menu_x = 0;
    menu_y = available_height - menu_h;
}

void draw_start_menu_f(int x, int y, int w, int h)
{
    Gpu::draw_rect(x, y, x + w, y + h, COLOR_NASUA_START_MENU);
    Gpu::draw_rect(x, y, x + panel_width, y + h, COLOR_NASUA_START_MENU_P);

    draw_start_menu_contents(x, y, w, h);
}

void draw_start_menu() 
{
    if (!Gpu::fb || !menu_start_open) 
    {
        return;
    }

    update_start_menu_size();

    draw_start_menu_f(menu_x, menu_y, menu_w, menu_h);
}

void open_start_menu() 
{
    if (!Gpu::fb) 
    {
        return;
    }

    update_start_menu_size();
    desktop_dismiss_overlays();
    menu_start_open = true;
    reset_start_menu_navigation();
}

void close_start_menu() 
{
    menu_start_open = false;
}

void update_gui_state(int mouse_x, int mouse_y) 
{
    if (is_mouse_over_start(mouse_x, mouse_y)) 
    {
        start_hover = true;
    }
    else 
    {
        start_hover = false;
    }
}

void update_windows_gui() 
{
    Gpu::Window_Manager::update_windows_positions(Mouse::x, Mouse::y);
    Gpu::Window_Manager::draw_windows(); 
}

void draw_desktop_icons()
{
    if (Gpu::fb == nullptr || auth_current_username() == nullptr)
    {
        return;
    }

    desktop_refresh_entries(false);
    for (uint32_t index = 0; index < desktop_entry_count; ++index)
    {
        const int x = desktop_icon_x(static_cast<int>(index));
        const int y = desktop_icon_y(static_cast<int>(index));
        if (static_cast<int>(index) == desktop_selected_entry)
        {
            Gpu::fill_block(x, y, 0x605B86D6, 88, 82);
        }

        if (desktop_entries[index].type == CLAWFS_DIRECTORY)
        {
            Gpu::fill_block(x + 18, y + 14, 0xFFE5B84B, 26, 8);
            Gpu::fill_block(x + 12, y + 20, 0xFFFFD66E, 42, 26);
            Gpu::fill_block(x + 12, y + 20, 0xFFF0B943, 42, 3);
        }
        else
        {
            Gpu::fill_block(x + 19, y + 10, 0xFFF2F4F8, 30, 39);
            Gpu::fill_block(x + 19, y + 10, 0xFFB7C4D5, 30, 3);
            Gpu::fill_block(x + 24, y + 22, 0xFF75869B, 19, 2);
            Gpu::fill_block(x + 24, y + 28, 0xFF75869B, 19, 2);
            Gpu::fill_block(x + 24, y + 34, 0xFF75869B, 15, 2);
        }
        Gpu::print_at8_clipped(desktop_entries[index].name, x - 10, y + 60,
                               0xFFFFFFFF, x - 14, x + 96);
    }
}

void draw_desktop_overlay()
{
    if (Gpu::fb == nullptr || auth_current_username() == nullptr || menu_start_open)
    {
        return;
    }

    if (desktop_menu_open)
    {
        const int row_count = desktop_menu_item_count();
        const int menu_width = 156;
        const int menu_height = row_count * DESKTOP_MENU_ROW_HEIGHT + 8;
        const int x = desktop_menu_x + menu_width > static_cast<int>(Gpu::fb->width)
            ? static_cast<int>(Gpu::fb->width) - menu_width
            : desktop_menu_x;
        const int y = desktop_menu_y + menu_height > static_cast<int>(Gpu::fb->height) - static_cast<int>(bar_h_scaled)
            ? static_cast<int>(Gpu::fb->height) - static_cast<int>(bar_h_scaled) - menu_height
            : desktop_menu_y;
        Gpu::fill_block(x, y, 0xF51D2634, menu_width, menu_height);
        Gpu::draw_rect(x, y, x + menu_width, y + menu_height, 0xFF71829A);
        for (int item = 0; item < row_count; ++item)
        {
            const char* label = desktop_menu_item(item);
            if (label != nullptr)
            {
                Gpu::print_at8_clipped(label, x + 12, y + 7 + item * DESKTOP_MENU_ROW_HEIGHT,
                                       0xFFF4F6FA, x + 8, x + menu_width - 8);
            }
        }
        desktop_menu_x = x;
        desktop_menu_y = y;
    }

    if (desktop_prompt != DESKTOP_PROMPT_NONE)
    {
        const int width = 340;
        const int height = 112;
        const int x = (static_cast<int>(Gpu::fb->width) - width) / 2;
        const int y = (static_cast<int>(Gpu::fb->height) - height) / 2;
        const char* title = desktop_prompt == DESKTOP_PROMPT_NEW_FILE ? "Create file" :
            desktop_prompt == DESKTOP_PROMPT_NEW_FOLDER ? "Create folder" :
            desktop_prompt == DESKTOP_PROMPT_RENAME ? "Rename item" : "Confirm delete";
        Gpu::fill_block(x, y, 0xF51D2634, width, height);
        Gpu::draw_rect(x, y, x + width, y + height, 0xFF71829A);
        Gpu::print_at8_clipped(title, x + 16, y + 14, 0xFFFFFFFF, x + 12, x + width - 12);
        if (desktop_prompt == DESKTOP_PROMPT_DELETE)
        {
            Gpu::print_at8_clipped("Type Y to delete; any other key cancels", x + 16, y + 42,
                                   0xFFCBD5E1, x + 12, x + width - 12);
        }
        else
        {
            Gpu::fill_block(x + 14, y + 42, 0xFF101722, width - 28, 26);
            Gpu::print_at8_clipped(desktop_prompt_text, x + 20, y + 50, 0xFFFFFFFF,
                                   x + 18, x + width - 18);
            Gpu::print_at8_clipped("Enter: confirm    Esc: cancel", x + 16, y + 80,
                                   0xFFCBD5E1, x + 12, x + width - 12);
        }
    }

    if (desktop_properties_open && desktop_selected_entry >= 0)
    {
        const int x = 20;
        const int y = 20;
        const int width = 280;
        const int height = 92;
        char size_text[32] = {};
        uint32_t size = 0;
        if (desktop_entries[desktop_selected_entry].type == CLAWFS_FILE)
        {
            const bool has_size = clawfs_get_file_size(desktop_selected_path, &size);
            uint32_t value = size;
            int position = sizeof(size_text) - 1;
            size_text[position--] = '\0';
            do
            {
                size_text[position--] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && position > 0);
            const char* number = &size_text[position + 1];
            Gpu::fill_block(x, y, 0xF51D2634, width, height);
            Gpu::draw_rect(x, y, x + width, y + height, 0xFF71829A);
            Gpu::print_at8_clipped(desktop_entries[desktop_selected_entry].name,
                                   x + 12, y + 12, 0xFFFFFFFF, x + 8, x + width - 8);
            char properties[64] = "File, ";
            strcat(properties, has_size ? number : "unknown size");
            strcat(properties, " bytes");
            Gpu::print_at8_clipped(properties, x + 12, y + 40, 0xFFCBD5E1,
                                   x + 8, x + width - 8);
        }
        else
        {
            Gpu::fill_block(x, y, 0xF51D2634, width, height);
            Gpu::draw_rect(x, y, x + width, y + height, 0xFF71829A);
            Gpu::print_at8_clipped(desktop_entries[desktop_selected_entry].name,
                                   x + 12, y + 12, 0xFFFFFFFF, x + 8, x + width - 8);
            Gpu::print_at8_clipped("Folder", x + 12, y + 40, 0xFFCBD5E1,
                                   x + 8, x + width - 8);
        }
    }

    if (desktop_status[0] != '\0')
    {
        Gpu::print_at8_clipped(desktop_status, 12,
            Gpu::fb->height > bar_h_scaled + 18 ? Gpu::fb->height - bar_h_scaled - 18 : 0,
            0xFFFFFFFF, 8, Gpu::fb->width - 8);
    }
}

bool desktop_has_keyboard_focus()
{
    return auth_current_username() != nullptr && !menu_start_open &&
        (desktop_prompt != DESKTOP_PROMPT_NONE || desktop_menu_open || desktop_properties_open);
}

bool desktop_handle_key(char key)
{
    if (!desktop_has_keyboard_focus())
    {
        return false;
    }

    if (key == NAPP_KEY_ESCAPE)
    {
        desktop_prompt = DESKTOP_PROMPT_NONE;
        desktop_menu_open = false;
        desktop_menu_submenu = 0;
        desktop_properties_open = false;
        return true;
    }

    if (desktop_prompt == DESKTOP_PROMPT_DELETE)
    {
        desktop_prompt_text[0] = key == 'y' || key == 'Y' ? key : '\0';
        desktop_finish_prompt();
        return true;
    }

    if (desktop_prompt != DESKTOP_PROMPT_NONE)
    {
        if (key == '\n' || key == '\r')
        {
            desktop_finish_prompt();
        }
        else if (key == '\b' || key == 127)
        {
            const size_t length = strlen(desktop_prompt_text);
            if (length > 0)
            {
                desktop_prompt_text[length - 1] = '\0';
            }
        }
        else if (key >= 32 && key <= 126 && key != '/' && key != '\\')
        {
            const size_t length = strlen(desktop_prompt_text);
            if (length + 1 < sizeof(desktop_prompt_text))
            {
                desktop_prompt_text[length] = key;
                desktop_prompt_text[length + 1] = '\0';
            }
        }
        return true;
    }

    if (desktop_menu_open)
    {
        if (key == '\n' || key == '\r')
        {
            return desktop_handle_mouse_click(desktop_menu_x + 14,
                desktop_menu_y + 8 + DESKTOP_MENU_ROW_HEIGHT / 2);
        }
        return true;
    }

    if (desktop_properties_open)
    {
        desktop_properties_open = false;
        return true;
    }
    return false;
}

bool desktop_handle_mouse_click(int mouse_x, int mouse_y)
{
    if (auth_current_username() == nullptr)
    {
        return false;
    }

    if (desktop_prompt != DESKTOP_PROMPT_NONE)
    {
        return true;
    }

    if (desktop_properties_open)
    {
        desktop_properties_open = false;
        return true;
    }

    if (desktop_menu_open)
    {
        const int relative_y = mouse_y - desktop_menu_y - 4;
        const int row = relative_y / DESKTOP_MENU_ROW_HEIGHT;
        if (mouse_x >= desktop_menu_x && mouse_x < desktop_menu_x + 156 &&
            relative_y >= 0 && row >= 0 && row < desktop_menu_item_count())
        {
            if (desktop_menu_submenu == 1)
            {
                desktop_start_prompt(row == 0 ? DESKTOP_PROMPT_NEW_FILE : DESKTOP_PROMPT_NEW_FOLDER);
                return true;
            }
            if (desktop_menu_submenu == 2)
            {
                desktop_open_selected(row != 0);
                return true;
            }
            if (desktop_selected_entry < 0)
            {
                desktop_menu_submenu = 1;
                return true;
            }

            switch (row)
            {
            case 0:
                desktop_open_selected(false);
                break;
            case 1:
                desktop_menu_submenu = 2;
                break;
            case 2:
                desktop_menu_submenu = 1;
                break;
            case 3:
                desktop_prompt_text[0] = '\0';
                desktop_start_prompt(DESKTOP_PROMPT_RENAME);
                break;
            case 4:
                desktop_prompt_text[0] = '\0';
                desktop_start_prompt(DESKTOP_PROMPT_DELETE);
                break;
            case 5:
                desktop_menu_open = false;
                desktop_properties_open = true;
                break;
            default:
                break;
            }
            return true;
        }
        desktop_menu_open = false;
        desktop_menu_submenu = 0;
        return true;
    }

    desktop_refresh_entries(false);
    const int entry = desktop_entry_at(mouse_x, mouse_y);
    if (entry < 0)
    {
        desktop_selected_entry = -1;
        desktop_selected_path[0] = '\0';
        desktop_last_click_entry = -1;
        desktop_set_status(nullptr);
        return false;
    }

    desktop_selected_entry = entry;
    desktop_build_path(desktop_selected_path, sizeof(desktop_selected_path),
                       desktop_directory, desktop_entries[entry].name);
    const uint64_t ticks = Timer::pit_get_ticks();
    if (desktop_last_click_entry == entry && ticks - desktop_last_click_tick <= 40)
    {
        desktop_open_selected(false);
        desktop_last_click_entry = -1;
    }
    else
    {
        desktop_last_click_entry = entry;
        desktop_last_click_tick = ticks;
    }
    desktop_set_status(nullptr);
    return true;
}

void desktop_handle_right_click(int mouse_x, int mouse_y)
{
    if (auth_current_username() == nullptr)
    {
        return;
    }

    desktop_refresh_entries(false);
    desktop_properties_open = false;
    desktop_prompt = DESKTOP_PROMPT_NONE;
    desktop_selected_entry = desktop_entry_at(mouse_x, mouse_y);
    desktop_last_click_entry = -1;
    if (desktop_selected_entry >= 0)
    {
        desktop_build_path(desktop_selected_path, sizeof(desktop_selected_path),
                           desktop_directory, desktop_entries[desktop_selected_entry].name);
    }
    else
    {
        desktop_selected_path[0] = '\0';
    }
    desktop_menu_submenu = 0;
    desktop_menu_x = mouse_x;
    desktop_menu_y = mouse_y;
    desktop_menu_open = true;
    desktop_set_status(nullptr);
}

void desktop_dismiss_overlays()
{
    desktop_menu_open = false;
    desktop_menu_submenu = 0;
    desktop_prompt = DESKTOP_PROMPT_NONE;
    desktop_properties_open = false;
}