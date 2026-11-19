#include <napp.h>
#include <nasua_user_gui.h>

NAPP_USER_APPLICATION("snake", "Classic Snake game", false, "Games");


// CONFIGURATION
#define SNAKE_GRID_SIZE 20
#define SNAKE_CELL_SIZE 14

#define SNAKE_BOARD_SIZE (SNAKE_GRID_SIZE * SNAKE_CELL_SIZE)

#define SNAKE_PADDING 5

#define SNAKE_WINDOW_WIDTH (SNAKE_BOARD_SIZE + SNAKE_PADDING * 2)
#define SNAKE_WINDOW_HEIGHT (SNAKE_BOARD_SIZE + 40)

#define SNAKE_MAX_LENGTH 400


// TYPES
struct snake_point
{
    int16_t x;
    int16_t y;
};

enum snake_direction
{
    SNAKE_UP,
    SNAKE_DOWN,
    SNAKE_LEFT,
    SNAKE_RIGHT
};

struct snake_state
{
    snake_point body[SNAKE_MAX_LENGTH];

    int length;

    snake_point food;

    snake_direction direction;
    snake_direction next_direction;

    int score;

    bool game_over;
    bool started;
    bool timer_event_logged;
    uint32_t random_state;
};

// RANDOM
static uint32_t snake_random(snake_state* state)
{
    state->random_state ^= state->random_state << 13;
    state->random_state ^= state->random_state >> 17;
    state->random_state ^= state->random_state << 5;

    return state->random_state;
}

// FOOD
static bool snake_is_body(const snake_state* state, int x, int y)
{
    for (int i = 0; i < state->length; i++)
    {
        if (state->body[i].x == x && state->body[i].y == y)
        {
            return true;
        }
    }

    return false;
}

static void snake_spawn_food(snake_state* state)
{
    for (int attempts = 0; attempts < 1000; attempts++)
    {
        int x = snake_random(state) % SNAKE_GRID_SIZE;
        int y = snake_random(state) % SNAKE_GRID_SIZE;

        if (!snake_is_body(state, x, y))
        {
            state->food.x = x;
            state->food.y = y;
            return;
        }
    }

    state->food.x = 0;
    state->food.y = 0;
}


// RESET
static void snake_reset(snake_state* state)
{
    state->length = 3;

    state->body[0].x = 10;
    state->body[0].y = 10;

    state->body[1].x = 9;
    state->body[1].y = 10;

    state->body[2].x = 8;
    state->body[2].y = 10;

    state->direction = SNAKE_RIGHT;
    state->next_direction = SNAKE_RIGHT;

    state->score = 0;

    state->game_over = false;
    state->started = false;
    state->timer_event_logged = false;
    state->random_state = 0x12345678;

    snake_spawn_food(state);
}


// MOVE
static void snake_move(snake_state* state)
{
    if (state->game_over)
    {
        return;
    }

    state->direction = state->next_direction;

    snake_point new_head = state->body[0];

    switch (state->direction)
    {
        case SNAKE_UP:
        {
            new_head.y--;
            break;
        }

        case SNAKE_DOWN:
        {
            new_head.y++;
            break;
        }

        case SNAKE_LEFT:
        {
            new_head.x--;
            break;
        }

        case SNAKE_RIGHT:
        {
            new_head.x++;
            break;
        }
    }


    // Wall collision
    if (new_head.x < 0 || new_head.x >= SNAKE_GRID_SIZE || new_head.y < 0 || new_head.y >= SNAKE_GRID_SIZE)
    {
        state->game_over = true;
        return;
    }


    // Self collision
    for (int i = 0; i < state->length; i++)
    {
        if (state->body[i].x == new_head.x && state->body[i].y == new_head.y)
        {
            state->game_over = true;
            return;
        }
    }


    bool ate_food = new_head.x == state->food.x && new_head.y == state->food.y;


    if (ate_food && state->length < SNAKE_MAX_LENGTH)
    {
        state->length++;
    }


    for (int i = state->length - 1; i > 0; i--)
    {
        state->body[i] = state->body[i - 1];
    }

    state->body[0] = new_head;

    if (ate_food)
    {
        state->score++;
        snake_spawn_food(state);
    }
}


// SCORE
static void draw_score(const snake_state* state, int x, int y)
{
    char buffer[32];

    buffer[0] = 'S';
    buffer[1] = 'c';
    buffer[2] = 'o';
    buffer[3] = 'r';
    buffer[4] = 'e';
    buffer[5] = ':';
    buffer[6] = ' ';

    int index = 7;
    int score = state->score;

    if (score == 0)
    {
        buffer[index++] = '0';
    }
    else
    {
        char digits[16];
        int count = 0;

        while (score > 0)
        {
            digits[count++] = '0' + (score % 10);
            score /= 10;
        }

        while (count > 0)
        {
            buffer[index++] = digits[--count];
        }
    }

    buffer[index] = '\0';

    nasua_user_gui_draw_text(buffer, x, y, NAPP_COLOR_WHITE);
}


// DRAW
static void draw_snake(napp_window* win)
{
    const snake_state* state = static_cast<const snake_state*>(win->userdata);
    int board_x = win->pos_x + SNAKE_PADDING;
    int board_y = win->pos_y + win->title_height + SNAKE_PADDING;


    // Black board
    nasua_user_gui_fill_block(board_x, board_y, NAPP_COLOR_BLACK, SNAKE_BOARD_SIZE, SNAKE_BOARD_SIZE);

    // Score
    draw_score(state, board_x + 5, board_y + 5);

    // Food
    nasua_user_gui_fill_block(board_x + state->food.x * SNAKE_CELL_SIZE, board_y + state->food.y * SNAKE_CELL_SIZE, NAPP_COLOR_GREEN, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1);


    // Snake
    for (int i = 0; i < state->length; i++)
    {
        uint32_t color = (i == 0) ? NAPP_COLOR_WHITE : NAPP_COLOR_GRAY;

        nasua_user_gui_fill_block(board_x + state->body[i].x * SNAKE_CELL_SIZE, board_y + state->body[i].y * SNAKE_CELL_SIZE, color, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1);
    }


    // Game over
    if (state->game_over)
    {
        nasua_user_gui_draw_text("GAME OVER", board_x + 95, board_y + 125, NAPP_COLOR_GREEN);
        nasua_user_gui_draw_text("Press R", board_x + 105, board_y + 145, NAPP_COLOR_WHITE);
    }
    else if (!state->started)
    {
        nasua_user_gui_draw_text("SNAKE", board_x + 115, board_y + 125, NAPP_COLOR_GREEN);
        nasua_user_gui_draw_text("WASD to move", board_x + 85, board_y + 145, NAPP_COLOR_WHITE);
    }
}


// TICK -- called periodically by the kernel via the NAPP timer.  The snake moves forward automatically once the game has started and stops when the player loses.
static void snake_tick(napp_window* win)
{
    snake_state* state = static_cast<snake_state*>(win->userdata);

    if (!state->timer_event_logged)
    {
        static constexpr char message[] =
            "[SNAKE] Ring-3 timer event delivered\n";
        constexpr uint64_t message_length = sizeof(message) - 1;
        if (nasua_user_write(message, message_length) !=
            static_cast<int64_t>(message_length))
        {
            nasua_user_exit(1);
        }
        state->timer_event_logged = true;
    }

    if (!state->started || state->game_over)
    {
        return;
    }

    snake_move(state);
}


// KEYBOARD
static void snake_key(napp_window* win, char key)
{
    snake_state* state = static_cast<snake_state*>(win->userdata);


    // RESET
    if (key == 'r' || key == 'R')
    {
        snake_reset(state);
        return;
    }


    if (state->game_over)
    {
        return;
    }


    // W
    if (key == 'w' || key == 'W')
    {
        if (!state->started || state->direction != SNAKE_DOWN)
        {
            state->next_direction = SNAKE_UP;
            state->started = true;
        }

        return;
    }


    // S
    if (key == 's' || key == 'S')
    {
        if (!state->started || state->direction != SNAKE_UP)
        {
            state->next_direction = SNAKE_DOWN;
            state->started = true;
        }

        return;
    }


    // A
    if (key == 'a' || key == 'A')
    {
        if (!state->started || state->direction != SNAKE_RIGHT)
        {
            state->next_direction = SNAKE_LEFT;
            state->started = true;
        }

        return;
    }


    // D
    if (key == 'd' || key == 'D')
    {
        if (!state->started || state->direction != SNAKE_LEFT)
        {
            state->next_direction = SNAKE_RIGHT;
            state->started = true;
        }

        return;
    }
}


// ENTRY POINT
int _start(void)
{
    snake_state snake_data = {};
    snake_reset(&snake_data);

    napp_window_config config = {};

    config.title = "Snake";

    config.width = SNAKE_WINDOW_WIDTH;
    config.height = SNAKE_WINDOW_HEIGHT;

    config.resizable = false;
    config.can_maximize = false;

    config.userdata = &snake_data;

    config.draw = draw_snake;
    config.key = snake_key;
    config.mouse = nullptr;
    config.tick = snake_tick;

    // Snake moves at 5 cells per second (200 ms per step).
    config.tick_interval_ms = 200;


    static constexpr char startup_message[] =
        "[SNAKE] Running in ring 3\n";
    constexpr uint64_t startup_message_length = sizeof(startup_message) - 1;
    if (nasua_user_write(startup_message, startup_message_length) !=
        static_cast<int64_t>(startup_message_length))
    {
        nasua_user_exit(1);
    }

    if (!nasua_user_gui_run(&config))
    {
        nasua_user_exit(1);
    }

    nasua_user_exit(0);
}