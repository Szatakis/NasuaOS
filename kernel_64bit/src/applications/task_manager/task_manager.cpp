#include "task_manager.hpp"

static constexpr uint32_t TASK_MANAGER_BG = 0xFF11151D;
static constexpr uint32_t TASK_MANAGER_PANEL = 0xFF1B222D;
static constexpr uint32_t TASK_MANAGER_HEADER = 0xFF283240;
static constexpr uint32_t TASK_MANAGER_TEXT = 0xFFE1E7EF;
static constexpr uint32_t TASK_MANAGER_MUTED = 0xFF9BA8B8;
static constexpr uint32_t TASK_MANAGER_GREEN = 0xFF59E81B;
static constexpr uint32_t TASK_MANAGER_RED = 0xFFFF7777;
static constexpr uint32_t TASK_MANAGER_ROW_HEIGHT = 25;
static constexpr uint32_t TASK_MANAGER_MAX_VISIBLE = 12;

static uint32_t process_scroll = 0;
static uint32_t visible_process_rows = TASK_MANAGER_MAX_VISIBLE;

static const char* process_type_name(Process::Type type)
{
    switch (type)
    {
        case Process::Type::System:
            return "SYSTEM";
        case Process::Type::Application:
            return "APP";
        case Process::Type::Command:
            return "COMMAND";
    }
    return "UNKNOWN";
}

static void append_number(char* output, uint32_t* length, uint32_t value)
{
    char digits[12];
    uint32_t count = 0;
    do
    {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (value != 0 && count < sizeof(digits));

    while (count > 0)
    {
        output[(*length)++] = digits[--count];
    }
    output[*length] = '\0';
}

static void draw_task_manager(Gpu::Window_Manager::window_struct* win)
{
    if (win == nullptr)
    {
        return;
    }

    const int x = win->pos_x;
    const int y = win->pos_y + 28;
    const int width = win->width;
    const int height = win->height - 28;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    Gpu::fill_block(x, y, TASK_MANAGER_BG, width, height);
    Gpu::fill_block(x + 12, y + 10, TASK_MANAGER_PANEL, width - 24, height - 22);
    Gpu::print_at8("PROCESS MANAGER", x + 24, y + 22, TASK_MANAGER_GREEN);
    Gpu::print_at8("Logical system services and application processes",
                   x + 24, y + 42, TASK_MANAGER_MUTED);

    Process::Info processes[Process::MAX_PROCESSES];
    const uint32_t count = Process::snapshot(processes, Process::MAX_PROCESSES);
    uint32_t running = 0;
    for (uint32_t index = 0; index < count; ++index)
    {
        if (processes[index].state == Process::State::Running)
        {
            ++running;
        }
    }

    char summary[64] = "Processes: ";
    uint32_t summary_length = 11;
    append_number(summary, &summary_length, count);
    const char running_label[] = "  Running: ";
    for (uint32_t index = 0; running_label[index] != '\0'; ++index)
    {
        summary[summary_length++] = running_label[index];
    }
    summary[summary_length] = '\0';
    append_number(summary, &summary_length, running);
    Gpu::print_at8(summary, x + 24, y + 62, TASK_MANAGER_TEXT);

    const int table_x = x + 24;
    const int table_y = y + 88;
    const int table_width = width - 48;
    Gpu::fill_block(table_x, table_y, TASK_MANAGER_HEADER, table_width, 22);
    Gpu::print_at8("PID", table_x + 8, table_y + 7, TASK_MANAGER_MUTED);
    Gpu::print_at8("NAME", table_x + 52, table_y + 7, TASK_MANAGER_MUTED);
    Gpu::print_at8("TYPE", table_x + table_width - 180, table_y + 7, TASK_MANAGER_MUTED);
    Gpu::print_at8("STATE", table_x + table_width - 105, table_y + 7, TASK_MANAGER_MUTED);

    const uint32_t available_rows = height > 135 ?
        (uint32_t)(height - 135) / TASK_MANAGER_ROW_HEIGHT : 1;
    visible_process_rows = available_rows < TASK_MANAGER_MAX_VISIBLE ?
        available_rows : TASK_MANAGER_MAX_VISIBLE;
    if (process_scroll + visible_process_rows > count)
    {
        process_scroll = count > visible_process_rows ? count - visible_process_rows : 0;
    }

    for (uint32_t row = 0; row < visible_process_rows && process_scroll + row < count; ++row)
    {
        const Process::Info& process = processes[process_scroll + row];
        const int row_y = table_y + 22 + (int)(row * TASK_MANAGER_ROW_HEIGHT);
        if ((row & 1) == 0)
        {
            Gpu::fill_block(table_x, row_y, TASK_MANAGER_PANEL, table_width, TASK_MANAGER_ROW_HEIGHT);
        }

        char pid_text[12];
        uint32_t pid_length = 0;
        append_number(pid_text, &pid_length, process.pid);
        Gpu::print_at8(pid_text, table_x + 8, row_y + 8, TASK_MANAGER_TEXT);
        Gpu::print_at8(process.name, table_x + 52, row_y + 8, TASK_MANAGER_TEXT);
        Gpu::print_at8(process_type_name(process.type), table_x + table_width - 180,
                       row_y + 8, TASK_MANAGER_MUTED);
        Gpu::print_at8(process.state == Process::State::Running ? "RUNNING" : "EXITED",
                       table_x + table_width - 105, row_y + 8,
                       process.state == Process::State::Running ? TASK_MANAGER_GREEN : TASK_MANAGER_RED);
    }

    Gpu::print_at8("J/K: scroll  |  F5: reset",
                   x + 24, y + height - 24, TASK_MANAGER_MUTED);
}

static void task_manager_key(Gpu::Window_Manager::window_struct*, char key)
{
    if ((key == 'j' || key == 's' || key == '2') &&
        process_scroll + visible_process_rows < Process::MAX_PROCESSES)
    {
        ++process_scroll;
    }
    else if ((key == 'k' || key == 'w' || key == '8') && process_scroll > 0)
    {
        --process_scroll;
    }
    else if ((uint8_t)key == 0xF5)
    {
        process_scroll = 0;
    }
}

static void task_manager_mouse(Gpu::Window_Manager::window_struct*, int mouse_x, int mouse_y)
{
    (void)mouse_x;
    (void)mouse_y;
    process_scroll = 0;
}

Gpu::Window_Manager::window_struct task_manager = 
{
    .name = "Task Manager",
    .id = 0,
    .process_id = 0,
    .process_attached = false,

    .pos_x = 10,
    .pos_y = 10,
    .width = 500,
    .height = 350,
    .min_width = 500,
    .min_height = 300,

    .visible = false,
    .minimized = false,
    .focused = false,

    .resizable = true,
    .can_maximize = true,
    .maximized = false,

    .restore_pos_x = 0,
    .restore_pos_y = 0,
    .restore_width = 0,
    .restore_height = 0,

    .is_dragging = false,
    .drag_offset_x = 0,
    .drag_offset_y = 0,

    .is_resizing = false,
    .resize_right = false,
    .resize_bottom = false,
    .resize_start_mouse_x = 500,
    .resize_start_mouse_y = 350,
    .resize_start_width = 10,
    .resize_start_height = 20,
    .max_width = 0,
    .max_height = 0,

    .userdata = nullptr,
    .draw_content = draw_task_manager,
    .key_press = task_manager_key,
    .mouse_click = task_manager_mouse,
    .mouse_button = 0
};