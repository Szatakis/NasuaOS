#include <napp.h>

NAPP_APPLICATION("snake", "Classic Snake game", false);

static const napp_gui* gui = nullptr;


// CONFIGURATION
#define SNAKE_GRID_SIZE 20
#define SNAKE_CELL_SIZE 14

#define SNAKE_BOARD_SIZE (SNAKE_GRID_SIZE * SNAKE_CELL_SIZE)

#define SNAKE_PADDING 5

#define SNAKE_WINDOW_WIDTH  (SNAKE_BOARD_SIZE + SNAKE_PADDING * 2)
#define SNAKE_WINDOW_HEIGHT (SNAKE_BOARD_SIZE + 40)

#define SNAKE_MAX_LENGTH 400


// TYPES
struct snake_point
{
    int x;
    int y;
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
};

static snake_state snake_data;


// RANDOM
static uint32_t snake_random_state = 0x12345678;

static uint32_t snake_random()
{
    snake_random_state ^= snake_random_state << 13;
    snake_random_state ^= snake_random_state >> 17;
    snake_random_state ^= snake_random_state << 5;

    return snake_random_state;
}

// FOOD
static bool snake_is_body(int x, int y)
{
    for (int i = 0; i < snake_data.length; i++)
    {
        if (snake_data.body[i].x == x &&
            snake_data.body[i].y == y)
        {
            return true;
        }
    }

    return false;
}

static void snake_spawn_food()
{
    for (int attempts = 0; attempts < 1000; attempts++)
    {
        int x = snake_random() % SNAKE_GRID_SIZE;
        int y = snake_random() % SNAKE_GRID_SIZE;

        if (!snake_is_body(x, y))
        {
            snake_data.food.x = x;
            snake_data.food.y = y;
            return;
        }
    }

    snake_data.food.x = 0;
    snake_data.food.y = 0;
}


// RESET
static void snake_reset()
{
    snake_data.length = 3;

    snake_data.body[0].x = 10;
    snake_data.body[0].y = 10;

    snake_data.body[1].x = 9;
    snake_data.body[1].y = 10;

    snake_data.body[2].x = 8;
    snake_data.body[2].y = 10;

    snake_data.direction = SNAKE_RIGHT;
    snake_data.next_direction = SNAKE_RIGHT;

    snake_data.score = 0;

    snake_data.game_over = false;
    snake_data.started = false;

    snake_spawn_food();
}


// MOVE
static void snake_move()
{
    if (snake_data.game_over)
    {
        return;
    }

    snake_data.direction = snake_data.next_direction;

    snake_point new_head = snake_data.body[0];

    switch (snake_data.direction)
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
        snake_data.game_over = true;
        return;
    }


    // Self collision
    for (int i = 0; i < snake_data.length; i++)
    {
        if (snake_data.body[i].x == new_head.x && snake_data.body[i].y == new_head.y)
        {
            snake_data.game_over = true;
            return;
        }
    }


    bool ate_food = new_head.x == snake_data.food.x && new_head.y == snake_data.food.y;


    if (ate_food && snake_data.length < SNAKE_MAX_LENGTH)
    {
        snake_data.length++;
    }


    for (int i = snake_data.length - 1; i > 0; i--)
    {
        snake_data.body[i] = snake_data.body[i - 1];
    }

    snake_data.body[0] = new_head;

    if (ate_food)
    {
        snake_data.score++;
        snake_spawn_food();
    }
}


// SCORE
static void draw_score(int x, int y)
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
    int score = snake_data.score;

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

    gui->draw_text(buffer, x, y, NAPP_COLOR_WHITE);
}


// DRAW
static void draw_snake(napp_window* win)
{
    int board_x = win->pos_x + SNAKE_PADDING;
    int board_y = win->pos_y + win->title_height + SNAKE_PADDING;


    // Black board
    gui->fill_block(board_x, board_y, NAPP_COLOR_BLACK, SNAKE_BOARD_SIZE, SNAKE_BOARD_SIZE);

    // Score
    draw_score( board_x + 5, board_y + 5);

    // Food
    gui->fill_block(board_x + snake_data.food.x * SNAKE_CELL_SIZE, board_y + snake_data.food.y * SNAKE_CELL_SIZE, NAPP_COLOR_GREEN, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1);


    // Snake
    for (int i = 0; i < snake_data.length; i++)
    {
        uint32_t color = (i == 0) ? NAPP_COLOR_WHITE : NAPP_COLOR_GRAY;

        gui->fill_block(board_x + snake_data.body[i].x * SNAKE_CELL_SIZE, board_y + snake_data.body[i].y * SNAKE_CELL_SIZE, color, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1);
    }


    // Game over
    if (snake_data.game_over)
    {
        gui->draw_text("GAME OVER", board_x + 95, board_y + 125, NAPP_COLOR_GREEN);
        gui->draw_text("Press R", board_x + 105, board_y + 145, NAPP_COLOR_WHITE);
    }
    else if (!snake_data.started)
    {
        gui->draw_text("SNAKE", board_x + 115, board_y + 125, NAPP_COLOR_GREEN);
        gui->draw_text( "WASD to move", board_x + 85, board_y + 145, NAPP_COLOR_WHITE);
    }
}


// TICK -- called periodically by the kernel via the NAPP timer.  The snake
// moves forward automatically once the game has started and stops when the
// player loses.
static void snake_tick(napp_window* win)
{
    (void)win;

    if (!snake_data.started || snake_data.game_over)
    {
        return;
    }

    snake_move();
}


// KEYBOARD
static void snake_key(napp_window* win, char key)
{
    (void)win;


    // RESET
    if (key == 'r' || key == 'R')
    {
        snake_reset();
        return;
    }


    if (snake_data.game_over)
    {
        return;
    }


    // W
    if (key == 'w' || key == 'W')
    {
        if (!snake_data.started || snake_data.direction != SNAKE_DOWN)
        {
            snake_data.next_direction = SNAKE_UP;
            snake_data.started = true;
        }

        return;
    }


    // S
    if (key == 's' || key == 'S')
    {
        if (!snake_data.started || snake_data.direction != SNAKE_UP)
        {
            snake_data.next_direction = SNAKE_DOWN;
            snake_data.started = true;
        }

        return;
    }


    // A
    if (key == 'a' || key == 'A')
    {
        if (!snake_data.started || snake_data.direction != SNAKE_RIGHT)
        {
            snake_data.next_direction = SNAKE_LEFT;
            snake_data.started = true;
        }

        return;
    }


    // D
    if (key == 'd' || key == 'D')
    {
        if (!snake_data.started || snake_data.direction != SNAKE_LEFT)
        {
            snake_data.next_direction = SNAKE_RIGHT;
            snake_data.started = true;
        }

        return;
    }
}


// ENTRY POINT
int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr)
    {
        return 1;
    }

    gui = api->gui;

    snake_reset();

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


    if (!gui->open_window(&config))
    {
        api->serial_log("[SNAKE] Failed to open window\n");
        return 1;
    }

    api->serial_log("[SNAKE] Window opened\n");

    return 0;
}