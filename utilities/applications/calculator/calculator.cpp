#include <napp.h>

NAPP_APPLICATION("calculator");

/*
    ┌─────────────────────────┐
    │         DISPLAY         │
    ├──────┬──────┬──────┬────┤
    │  AC  │  CE  │ C    │ ←  │
    ├──────┼──────┼──────┼────┤
    │  x²  │  1/x │  %   │ /  │
    ├──────┼──────┼──────┼────┤
    │  7   │  8   │  9   │ *  │
    ├──────┼──────┼──────┼────┤
    │  4   │  5   │  6   │ -  │
    ├──────┼──────┼──────┼────┤
    │  1   │  2   │  3   │ +  │
    ├──────┼──────┼──────┼────┤
    │  0   │ +/-  │ sqrt │ =  │
    └──────┴──────┴──────┴────┘
*/

static const napp_gui* gui = nullptr;


static int calc_strlen(const char* text)
{
    int length = 0;

    while (text[length] != '\0')
    {
        length++;
    }

    return length;
}


static void calc_strcpy(char* destination, const char* source)
{
    int i = 0;

    while (source[i] != '\0')
    {
        destination[i] = source[i];

        i++;
    }

    destination[i] = '\0';
}


static int calc_atoi(const char* text)
{
    int i = 0;
    int sign = 1;
    int value = 0;

    if (text[i] == '-')
    {
        sign = -1;

        i++;
    }

    while (text[i] >= '0' && text[i] <= '9')
    {
        value = value * 10 + (text[i] - '0');

        i++;
    }

    return value * sign;
}


static void calc_itoa(int value, char* output)
{
    char digits[16];

    int count = 0;
    int index = 0;

    if (value < 0)
    {
        output[index] = '-';

        index++;
    }

    unsigned int magnitude = value < 0 ? (unsigned int)(-value) : (unsigned int)value;

    do
    {
        digits[count] = (char)('0' + (magnitude % 10));

        magnitude /= 10;
        count++;
    }
    while (magnitude != 0 && count < 15);

    while (count > 0)
    {
        count--;

        output[index] = digits[count];

        index++;
    }

    output[index] = '\0';
}


struct calculator_state
{
    char display[64];

    int value1;
    int value2;

    char operation;

    bool entering_second;
    bool result_shown;
};


static calculator_state calc_data =
{
    .display = "0",

    .value1 = 0,
    .value2 = 0,

    .operation = 0,

    .entering_second = false,
    .result_shown = false
};


// CALCULATOR KEY
static void calculator_key(napp_window* win, char key)
{
    calculator_state* calc = (calculator_state*)win->userdata;


    // numbers
    if(key >= '0' && key <= '9')
    {
        int len = calc_strlen(calc->display);

        if(calc->result_shown)
        {
            calc_strcpy(calc->display, "0");

            calc->result_shown = false;

            len = 1;
        }


        if(calc->operation != 0 && !calc->entering_second)
        {
            calc_strcpy(calc->display, "0");

            calc->entering_second = true;

            len = 1;
        }


        if(len < 62)
        {
            if(len == 1 && calc->display[0] == '0')
            {
                calc->display[0] = key;
            }
            else
            {
                calc->display[len] = key;
                calc->display[len + 1] = 0;
            }
        }

        return;
    }


    // AC
    if(key == 'A')
    {
        calc_strcpy(calc->display, "0");

        calc->value1 = 0;
        calc->value2 = 0;

        calc->operation = 0;

        calc->entering_second = false;
        calc->result_shown = false;

        return;
    }


    // CE
    if(key == 'E')
    {
        calc_strcpy(calc->display, "0");

        calc->entering_second = false;

        return;
    }


    // C
    if(key == 'C')
    {
        calc_strcpy(calc->display, "0");

        return;
    }


    // BACKSPACE
    if(key == 'B')
    {
        int len = calc_strlen(calc->display);

        if(len > 1)
        {
            calc->display[len - 1] = 0;
        }
        else
        {
            calc_strcpy(calc->display, "0");
        }

        return;
    }


    // PLUS / MINUS
    if(key == 'N')
    {
        if(calc->display[0] == '-')
        {
            int len = calc_strlen(calc->display);

            for(int i = 0; i < len; i++)
            {
                calc->display[i] = calc->display[i + 1];
            }
        }
        else if(calc->display[0] != '0')
        {
            int len = calc_strlen(calc->display);

            if(len < 62)
            {
                for(int i = len; i > 0; i--)
                {
                    calc->display[i] = calc->display[i - 1];
                }

                calc->display[0] = '-';
            }
        }

        return;
    }


    // PERCENT
    if(key == '%')
    {
        int value = calc_atoi(calc->display);
        value /= 100;
        char buf[32];

        calc_itoa(value, buf);
        calc_strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // SQUARE
    if(key == 'Q')
    {
        int value = calc_atoi(calc->display);
        int result = value * value;
        char buf[32];

        calc_itoa(result, buf);
        calc_strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // SQUARE ROOT
    if(key == 'R')
    {
        int value = calc_atoi(calc->display);

        if(value < 0)
        {
            calc_strcpy(calc->display, "Error");

            calc->result_shown = true;

            return;
        }


        int result = 0;

        while((result + 1) <= value / (result + 1))
        {
            result++;
        }


        char buf[32];

        calc_itoa(result, buf);
        calc_strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // 1 / X
    if(key == 'I')
    {
        int value = calc_atoi(calc->display);

        if(value == 0)
        {
            calc_strcpy(calc->display, "Error");

            calc->result_shown = true;

            return;
        }


        int result = 1 / value;
        char buf[32];

        calc_itoa(result, buf);
        calc_strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // OPERATORS
    if(key == '+' || key == '-' || key == '*' || key == '/')
    {
        calc->value1 = calc_atoi(calc->display);

        calc->operation = key;

        calc->entering_second = false;
        calc->result_shown = false;

        return;
    }


    // EQUAL
    if(key == '=')
    {
        if(calc->operation == 0)
        {
            return;
        }

        calc->value2 = calc_atoi(calc->display);
        int result = 0;


        switch(calc->operation)
        {
            case '+':
                result = calc->value1 + calc->value2;
                break;

            case '-':
                result = calc->value1 - calc->value2;
                break;

            case '*':
                result = calc->value1 * calc->value2;
                break;

            case '/':
                if(calc->value2 == 0)
                {
                    calc_strcpy(calc->display, "Error");

                    calc->operation = 0;
                    calc->result_shown = true;

                    return;
                }

                result = calc->value1 / calc->value2;

                break;
        }


        char buf[32];

        calc_itoa(result, buf);
        calc_strcpy(calc->display, buf);


        calc->value1 = result;
        calc->value2 = 0;

        calc->operation = 0;

        calc->entering_second = false;
        calc->result_shown = true;

        return;
    }
}


// BUTTON
static void draw_button(int x, int y, const char* text)
{
    gui->fill_block(x, y, NAPP_COLOR_TITLEBAR, 55, 35);
    gui->draw_text(text, x + 8, y + 11, NAPP_COLOR_WHITE);
}


// DRAW CALCULATOR
static void draw_calculator(napp_window* win)
{
    calculator_state* calc = (calculator_state*)win->userdata;


    // DISPLAY
    gui->fill_block(win->pos_x + 12, win->pos_y + win->title_height + 10, NAPP_COLOR_BLACK, win->width - 24, 42);
    gui->draw_text(calc->display, win->pos_x + 22, win->pos_y + win->title_height + 26, NAPP_COLOR_GREEN);


    // BUTTON GRID
    int x = win->pos_x + 12;
    int y = win->pos_y + win->title_height + 65;

    // ROW 1
    draw_button(x,       y, "AC");
    draw_button(x + 65,  y, "CE");
    draw_button(x + 130, y, "C");
    draw_button(x + 195, y, "<-");


    // ROW 2
    y += 43;

    draw_button(x,       y, "x2");
    draw_button(x + 65,  y, "1/x");
    draw_button(x + 130, y, "%");
    draw_button(x + 195, y, "/");


    // ROW 3
    y += 43;

    draw_button(x,       y, "7");
    draw_button(x + 65,  y, "8");
    draw_button(x + 130, y, "9");
    draw_button(x + 195, y, "*");


    // ROW 4
    y += 43;

    draw_button(x,       y, "4");
    draw_button(x + 65,  y, "5");
    draw_button(x + 130, y, "6");
    draw_button(x + 195, y, "-");


    // ROW 5
    y += 43;

    draw_button(x,       y, "1");
    draw_button(x + 65,  y, "2");
    draw_button(x + 130, y, "3");
    draw_button(x + 195, y, "+");


    // ROW 6
    y += 43;

    draw_button(x,       y, "0");
    draw_button(x + 65,  y, "+/-");
    draw_button(x + 130, y, "sqrt");
    draw_button(x + 195, y, "=");
}


// MOUSE
static void calculator_mouse(napp_window* win, int mx, int my)
{
    int x = win->pos_x + 12;
    int y = win->pos_y + win->title_height + 65;

    struct button
    {
        int x;
        int y;

        char key;
    };


    // EXACT SAME ORDER AS DRAW
    button buttons[] = {

        // ROW 1
        {0,   0,   'A'}, // AC
        {65,  0,   'E'}, // CE
        {130, 0,   'C'}, // C
        {195, 0,   'B'}, // <-

        // ROW 2
        {0,   43,  'Q'}, // x2
        {65,  43,  'I'}, // 1/x
        {130, 43,  '%'}, // %
        {195, 43,  '/'}, // /

        // ROW 3
        {0,   86,  '7'},
        {65,  86,  '8'},
        {130, 86,  '9'},
        {195, 86,  '*'},

        // ROW 4
        {0,   129, '4'},
        {65,  129, '5'},
        {130, 129, '6'},
        {195, 129, '-'},

        // ROW 5
        {0,   172, '1'},
        {65,  172, '2'},
        {130, 172, '3'},
        {195, 172, '+'},

        // ROW 6
        {0,   215, '0'},
        {65,  215, 'N'}, // +/-
        {130, 215, 'R'}, // sqrt
        {195, 215, '='}
    };


    const int button_count = (int)(sizeof(buttons) / sizeof(buttons[0]));
    char key = 0;


    for(int i = 0; i < button_count; i++)
    {
        int bx = x + buttons[i].x;
        int by = y + buttons[i].y;

        if(mx >= bx && mx < bx + 55 && my >= by && my < by + 35)
        {
            key = buttons[i].key;

            break;
        }
    }


    if(key)
    {
        calculator_key(win, key);
    }
}


int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION || api->gui == nullptr)
    {
        return 1;
    }

    gui = api->gui;

    napp_window_config config = {};

    config.title = "Calculator";

    config.width = 290;
    config.height = 390;

    config.resizable = false;
    config.can_maximize = false;

    config.userdata = &calc_data;

    config.draw = draw_calculator;
    config.key = calculator_key;
    config.mouse = calculator_mouse;

    if (!gui->open_window(&config))
    {
        api->serial_log("[CALCULATOR] Failed to open window\n");

        return 1;
    }

    api->serial_log("[CALCULATOR] Window opened\n");

    return 0;
}
