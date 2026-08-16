#include "calculator.hpp"

#include "system/gui/gui.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.hpp"

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
    │  0   │ +/-  │ √    │ =  │
    └──────┴──────┴──────┴────┘
*/


struct calculator_state {
    char display[64];

    int value1;
    int value2;

    char operation;

    bool entering_second;
    bool result_shown;
};


static calculator_state calc_data = {
    .display = "0",

    .value1 = 0,
    .value2 = 0,

    .operation = 0,

    .entering_second = false,
    .result_shown = false
};


// CALCULATOR KEY
void calculator_key(window_struct* win, char key) 
{

    calculator_state* calc = (calculator_state*)win->userdata;


    // numbers
    if(key >= '0' && key <= '9') 
    {

        size_t len = strlen(calc->display);

        if(calc->result_shown) 
        {
            strcpy(calc->display, "0");

            calc->result_shown = false;

            len = 1;
        }


        if(calc->operation != 0 && !calc->entering_second) 
        {
            strcpy(calc->display, "0");

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
        strcpy(calc->display, "0");

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
        strcpy(calc->display, "0");

        calc->entering_second = false;

        return;
    }


    // C
    if(key == 'C') 
    {
        strcpy(calc->display, "0");

        return;
    }


    // BACKSPACE
    if(key == 'B') 
    {
        size_t len = strlen(calc->display);

        if(len > 1) 
        {
            calc->display[len - 1] = 0;
        }
        else 
        {
            strcpy(calc->display, "0");
        }

        return;
    }


    // PLUS / MINUS
    if(key == 'N') 
    {
        if(calc->display[0] == '-') 
        {
            size_t len = strlen(calc->display);

            for(size_t i = 0; i < len; i++) 
            {
                calc->display[i] = calc->display[i + 1];
            }
        }
        else if(calc->display[0] != '0') 
        {
            size_t len = strlen(calc->display);

            if(len < 62) 
            {
                for(size_t i = len; i > 0; i--) 
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
        int value = atoi(calc->display);
        value /= 100;
        char buf[32];

        itoa(value, buf);
        strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // SQUARE
    if(key == 'Q') 
    {
        int value = atoi(calc->display);
        int result = value * value;
        char buf[32];

        itoa(result, buf);
        strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // SQUARE ROOT
    if(key == 'R') 
    {
        int value = atoi(calc->display);

        if(value < 0) 
        {
            strcpy(calc->display, "Error");

            calc->result_shown = true;

            return;
        }


        int result = 0;

        while((result + 1) <= value / (result + 1)) 
        {
            result++;
        }


        char buf[32];

        itoa(result, buf);
        strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // 1 / X
    if(key == 'I') 
    {

        int value = atoi(calc->display);

        if(value == 0) 
        {
            strcpy(calc->display, "Error");

            calc->result_shown = true;

            return;
        }


        int result = 1 / value;
        char buf[32];

        itoa(result, buf);
        strcpy(calc->display, buf);

        calc->result_shown = true;

        return;
    }


    // OPERATORS
    if(key == '+' || key == '-' || key == '*' || key == '/') 
    {
        calc->value1 = atoi(calc->display);

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

        calc->value2 = atoi(calc->display);
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
                    strcpy(calc->display, "Error");

                    calc->operation = 0;
                    calc->result_shown = true;

                    return;
                }

                result = calc->value1 / calc->value2;

                break;
        }


        char buf[32];

        itoa(result, buf);
        strcpy(calc->display, buf);


        calc->value1 = result;
        calc->value2 = 0;

        calc->operation = 0;

        calc->entering_second = false;
        calc->result_shown = true;

        return;
    }
}


// BUTTON
void draw_button(int x, int y, const char* text) 
{
    fill_block(x, y, COLOR_TITLEBAR, 55, 35);
    print_at8(text, x + 8, y + 11, COLOR_WHITE);
}


// DRAW CALCULATOR
void draw_calculator(window_struct* win) 
{
    calculator_state* calc = (calculator_state*)win->userdata;
    int title = win->height / 10;

    if(title < 18)
    {
        title = 18;
    }


    // DISPLAY
    fill_block(win->pos_x + 12, win->pos_y + title + 10, COLOR_BLACK, win->width - 24, 42);
    print_at8(calc->display, win->pos_x + 22, win->pos_y + title + 26, COLOR_GREEN);


    // BUTTON GRID
    int x = win->pos_x + 12;
    int y = win->pos_y + title + 65;

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
void calculator_mouse(window_struct* win, int mx, int my) 
{
    int title = win->height / 10;

    if(title < 18)
    {
        title = 18;
    }

    int x = win->pos_x + 12;
    int y = win->pos_y + title + 65;

    struct button {
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


    const size_t button_count = sizeof(buttons) / sizeof(buttons[0]);
    char key = 0;


    for(size_t i = 0; i < button_count; i++) 
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


// CALCULATOR WINDOW
window_struct calculator = {
    .name = "Calculator",
    .id = 0,

    .pos_x = 10,
    .pos_y = 10,

    .width = 290,
    .height = 390,

    .visible = false,
    .minimized = false,
    .focused = false,

    .resizable = false,
    .can_maximize = false,
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
    .resize_start_mouse_x = 290,
    .resize_start_mouse_y = 390,
    .resize_start_width = 10,
    .resize_start_height = 20,

    .userdata = &calc_data,
    .draw_content = draw_calculator,
    .key_press = calculator_key,
    .mouse_click = calculator_mouse
};