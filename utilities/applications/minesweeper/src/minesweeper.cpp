#include <napp.h>

NAPP_APPLICATION("minesweeper", "Classic Minesweeper game", false);

static const napp_gui* gui = nullptr;


// CONFIGURATION
#define MS_EASY_WIDTH       9
#define MS_EASY_HEIGHT      9
#define MS_EASY_MINES       10

#define MS_MEDIUM_WIDTH     16
#define MS_MEDIUM_HEIGHT    16
#define MS_MEDIUM_MINES     40

#define MS_HARD_WIDTH       24
#define MS_HARD_HEIGHT      16
#define MS_HARD_MINES       99

#define MS_CELL_SIZE        22
#define MS_PADDING          8

#define MS_MAX_WIDTH        MS_HARD_WIDTH
#define MS_MAX_HEIGHT       MS_HARD_HEIGHT
#define MS_MAX_CELLS        (MS_MAX_WIDTH * MS_MAX_HEIGHT)


// TYPES
struct ms_cell
{
    bool mine;
    bool revealed;
    bool flagged;
    uint8_t adjacent;
};

enum ms_difficulty
{
    MS_EASY,
    MS_MEDIUM,
    MS_HARD
};

enum ms_game_state
{
    MS_PLAYING,
    MS_WON,
    MS_LOST
};

struct minesweeper_state
{
    ms_cell cells[MS_MAX_CELLS];

    int width;
    int height;
    int mines;

    int flags;
    int revealed;

    int elapsed_seconds;

    ms_difficulty difficulty;
    ms_game_state state;

    uint64_t last_second_tick;
    bool started;
};

static minesweeper_state ms_data;


// RANDOM
static uint32_t ms_random_state = 0xA53C91E7;

static uint32_t ms_random()
{
    ms_random_state ^= ms_random_state << 13;
    ms_random_state ^= ms_random_state >> 17;
    ms_random_state ^= ms_random_state << 5;

    return ms_random_state;
}


// HELPERS
static int ms_index(int x, int y)
{
    return y * ms_data.width + x;
}

static bool ms_inside(int x, int y)
{
    return x >= 0 && x < ms_data.width && y >= 0 && y < ms_data.height;
}

static int ms_count_adjacent_mines(int x, int y)
{
    int count = 0;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;

            if (!ms_inside(nx, ny))
            {
                continue;
            }

            if (ms_data.cells[ms_index(nx, ny)].mine)
            {
                count++;
            }
        }
    }

    return count;
}


// DIFFICULTY
static void ms_set_difficulty(ms_difficulty difficulty)
{
    ms_data.difficulty = difficulty;

    switch (difficulty)
    {
        case MS_EASY:
        {
            ms_data.width = MS_EASY_WIDTH;
            ms_data.height = MS_EASY_HEIGHT;
            ms_data.mines = MS_EASY_MINES;
            break;
        }

        case MS_MEDIUM:
        {
            ms_data.width = MS_MEDIUM_WIDTH;
            ms_data.height = MS_MEDIUM_HEIGHT;
            ms_data.mines = MS_MEDIUM_MINES;
            break;
        }

        case MS_HARD:
        {
            ms_data.width = MS_HARD_WIDTH;
            ms_data.height = MS_HARD_HEIGHT;
            ms_data.mines = MS_HARD_MINES;
            break;
        }
    }
}


// RESET / GENERATE BOARD
static void ms_reset()
{
    for (int i = 0; i < MS_MAX_CELLS; i++)
    {
        ms_data.cells[i].mine = false;
        ms_data.cells[i].revealed = false;
        ms_data.cells[i].flagged = false;
        ms_data.cells[i].adjacent = 0;
    }

    ms_data.flags = 0;
    ms_data.revealed = 0;
    ms_data.elapsed_seconds = 0;
    ms_data.last_second_tick = 0;

    ms_data.state = MS_PLAYING;
    ms_data.started = false;

    // Place mines.
    int placed = 0;

    while (placed < ms_data.mines)
    {
        int x = ms_random() % ms_data.width;
        int y = ms_random() % ms_data.height;

        int index = ms_index(x, y);

        if (ms_data.cells[index].mine)
        {
            continue;
        }

        ms_data.cells[index].mine = true;
        placed++;
    }

    // Calculate adjacent mine counts.
    for (int y = 0; y < ms_data.height; y++)
    {
        for (int x = 0; x < ms_data.width; x++)
        {
            int index = ms_index(x, y);

            if (!ms_data.cells[index].mine)
            {
                ms_data.cells[index].adjacent = ms_count_adjacent_mines(x, y);
            }
        }
    }
}


// FLOOD FILL
static void ms_reveal_empty(int start_x, int start_y)
{
    int queue_x[MS_MAX_CELLS];
    int queue_y[MS_MAX_CELLS];

    int queue_start = 0;
    int queue_end = 0;

    queue_x[queue_end] = start_x;
    queue_y[queue_end] = start_y;
    queue_end++;

    while (queue_start < queue_end)
    {
        int x = queue_x[queue_start];
        int y = queue_y[queue_start];

        queue_start++;

        if (!ms_inside(x, y))
        {
            continue;
        }

        int index = ms_index(x, y);
        ms_cell* cell = &ms_data.cells[index];

        if (cell->revealed || cell->flagged || cell->mine)
        {
            continue;
        }

        cell->revealed = true;
        ms_data.revealed++;

        if (cell->adjacent != 0)
        {
            continue;
        }

        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                int nx = x + dx;
                int ny = y + dy;

                if (!ms_inside(nx, ny))
                {
                    continue;
                }

                int ni = ms_index(nx, ny);

                if (!ms_data.cells[ni].revealed && !ms_data.cells[ni].flagged && !ms_data.cells[ni].mine)
                {
                    if (queue_end < MS_MAX_CELLS)
                    {
                        queue_x[queue_end] = nx;
                        queue_y[queue_end] = ny;
                        queue_end++;
                    }
                }
            }
        }
    }
}


// WIN CHECK
static void ms_check_win()
{
    int safe_cells = ms_data.width * ms_data.height - ms_data.mines;

    if (ms_data.revealed >= safe_cells)
    {
        ms_data.state = MS_WON;

        // Automatically flag all mines.
        for (int y = 0; y < ms_data.height; y++)
        {
            for (int x = 0; x < ms_data.width; x++)
            {
                ms_cell* cell = &ms_data.cells[ms_index(x, y)];

                if (cell->mine && !cell->flagged)
                {
                    cell->flagged = true;
                    ms_data.flags++;
                }
            }
        }
    }
}


// REVEAL
static void ms_reveal(int x, int y)
{
    if (!ms_inside(x, y))
    {
        return;
    }

    if (ms_data.state != MS_PLAYING)
    {
        return;
    }

    ms_cell* cell = &ms_data.cells[ms_index(x, y)];

    if (cell->revealed || cell->flagged)
    {
        return;
    }

    ms_data.started = true;

    if (cell->mine)
    {
        cell->revealed = true;
        ms_data.state = MS_LOST;

        // Reveal all mines.
        for (int yy = 0; yy < ms_data.height; yy++)
        {
            for (int xx = 0; xx < ms_data.width; xx++)
            {
                ms_cell* other = &ms_data.cells[ms_index(xx, yy)];

                if (other->mine)
                {
                    other->revealed = true;
                }
            }
        }

        return;
    }

    if (cell->adjacent == 0)
    {
        ms_reveal_empty(x, y);
    }
    else
    {
        cell->revealed = true;
        ms_data.revealed++;
    }

    ms_check_win();
}


// FLAG
static void ms_toggle_flag(int x, int y)
{
    if (!ms_inside(x, y))
    {
        return;
    }

    if (ms_data.state != MS_PLAYING)
    {
        return;
    }

    ms_cell* cell = &ms_data.cells[ms_index(x, y)];

    if (cell->revealed)
    {
        return;
    }

    if (!cell->flagged)
    {
        if (ms_data.flags >= ms_data.mines)
        {
            return;
        }

        cell->flagged = true;
        ms_data.flags++;
    }
    else
    {
        cell->flagged = false;
        ms_data.flags--;
    }
}


// TEXT HELPERS
static void ms_draw_number(int value, int x, int y)
{
    char buffer[16];

    int pos = 0;

    if (value == 0)
    {
        buffer[pos++] = '0';
    }
    else
    {
        char digits[16];
        int count = 0;

        while (value > 0)
        {
            digits[count++] = '0' + (value % 10);
            value /= 10;
        }

        while (count > 0)
        {
            buffer[pos++] = digits[--count];
        }
    }

    buffer[pos] = '\0';

    gui->draw_text(buffer, x, y, NAPP_COLOR_WHITE);
}


// DRAW BUTTON
static bool ms_point_in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void ms_draw_button(int x, int y, int width, const char* text, bool active)
{
    uint32_t color = active ? NAPP_COLOR_GREEN : NAPP_COLOR_TITLEBAR;

    gui->fill_block(x, y, color, width, 22);

    gui->draw_text(text, x + 5, y + 5, active ? NAPP_COLOR_BLACK : NAPP_COLOR_WHITE);
}


// DRAW GAME
static void ms_draw(napp_window* win)
{
    int board_x =  win->pos_x + MS_PADDING;
    int board_y = win->pos_y + win->title_height + 40;
    int board_width = ms_data.width * MS_CELL_SIZE;
    int board_height = ms_data.height * MS_CELL_SIZE;


    // Background.
    gui->fill_block(board_x - MS_PADDING, board_y - 40, NAPP_COLOR_WINDOW, board_width + MS_PADDING * 2, board_height + 85);


    // Header.
    gui->draw_text( "MINES:", board_x, board_y - 32, NAPP_COLOR_WHITE);

    ms_draw_number(ms_data.mines - ms_data.flags, board_x + 48, board_y - 32);

    gui->draw_text("TIME:", board_x + 95, board_y - 32, NAPP_COLOR_WHITE);

    ms_draw_number(ms_data.elapsed_seconds, board_x + 133, board_y - 32);


    // Board.
    for (int y = 0; y < ms_data.height; y++)
    {
        for (int x = 0; x < ms_data.width; x++)
        {
            ms_cell* cell = &ms_data.cells[ms_index(x, y)];

            int px = board_x + x * MS_CELL_SIZE;
            int py = board_y + y * MS_CELL_SIZE;


            // Revealed cell.
            if (cell->revealed)
            {
                gui->fill_block(px, py, NAPP_COLOR_GRAY, MS_CELL_SIZE - 1, MS_CELL_SIZE - 1
                );

                if (cell->mine)
                {
                    gui->draw_text("*", px + 7, py + 3, NAPP_COLOR_BLACK);
                }
                else if (cell->adjacent > 0)
                {
                    char number[2];

                    number[0] = '0' + cell->adjacent;
                    number[1] = '\0';

                    uint32_t color = NAPP_COLOR_BLACK;

                    gui->draw_text(number, px + 7, py + 3, color);
                }
            }
            else
            {
                gui->fill_block(px, py, NAPP_COLOR_TITLEBAR, MS_CELL_SIZE - 1, MS_CELL_SIZE - 1);

                if (cell->flagged)
                {
                    gui->draw_text("F", px + 7, py + 3, NAPP_COLOR_GREEN);
                }
            }
        }
    }


    // Status text.
    int status_y = board_y + board_height + 5;

    if (ms_data.state == MS_LOST)
    {
        gui->draw_text("GAME OVER", board_x, status_y, NAPP_COLOR_GREEN);
    }
    else if (ms_data.state == MS_WON)
    {
        gui->draw_text("YOU WIN!", board_x, status_y, NAPP_COLOR_GREEN);
    }
    else
    {
        gui->draw_text("LMB: open   RMB: flag", board_x, status_y, NAPP_COLOR_WHITE);
    }


    // Difficulty buttons.
    int buttons_y =
        status_y + 18;

    ms_draw_button(
        board_x,
        buttons_y,
        55,
        "Easy",
        ms_data.difficulty == MS_EASY
    );

    ms_draw_button(
        board_x + 60,
        buttons_y,
        65,
        "Medium",
        ms_data.difficulty == MS_MEDIUM
    );

    ms_draw_button(
        board_x + 130,
        buttons_y,
        50,
        "Hard",
        ms_data.difficulty == MS_HARD
    );

    ms_draw_button(
        board_x + 185,
        buttons_y,
        65,
        "New",
        false
    );
}


// ============================================================
// MOUSE
// ============================================================

static void ms_mouse_button(napp_window* win, int mouse_x, int mouse_y, int button)
{
    int board_x =
        win->pos_x + MS_PADDING;

    int board_y =
        win->pos_y +
        win->title_height +
        40;

    int local_x =
        mouse_x - board_x;

    int local_y =
        mouse_y - board_y;


    // Inside board.
    if (local_x >= 0 &&
        local_y >= 0 &&
        local_x < ms_data.width * MS_CELL_SIZE &&
        local_y < ms_data.height * MS_CELL_SIZE)
    {
        int cell_x =
            local_x / MS_CELL_SIZE;

        int cell_y =
            local_y / MS_CELL_SIZE;


        if (button == 1 && ms_data.state == MS_PLAYING)
        {
            ms_toggle_flag(cell_x, cell_y);
        }

        return;
    }


    // Difficulty / New Game buttons.
    if (ms_data.state != MS_PLAYING)
    {
        int buttons_y =
            board_y +
            ms_data.height * MS_CELL_SIZE +
            23;

        if (button == 0 || button == 1)
        {
            if (ms_point_in_rect(mouse_x, mouse_y, board_x, buttons_y, 55, 22))
            {
                ms_set_difficulty(MS_EASY);
                ms_reset();
                return;
            }

            if (ms_point_in_rect(mouse_x, mouse_y, board_x + 60, buttons_y, 65, 22))
            {
                ms_set_difficulty(MS_MEDIUM);
                ms_reset();
                return;
            }

            if (ms_point_in_rect(mouse_x, mouse_y, board_x + 130, buttons_y, 50, 22))
            {
                ms_set_difficulty(MS_HARD);
                ms_reset();
                return;
            }

            if (ms_point_in_rect(mouse_x, mouse_y, board_x + 185, buttons_y, 65, 22))
            {
                ms_reset();
                return;
            }
        }
    }
}


static void ms_mouse(
    napp_window* win,
    int mouse_x,
    int mouse_y)
{
    if (ms_data.state == MS_PLAYING)
    {
        int board_x =
            win->pos_x + MS_PADDING;

        int board_y =
            win->pos_y +
            win->title_height +
            40;

        int local_x =
            mouse_x - board_x;

        int local_y =
            mouse_y - board_y;


        // Inside board.
        if (local_x >= 0 &&
            local_y >= 0 &&
            local_x < ms_data.width * MS_CELL_SIZE &&
            local_y < ms_data.height * MS_CELL_SIZE)
        {
            int cell_x =
                local_x / MS_CELL_SIZE;

            int cell_y =
                local_y / MS_CELL_SIZE;

            ms_reveal(cell_x, cell_y);

            return;
        }
    }


    // Difficulty / new game buttons.
    int board_x =
        win->pos_x + MS_PADDING;

    int board_y =
        win->pos_y +
        win->title_height +
        40;

    int buttons_y =
        board_y +
        ms_data.height * MS_CELL_SIZE +
        23;


    if (ms_point_in_rect(
            mouse_x,
            mouse_y,
            board_x,
            buttons_y,
            55,
            22))
    {
        ms_set_difficulty(MS_EASY);
        ms_reset();
        return;
    }


    if (ms_point_in_rect(
            mouse_x,
            mouse_y,
            board_x + 60,
            buttons_y,
            65,
            22))
    {
        ms_set_difficulty(MS_MEDIUM);
        ms_reset();
        return;
    }


    if (ms_point_in_rect(
            mouse_x,
            mouse_y,
            board_x + 130,
            buttons_y,
            50,
            22))
    {
        ms_set_difficulty(MS_HARD);
        ms_reset();
        return;
    }


    if (ms_point_in_rect(
            mouse_x,
            mouse_y,
            board_x + 185,
            buttons_y,
            65,
            22))
    {
        ms_reset();
        return;
    }
}


// ============================================================
// TIMER
// ============================================================

static const napp_api* ms_api = nullptr;

static void ms_tick(napp_window* win)
{
    (void)win;

    if (!ms_data.started ||
        ms_data.state != MS_PLAYING)
    {
        return;
    }

    uint64_t ticks = ms_api->get_ticks();

    if (ticks - ms_data.last_second_tick >= 100)
    {
        ms_data.last_second_tick = ticks;
        ms_data.elapsed_seconds++;
    }
}


// ============================================================
// ENTRY POINT
// ============================================================

int _start(const napp_api* api)
{
    if (api == nullptr ||
        api->abi_version != NAPP_ABI_VERSION ||
        api->gui == nullptr)
    {
        return 1;
    }

    gui = api->gui;
    ms_api = api;

    ms_set_difficulty(MS_EASY);
    ms_reset();

    ms_data.last_second_tick = api->get_ticks();

    napp_window_config config = {};

    config.title = "Minesweeper";

    /*
     * Maximum size is based on Hard mode.
     */
    config.width =
        MS_HARD_WIDTH * MS_CELL_SIZE +
        MS_PADDING * 2;

    config.height =
        MS_HARD_HEIGHT * MS_CELL_SIZE +
        105;

    config.resizable = false;
    config.can_maximize = false;

    config.userdata = &ms_data;

    config.draw = ms_draw;
    config.key = nullptr;
    config.mouse = ms_mouse;
    config.mouse_button = ms_mouse_button;

    config.tick = ms_tick;
    config.tick_interval_ms = 1000;


    if (!gui->open_window(&config))
    {
        api->serial_log(
            "[MINESWEEPER] Failed to open window\n"
        );

        return 1;
    }

    api->serial_log(
        "[MINESWEEPER] Window opened\n"
    );

    return 0;
}