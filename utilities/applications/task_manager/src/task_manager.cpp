#include <napp.h>

NAPP_APPLICATION("task_manager", "View running system processes", true);

static const napp_api* task_api = nullptr;
static uint32_t process_scroll = 0;
static uint32_t visible_rows = 10;

static constexpr uint32_t COLOR_BACKGROUND = 0xFF11151D;
static constexpr uint32_t COLOR_PANEL = 0xFF1B222D;
static constexpr uint32_t COLOR_HEADER = 0xFF283240;
static constexpr uint32_t COLOR_TEXT = 0xFFE1E7EF;
static constexpr uint32_t COLOR_MUTED = 0xFF9BA8B8;
static constexpr uint32_t COLOR_ACCENT = 0xFF59E81B;
static constexpr uint32_t COLOR_RED = 0xFFFF7777;

static void append_number(char* output, uint32_t capacity, uint32_t value)
{
    if (capacity == 0)
    {
        return;
    }
    char digits[12];
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

static const char* process_type_name(uint32_t type)
{
    switch (type)
    {
        case NAPP_PROCESS_SYSTEM:
            return "SYSTEM";
        case NAPP_PROCESS_APPLICATION:
            return "APP";
        case NAPP_PROCESS_COMMAND:
            return "COMMAND";
        default:
            return "UNKNOWN";
    }
}

static void draw_text(const char* text, int x, int y, int width, uint32_t color)
{
    if (task_api == nullptr || task_api->gui == nullptr ||
        task_api->gui->draw_text == nullptr || text == nullptr || width < 8)
    {
        return;
    }
    char clipped[96];
    uint32_t index = 0;
    const uint32_t max_chars = static_cast<uint32_t>(width / 8);
    while (text[index] != '\0' && index < max_chars && index + 1 < sizeof(clipped))
    {
        clipped[index] = text[index];
        ++index;
    }
    clipped[index] = '\0';
    task_api->gui->draw_text(clipped, x, y, color);
}

static void draw_task_manager(napp_window* window)
{
    if (window == nullptr || task_api == nullptr || task_api->gui == nullptr ||
        task_api->abi_version < 10 || task_api->get_processes == nullptr)
    {
        return;
    }

    const int content_y = window->pos_y + window->title_height;
    const int content_height = window->height - window->title_height;
    if (content_height <= 0 || window->width <= 0)
    {
        return;
    }

    task_api->gui->fill_block(window->pos_x, content_y, COLOR_BACKGROUND, window->width, content_height);
    task_api->gui->fill_block(window->pos_x + 12, content_y + 10, COLOR_PANEL,
                              window->width - 24, content_height - 22);
    const int table_x = window->pos_x + 24;
    const int table_width = window->width - 48;
    draw_text("PROCESS MANAGER", table_x, content_y + 22, table_width, COLOR_ACCENT);
    draw_text("Logical system services and application processes",
              table_x, content_y + 42, table_width, COLOR_MUTED);

    napp_process_info processes[NAPP_MAX_PROCESSES]{};
    const uint32_t count = task_api->get_processes(processes, NAPP_MAX_PROCESSES);
    uint32_t running = 0;
    for (uint32_t index = 0; index < count; ++index)
    {
        if (processes[index].state == NAPP_PROCESS_RUNNING)
        {
            ++running;
        }
    }

    char summary[64] = "Processes: ";
    uint32_t summary_length = 11;
    char number[12];
    append_number(number, sizeof(number), count);
    for (uint32_t index = 0; number[index] != '\0' && summary_length + 1 < sizeof(summary); ++index)
    {
        summary[summary_length++] = number[index];
    }
    const char running_label[] = "  Running: ";
    for (uint32_t index = 0; running_label[index] != '\0' && summary_length + 1 < sizeof(summary); ++index)
    {
        summary[summary_length++] = running_label[index];
    }
    append_number(number, sizeof(number), running);
    for (uint32_t index = 0; number[index] != '\0' && summary_length + 1 < sizeof(summary); ++index)
    {
        summary[summary_length++] = number[index];
    }
    summary[summary_length] = '\0';
    draw_text(summary, table_x, content_y + 62, table_width, COLOR_TEXT);

    const int table_y = content_y + 88;
    task_api->gui->fill_block(table_x, table_y, COLOR_HEADER, table_width, 22);
    draw_text("PID", table_x + 8, table_y + 7, 42, COLOR_MUTED);
    draw_text("NAME", table_x + 52, table_y + 7, table_width - 250, COLOR_MUTED);
    draw_text("TYPE", table_x + table_width - 180, table_y + 7, 72, COLOR_MUTED);
    draw_text("STATE", table_x + table_width - 105, table_y + 7, 96, COLOR_MUTED);

    const uint32_t available = content_height > 135 ?
        static_cast<uint32_t>(content_height - 135) / 25 : 1;
    visible_rows = available < 12 ? available : 12;
    if (process_scroll + visible_rows > count)
    {
        process_scroll = count > visible_rows ? count - visible_rows : 0;
    }

    for (uint32_t row = 0; row < visible_rows && process_scroll + row < count; ++row)
    {
        const napp_process_info& process = processes[process_scroll + row];
        const int row_y = table_y + 22 + static_cast<int>(row * 25);
        if ((row & 1) == 0)
        {
            task_api->gui->fill_block(table_x, row_y, COLOR_PANEL, table_width, 25);
        }
        append_number(number, sizeof(number), process.pid);
        draw_text(number, table_x + 8, row_y + 8, 42, COLOR_TEXT);
        draw_text(process.name, table_x + 52, row_y + 8, table_width - 250, COLOR_TEXT);
        draw_text(process_type_name(process.type), table_x + table_width - 180,
                  row_y + 8, 72, COLOR_MUTED);
        const bool is_running = process.state == NAPP_PROCESS_RUNNING;
        draw_text(is_running ? "RUNNING" : "EXITED", table_x + table_width - 105,
                  row_y + 8, 96, is_running ? COLOR_ACCENT : COLOR_RED);
    }

    draw_text("J/K or arrows: scroll  |  F5: reset",
              table_x, content_y + content_height - 24, table_width, COLOR_MUTED);
}

static void task_manager_key(napp_window*, char key)
{
    if ((key == 'j' || key == 's' || key == NAPP_KEY_DOWN) &&
        process_scroll + visible_rows < NAPP_MAX_PROCESSES)
    {
        ++process_scroll;
    }
    else if ((key == 'k' || key == 'w' || key == NAPP_KEY_UP) && process_scroll > 0)
    {
        --process_scroll;
    }
    else if (key == NAPP_KEY_F5)
    {
        process_scroll = 0;
    }
}

static void task_manager_mouse(napp_window*, int, int)
{
    process_scroll = 0;
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version < 10 || api->get_processes == nullptr ||
        api->gui == nullptr || api->gui->open_window == nullptr ||
        api->gui->fill_block == nullptr || api->gui->draw_text == nullptr)
    {
        return 1;
    }

    task_api = api;
    process_scroll = 0;
    const napp_window_config config =
    {
        "Task Manager", 560, 370, true, true, nullptr,
        draw_task_manager, task_manager_key, task_manager_mouse, nullptr,
        nullptr, 0, nullptr, nullptr
    };
    return api->gui->open_window(&config) ? 0 : 1;
}
