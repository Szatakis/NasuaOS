#include "icons.hpp"
#include "system/gui/gui.hpp"
#include "drivers/mouse/driver.hpp"
#include "system/vars/info_vars/info_vars.hpp"

#include "libs/libc/libc.hpp"

bool start_hover = false;

static constexpr int START_MENU_NAV_WIDTH = 156;
static constexpr int START_MENU_HEADER_HEIGHT = 50;
static constexpr int START_MENU_FOOTER_HEIGHT = 44;
static constexpr int START_MENU_GRID_ROW_HEIGHT = 88;
static constexpr int START_MENU_GRID_CELL_WIDTH = 112;
static constexpr int START_MENU_CATEGORY_ROW_HEIGHT = 30;
static constexpr int START_MENU_SEARCH_CAPACITY = 64;
static constexpr int START_MENU_POWER_BUTTON_WIDTH = 132;
static constexpr int START_MENU_POWER_BUTTON_GAP = 8;

static int start_menu_grid_row_height()
{
    const int available = static_cast<int>(menu_h) - START_MENU_HEADER_HEIGHT - 32 -
                          START_MENU_FOOTER_HEIGHT;
    return available > 0 && available < START_MENU_GRID_ROW_HEIGHT
        ? available
        : START_MENU_GRID_ROW_HEIGHT;
}

int start_y = static_cast<int>(Gpu::fb->height) - bar_h_scaled;
bool menu_start_open = false;

struct ButtonArea
{
    int x1, y1;
    int x2, y2;
};

static ButtonArea start_button = { 0, 0, 0, 0 };

void draw_start_button(size_t x, size_t taskbar_y)
{
    constexpr int button_w = 100;
    const int button_h = 28 + (bar_h_scaled - bar_h);
    constexpr int margin = 2;

    const int button_y = taskbar_y + (bar_h_scaled - button_h) / 2;

    start_button.x1 = static_cast<int>(x);
    start_button.y1 = button_y;
    start_button.x2 = static_cast<int>(x + button_w);
    start_button.y2 = button_y + button_h;

    Gpu::draw_rect(start_button.x1 - margin, start_button.y1, start_button.x2 + margin, start_button.y2, COLOR_SHADOW);

    Gpu::draw_rect(start_button.x1, start_button.y1 + margin, start_button.x2, start_button.y2 - margin, COLOR_HARD_SHADOW);

    if(start_hover)
    {
        draw_icon<32, 32>(start_icon_hover, x, button_y + ((bar_h_scaled - bar_h) / 2));
    }

    else
    {
        draw_icon<32, 32>(start_icon, x, button_y + ((bar_h_scaled - bar_h) / 2));
    }

    Gpu::print_at10("START", x + 38, button_y + ((button_h - 8) / 2), COLOR_WHITE);
}

char start_menu_napps[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
int start_menu_napp_count = 0;

static bool start_menu_pinned[NAPP_MAX_APPLICATIONS];
static char start_menu_app_categories[NAPP_MAX_APPLICATIONS][NAPP_CATEGORY_LENGTH];
static char start_menu_categories[NAPP_MAX_APPLICATIONS][NAPP_CATEGORY_LENGTH];
static int start_menu_category_count = 0;
static bool start_menu_data_loaded = false;
static int start_menu_selected_category = -1; // -1 pinned, -2 all applications
static int start_menu_app_scroll = 0;
static int start_menu_category_scroll = 0;
static char start_menu_search[START_MENU_SEARCH_CAPACITY];
static bool start_menu_search_focused = false;

static void load_start_menu_apps();

enum start_menu_action
{
    START_MENU_ACTION_PINNED,
    START_MENU_ACTION_ALL,
    START_MENU_ACTION_CATEGORY
};

static bool text_contains_case_insensitive(const char* text, const char* query)
{
    if (text == nullptr || query == nullptr || query[0] == '\0')
    {
        return true;
    }

    for (const char* start = text; *start != '\0'; ++start)
    {
        const char* text_char = start;
        const char* query_char = query;
        while (*text_char != '\0' && *query_char != '\0')
        {
            char left = *text_char;
            char right = *query_char;
            if (left >= 'A' && left <= 'Z')
            {
                left = static_cast<char>(left - 'A' + 'a');
            }
            if (right >= 'A' && right <= 'Z')
            {
                right = static_cast<char>(right - 'A' + 'a');
            }
            if (left != right)
            {
                break;
            }
            ++text_char;
            ++query_char;
        }
        if (*query_char == '\0')
        {
            return true;
        }
    }
    return false;
}

static int start_menu_action_count()
{
    load_start_menu_apps();
    return 2 + start_menu_category_count;
}

static start_menu_action start_menu_action_at(int index)
{
    if (index == 0)
    {
        return START_MENU_ACTION_PINNED;
    }
    if (index == 1)
    {
        return START_MENU_ACTION_ALL;
    }
    return START_MENU_ACTION_CATEGORY;
}

static const char* start_menu_action_label(int index)
{
    if (index == 0)
    {
        return "Pinned";
    }
    if (index == 1)
    {
        return "All Applications";
    }
    return start_menu_categories[index - 2];
}

static int start_menu_power_button_width(int menu_width, int action)
{
    if (menu_width < 560)
    {
        const int gap = menu_width >= 240 ? START_MENU_POWER_BUTTON_GAP : 4;
        return (menu_width - 16 - gap) / 2;
    }

    return action == START_MENU_CLICK_REBOOT
        ? START_MENU_POWER_BUTTON_WIDTH
        : menu_width - menu_width * 3 / 4;
}

static int start_menu_power_button_x(int menu_x, int menu_width, int action)
{
    if (menu_width < 560)
    {
        const int button_width = start_menu_power_button_width(menu_width, action);
        const int gap = menu_width >= 240 ? START_MENU_POWER_BUTTON_GAP : 4;
        if (action == START_MENU_CLICK_REBOOT)
        {
            return menu_x + menu_width - 8 - 2 * button_width - gap;
        }
        return menu_x + menu_width - 8 - button_width;
    }

    if (action == START_MENU_CLICK_REBOOT)
    {
        const int shutdown_x = menu_x + menu_width * 3 / 4;
        return shutdown_x - START_MENU_POWER_BUTTON_WIDTH - START_MENU_POWER_BUTTON_GAP;
    }
    return menu_x + menu_width * 3 / 4;
}

static void load_start_menu_apps()
{
    if (start_menu_data_loaded)
    {
        return;
    }

    char app_names[NAPP_MAX_APPLICATIONS][NAPP_MAX_NAME];
    const int all_count = (int)napp_list(app_names, NAPP_MAX_APPLICATIONS);
    start_menu_napp_count = 0;
    start_menu_category_count = 0;

    for (int i = 0; i < all_count; i++)
    {
        bool pinned = true;
        char category[NAPP_CATEGORY_LENGTH] = {};
        if (napp_get_start_menu_info(app_names[i], &pinned, category, sizeof(category)))
        {
            const int app_index = start_menu_napp_count++;
            strcpy(start_menu_napps[app_index], app_names[i]);
            start_menu_pinned[app_index] = pinned;
            strcpy(start_menu_app_categories[app_index], category);

            if (category[0] != '\0')
            {
                bool known_category = false;
                for (int category_index = 0; category_index < start_menu_category_count; ++category_index)
                {
                    if (strcmp(start_menu_categories[category_index], category) == 0)
                    {
                        known_category = true;
                        break;
                    }
                }
                if (!known_category && start_menu_category_count < NAPP_MAX_APPLICATIONS)
                {
                    strcpy(start_menu_categories[start_menu_category_count++], category);
                }
            }
        }
    }

    start_menu_data_loaded = true;
}

static int get_visible_apps(int app_indices[NAPP_MAX_APPLICATIONS])
{
    load_start_menu_apps();
    int count = 0;
    for (int i = 0; i < start_menu_napp_count; ++i)
    {
        bool matches_category = start_menu_search[0] != '\0' ||
            start_menu_selected_category == -2 ||
            (start_menu_selected_category == -1 && start_menu_pinned[i]);
        if (start_menu_search[0] == '\0' && start_menu_selected_category >= 0)
        {
            matches_category = strcmp(start_menu_app_categories[i],
                       start_menu_categories[start_menu_selected_category]) == 0;
        }

        if (matches_category &&
            (text_contains_case_insensitive(start_menu_napps[i], start_menu_search) ||
             text_contains_case_insensitive(start_menu_app_categories[i], start_menu_search)))
        {
            app_indices[count++] = i;
        }
    }
    return count;
}

static void draw_app_icon(const char* name, int x, int y)
{
    const uint32_t* icon = suaedit_icon;
    if (strcmp(name, "terminal") == 0)
    {
        icon = terminal_icon;
    }
    else if (strcmp(name, "settings") == 0)
    {
        icon = settings_icon;
    }
    else if (strcmp(name, "task_manager") == 0)
    {
        icon = task_manager_icon;
    }
    else if (strcmp(name, "calculator") == 0)
    {
        icon = calculator_icon;
    }
    draw_icon<32, 32>(icon, x, y);
}

void draw_start_menu_contents(int x, int y, int w, int h)
{
    load_start_menu_apps();
    Gpu::draw_rect(x, y, x + w, y + h, COLOR_NASUA_START_MENU);
    Gpu::draw_rect(x, y, x + w, y + START_MENU_HEADER_HEIGHT, COLOR_NASUA_DARK);
    Gpu::draw_rect(x + START_MENU_NAV_WIDTH - 1, y, x + START_MENU_NAV_WIDTH, y + h, COLOR_BORDER);

    Gpu::draw_rect(x + 10, y + 9, x + 40, y + 39, COLOR_BLUE);
    char user_initial[2] = { user_name[0] != '\0' ? user_name[0] : 'U', '\0' };
    if (user_initial[0] >= 'a' && user_initial[0] <= 'z')
    {
        user_initial[0] = static_cast<char>(user_initial[0] - 'a' + 'A');
    }
    Gpu::print_at10(user_initial, x + 20, y + 19, COLOR_WHITE);
    Gpu::print_at8_clipped(user_name, x + 48, y + 21, COLOR_WHITE,
                           x + 48, x + START_MENU_NAV_WIDTH - 12);

    const int search_x = x + START_MENU_NAV_WIDTH + 12;
    const int search_y = y + 9;
    const int search_right = x + w - 12;
    Gpu::draw_rect(search_x, search_y, search_right, search_y + 32,
                   start_menu_search_focused ? COLOR_NASUA_BG : COLOR_NASUA_START_MENU_P);
    Gpu::print_at8_clipped(start_menu_search[0] != '\0' ? start_menu_search : "Search applications...",
                           search_x + 10, search_y + 12,
                           start_menu_search[0] != '\0' ? COLOR_WHITE : COLOR_GRAY,
                           search_x + 10, search_right - 8);

    const int nav_x = x;
    const int nav_right = x + START_MENU_NAV_WIDTH;
    const int nav_top = y + START_MENU_HEADER_HEIGHT + 4;
    const int nav_bottom = y + h - START_MENU_FOOTER_HEIGHT - 4;
    const int visible_actions = (nav_bottom - nav_top) / START_MENU_CATEGORY_ROW_HEIGHT;
    const int action_count = start_menu_action_count();
    if (start_menu_category_scroll + visible_actions > action_count)
    {
        start_menu_category_scroll = action_count > visible_actions
            ? action_count - visible_actions
            : 0;
    }
    for (int row = 0; row < visible_actions &&
                       start_menu_category_scroll + row < action_count; ++row)
    {
        const int action_index = start_menu_category_scroll + row;
        const int row_y = nav_top + row * START_MENU_CATEGORY_ROW_HEIGHT;
        const start_menu_action action = start_menu_action_at(action_index);
        const bool selected =
            (action == START_MENU_ACTION_PINNED && start_menu_selected_category == -1) ||
            (action == START_MENU_ACTION_ALL && start_menu_selected_category == -2) ||
            (action == START_MENU_ACTION_CATEGORY &&
             start_menu_selected_category == action_index - 2);
        if (selected)
        {
            Gpu::draw_rect(nav_x, row_y, nav_right, row_y + START_MENU_CATEGORY_ROW_HEIGHT,
                           COLOR_NASUA_DARK);
        }
        Gpu::print_at8_clipped(start_menu_action_label(action_index), nav_x + 10, row_y + 10,
                               selected ? COLOR_WHITE : COLOR_LIGHT_GRAY,
                               nav_x + 10, nav_right - 6);
    }

    const int list_top = y + START_MENU_HEADER_HEIGHT + 32;
    const int list_bottom = y + h - START_MENU_FOOTER_HEIGHT;
    const int grid_width = w - START_MENU_NAV_WIDTH;
    const int columns = grid_width / START_MENU_GRID_CELL_WIDTH > 0
        ? grid_width / START_MENU_GRID_CELL_WIDTH
        : 1;
    const int grid_row_height = start_menu_grid_row_height();
    const int list_height = list_bottom - list_top;
    const int visible_rows = list_height > 0 ? list_height / grid_row_height : 0;
    int app_indices[NAPP_MAX_APPLICATIONS];
    const int visible_app_count = get_visible_apps(app_indices);
    const int visible_app_capacity = visible_rows * columns;
    const int max_scroll = visible_app_count > visible_app_capacity
        ? visible_app_count - visible_app_capacity
        : 0;
    if (start_menu_app_scroll > max_scroll)
    {
        start_menu_app_scroll = max_scroll;
    }

    for (int row = 0; row < visible_rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const int item_index = start_menu_app_scroll + row * columns + column;
            if (item_index >= visible_app_count)
            {
                break;
            }
            const int cell_x = x + START_MENU_NAV_WIDTH + column * START_MENU_GRID_CELL_WIDTH;
            const int cell_y = list_top + row * grid_row_height;
            const int cell_width = column == columns - 1
                ? x + w - cell_x
                : START_MENU_GRID_CELL_WIDTH;
            if (Mouse::x >= cell_x && Mouse::x < cell_x + cell_width &&
                Mouse::y >= cell_y && Mouse::y < cell_y + grid_row_height)
            {
                Gpu::draw_rect(cell_x + 2, cell_y + 2, cell_x + cell_width - 2,
                               cell_y + grid_row_height - 2, COLOR_NASUA_DARK);
            }
            const int icon_x = cell_x + (cell_width - 32) / 2;
            draw_app_icon(start_menu_napps[app_indices[item_index]], icon_x, cell_y + 8);
            Gpu::print_at8_clipped(start_menu_napps[app_indices[item_index]],
                                   cell_x + 4, cell_y + 52, COLOR_WHITE,
                                   cell_x + 4, cell_x + cell_width - 4);
        }
    }

    const int footer_y = y + h - START_MENU_FOOTER_HEIGHT;
    Gpu::draw_rect(x, footer_y, x + w, y + h, COLOR_NASUA_DARK);
    for (int action = START_MENU_CLICK_SHUTDOWN; action <= START_MENU_CLICK_REBOOT; ++action)
    {
        const int action_x = start_menu_power_button_x(x, w, action);
        const int action_right = action_x + start_menu_power_button_width(w, action);
        if (Mouse::x >= action_x && Mouse::x < action_right &&
            Mouse::y >= footer_y && Mouse::y < y + h)
        {
            Gpu::draw_rect(action_x + 2, footer_y + 3, action_right - 2,
                           y + h - 3, COLOR_NASUA_START_MENU);
        }
        const char icon_letter[] = { action == START_MENU_CLICK_REBOOT ? 'R' : 'P', '\0' };
        const char* label = action == START_MENU_CLICK_REBOOT ? "Restart" : "Shutdown";
        Gpu::draw_rect(action_x + 8, footer_y + 12, action_x + 28, footer_y + 32, COLOR_BLUE);
        Gpu::print_at8(icon_letter, action_x + 14, footer_y + 18, COLOR_WHITE);
        Gpu::print_at8_clipped(label, action_x + 34, footer_y + 17, COLOR_WHITE,
                               action_x + 34, action_right - 4);
    }
}

void reset_start_menu_navigation()
{
    start_menu_selected_category = -1;
    start_menu_app_scroll = 0;
    start_menu_category_scroll = 0;
    start_menu_search[0] = '\0';
    start_menu_search_focused = false;
}

void scroll_start_menu(int direction, bool categories)
{
    if (!menu_start_open || direction == 0)
    {
        return;
    }

    if (categories)
    {
        const int available_category_rows =
            (static_cast<int>(menu_h) - START_MENU_HEADER_HEIGHT - START_MENU_FOOTER_HEIGHT - 8) /
            START_MENU_CATEGORY_ROW_HEIGHT;
        const int category_rows = available_category_rows > 0 ? available_category_rows : 1;
        const int action_count = start_menu_action_count();
        const int max_scroll = action_count > category_rows
            ? action_count - category_rows
            : 0;
        start_menu_category_scroll += direction;
        if (start_menu_category_scroll < 0)
        {
            start_menu_category_scroll = 0;
        }
        else if (start_menu_category_scroll > max_scroll)
        {
            start_menu_category_scroll = max_scroll;
        }
        return;
    }

    const int list_height = static_cast<int>(menu_h) - START_MENU_HEADER_HEIGHT - 32 -
                            START_MENU_FOOTER_HEIGHT;
    const int visible_rows = list_height > 0
        ? list_height / start_menu_grid_row_height()
        : 1;
    const int columns =
        (static_cast<int>(menu_w) - START_MENU_NAV_WIDTH) / START_MENU_GRID_CELL_WIDTH;
    int app_indices[NAPP_MAX_APPLICATIONS];
    const int app_count = get_visible_apps(app_indices);
    const int capacity = visible_rows * (columns > 0 ? columns : 1);
    const int max_scroll = app_count > capacity ? app_count - capacity : 0;
    start_menu_app_scroll += direction;
    if (start_menu_app_scroll < 0)
    {
        start_menu_app_scroll = 0;
    }
    else if (start_menu_app_scroll > max_scroll)
    {
        start_menu_app_scroll = max_scroll;
    }
}

start_menu_click_result handle_start_menu_click(int mouse_x, int mouse_y)
{
    start_menu_search_focused = false;
    const int footer_y = menu_y + static_cast<int>(menu_h) - START_MENU_FOOTER_HEIGHT;
    if (mouse_y >= footer_y)
    {
        const int restart_x = start_menu_power_button_x(menu_x, menu_w, START_MENU_CLICK_REBOOT);
        if (mouse_x >= restart_x &&
            mouse_x < restart_x + start_menu_power_button_width(menu_w, START_MENU_CLICK_REBOOT))
        {
            return START_MENU_CLICK_REBOOT;
        }
        const int shutdown_x = start_menu_power_button_x(menu_x, menu_w, START_MENU_CLICK_SHUTDOWN);
        if (mouse_x >= shutdown_x &&
            mouse_x < shutdown_x + start_menu_power_button_width(menu_w, START_MENU_CLICK_SHUTDOWN))
        {
            return START_MENU_CLICK_SHUTDOWN;
        }
        return START_MENU_CLICK_HANDLED;
    }

    const int search_x = static_cast<int>(menu_x) + START_MENU_NAV_WIDTH + 12;
    const int search_y = static_cast<int>(menu_y) + 9;
    if (mouse_x >= search_x && mouse_x < static_cast<int>(menu_x + menu_w) - 12 &&
        mouse_y >= search_y && mouse_y < search_y + 32)
    {
        start_menu_search_focused = true;
        return START_MENU_CLICK_HANDLED;
    }

    const int nav_x = static_cast<int>(menu_x);
    const int nav_right = static_cast<int>(menu_x) + START_MENU_NAV_WIDTH;
    const int nav_top = static_cast<int>(menu_y) + START_MENU_HEADER_HEIGHT + 4;
    const int nav_bottom = footer_y - 4;
    if (mouse_x >= nav_x && mouse_x < nav_right &&
        mouse_y >= nav_top && mouse_y < nav_bottom)
    {
        const int row = (mouse_y - nav_top) / START_MENU_CATEGORY_ROW_HEIGHT;
        const int action_index = start_menu_category_scroll + row;
        if (action_index < start_menu_action_count())
        {
            const start_menu_action action = start_menu_action_at(action_index);
            start_menu_selected_category = action == START_MENU_ACTION_PINNED
                ? -1
                : action == START_MENU_ACTION_ALL ? -2 : action_index - 2;
            start_menu_app_scroll = 0;
        }
        return START_MENU_CLICK_HANDLED;
    }

    const int list_top = static_cast<int>(menu_y) + START_MENU_HEADER_HEIGHT + 32;
    const int list_bottom = footer_y;
    if (list_top < list_bottom &&
        mouse_x >= static_cast<int>(menu_x) + START_MENU_NAV_WIDTH &&
        mouse_x < static_cast<int>(menu_x) + static_cast<int>(menu_w) &&
        mouse_y >= list_top && mouse_y < list_bottom)
    {
        const int columns = (static_cast<int>(menu_w) - START_MENU_NAV_WIDTH) /
                            START_MENU_GRID_CELL_WIDTH;
        const int column_offset = mouse_x - static_cast<int>(menu_x) - START_MENU_NAV_WIDTH;
        const int raw_column = column_offset / START_MENU_GRID_CELL_WIDTH;
        const int column = raw_column < columns ? raw_column : columns - 1;
        const int row = (mouse_y - list_top) / start_menu_grid_row_height();
        int app_indices[NAPP_MAX_APPLICATIONS];
        const int app_count = get_visible_apps(app_indices);
        const int app_index = start_menu_app_scroll + row * columns + column;
        if (row >= 0 && column >= 0 && column < columns && app_index < app_count)
        {
            napp_run(start_menu_napps[app_indices[app_index]], nullptr);
        }
        return START_MENU_CLICK_HANDLED;
    }

    return START_MENU_CLICK_NONE;
}

bool handle_start_menu_key(char key)
{
    if (!menu_start_open)
    {
        return false;
    }

    if (key == NAPP_KEY_ESCAPE)
    {
        if (start_menu_search_focused)
        {
            start_menu_search[0] = '\0';
            start_menu_search_focused = false;
            start_menu_app_scroll = 0;
        }
        else
        {
            close_start_menu();
        }
        return true;
    }

    if (!start_menu_search_focused && (key == 'j' || key == 'k'))
    {
        scroll_start_menu(key == 'k' ? -1 : 1, false);
        return true;
    }

    if (!start_menu_search_focused && key >= 32 && key <= 126)
    {
        start_menu_search_focused = true;
    }

    if (key == '\b')
    {
        const uint32_t length = strlen(start_menu_search);
        if (length > 0)
        {
            start_menu_search[length - 1] = '\0';
            start_menu_app_scroll = 0;
        }
        return true;
    }

    if (key >= 32 && key <= 126)
    {
        const uint32_t length = strlen(start_menu_search);
        if (length + 1 < sizeof(start_menu_search))
        {
            start_menu_search[length] = key;
            start_menu_search[length + 1] = '\0';
            start_menu_app_scroll = 0;
        }
        return true;
    }

    return false;
}

bool is_mouse_over_start(int mouse_x, int mouse_y) 
{
    if (mouse_x >= start_button.x1 && mouse_x <= start_button.x2 && mouse_y >= start_button.y1 && mouse_y <= start_button.y2) 
    {
        return true;
    }

    return false;
}

bool is_mouse_over_icon(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}