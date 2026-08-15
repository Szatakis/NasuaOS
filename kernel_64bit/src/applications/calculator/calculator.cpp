#include "calculator.hpp"

#include "system/gui/gui.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.hpp"


struct calculator_state {
    char display[64];

    int value1;
    int value2;

    char operation;

    bool entering_second;
};

static calculator_state calc_data = {
    .display = "0",
    .value1 = 0,
    .value2 = 0,
    .operation = 0,
    .entering_second = false
};



void calculator_key(window_struct* win, char key) {
    calculator_state* calc =
        (calculator_state*)win->userdata;


    // numbers
    if(key >= '0' && key <= '9') {
        size_t len = strlen(calc->display);

        if(len < 62) {
            if(len == 1 && calc->display[0] == '0') {
                calc->display[0] = key;
            }
            else 
            {
                calc->display[len] = key;
                calc->display[len+1] = 0;
            }
        }

        return;
    }


    // clean
    if(key == 'c' || key == 'C') {
        strcpy(calc->display,"0");

        calc->value1 = 0;
        calc->value2 = 0;
        calc->operation = 0;
        calc->entering_second = false;

        return;
    }


    // operators
    if(key=='+' || key=='-' || key=='*' || key=='/') {
        calc->value1 = atoi(calc->display);

        calc->operation = key;

        calc->entering_second = true;

        strcpy(calc->display,"0");

        return;
    }

    // resoult
    if(key=='=') {
        calc->value2 = atoi(calc->display);

        int result = 0;

        switch(calc->operation) {
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
                if(calc->value2 != 0)
                    result = calc->value1 / calc->value2;
                break;
        }


        char buf[32];

        itoa(result,buf);
        strcpy(calc->display,buf);


        calc->entering_second=false;
        calc->operation=0;
    }
}


void draw_button(int x, int y, const char* text) {
    fill_block(x, y, COLOR_TITLEBAR, 50, 35);
    print_at8(text, x+15, y+12, COLOR_WHITE);
}


void draw_calculator(window_struct* win) {

    calculator_state* calc = (calculator_state*)win->userdata;
    int title = win->height/10;


    if(title < 18) {
        title = 18;
    }

    // display
    fill_block(win->pos_x+15, win->pos_y+title+10, COLOR_BLACK, 220, 40);
    print_at8(calc->display, win->pos_x+25, win->pos_y+title+25, COLOR_GREEN);



    int x = win->pos_x+10;
    int y = win->pos_y+title+70;


    draw_button(x,y,"7");
    draw_button(x+60,y,"8");
    draw_button(x+120,y,"9");
    draw_button(x+180,y,"/");


    y+=45;

    draw_button(x,y,"4");
    draw_button(x+60,y,"5");
    draw_button(x+120,y,"6");
    draw_button(x+180,y,"*");


    y+=45;

    draw_button(x,y,"1");
    draw_button(x+60,y,"2");
    draw_button(x+120,y,"3");
    draw_button(x+180,y,"-");


    y+=45;

    draw_button(x,y,"0");
    draw_button(x+60,y,"C");
    draw_button(x+120,y,"=");
    draw_button(x+180,y,"+");
}



void calculator_mouse(window_struct* win, int mx, int my) 
{
    calculator_state* calc = (calculator_state*)win->userdata;

    (void)calc;

    int title = win->height / 10;

    if(title < 18) 
    {
        title = 18;
    }


    int x = win->pos_x + 10;
    int y = win->pos_y + title + 70;

    char key = 0;

    struct button 
    {
        int x;
        int y;
        char key;
    };


    button buttons[] = 
    {
        {0,0,'7'},
        {60,0,'8'},
        {120,0,'9'},
        {180,0,'/'},

        {0,45,'4'},
        {60,45,'5'},
        {120,45,'6'},
        {180,45,'*'},

        {0,90,'1'},
        {60,90,'2'},
        {120,90,'3'},
        {180,90,'-'},

        {0,135,'0'},
        {60,135,'C'},
        {120,135,'='},
        {180,135,'+'}
    };


    for(size_t i=0;i<16;i++) 
    {
        int bx = x + buttons[i].x;
        int by = y + buttons[i].y;

        if(mx >= bx && mx < bx + 50 && my >= by && my < by + 35) 
        {
            key = buttons[i].key;
            break;
        }
    }

    if(key) 
    {
        calculator_key(win,key);
    }
}


window_struct calculator = {
    .name="Calculator",
    .id=0,

    .pos_x=10,
    .pos_y=10,
    .width=250,
    .height=320,

    .visible=false,
    .minimized=false,
    .focused=false,
    .can_maximize=false,
    .maximized=false,
    .restore_pos_x=0,
    .restore_pos_y=0,
    .restore_width=0,
    .restore_height=0,

    .is_dragging=false,
    .drag_offset_x=0,
    .drag_offset_y=0,

    .userdata=&calc_data,
    .draw_content=draw_calculator,
    .key_press=calculator_key,
    .mouse_click=calculator_mouse
};