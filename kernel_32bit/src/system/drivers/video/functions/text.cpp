#include "../driver.hpp"

uint32_t text_color = 0xFFFFFF;
uint32_t background_color = 0x000000;

extern framebuffer fb;

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern bool cursor_state;

void putchar(char c)
{

    clear_cursor();


    if(c == '\n')
    {
        cursor_x = 0;
        cursor_y += CHAR_HEIGHT;
    }
    else
    {

        draw_char(
            c,
            cursor_x,
            cursor_y,
            text_color
        );


        cursor_x += CHAR_WIDTH;

    }


    if(cursor_x >= fb.width)
    {
        cursor_x = 0;
        cursor_y += CHAR_HEIGHT;
    }


    if(cursor_y >= fb.height)
    {
        cursor_y = 0;
        clear_screen(background_color);
    }


    draw_cursor();
}

void print(const char* str)
{

    while(*str)
    {
        putchar(*str);
        str++;
    }

}

void fetch() 
{
    print("\n $$\\   $$\\                                          $$$$$$\\   $$$$$$\\  \n");
    print(" $$$\\  $$ |                                        $$  __$$\\ $$  __$$\\ \n");
    print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    print(" $$ $$\\$$ | \\____$$\\ $$  _____|$$ |  $$ | \\____$$\\ $$ |  $$ |\\$$$$$$\\  \n");
    print(" $$ \\$$$$ | $$$$$$$ |\\$$$$$$\\  $$ |  $$ | $$$$$$$ |$$ |  $$ | \\____$$\\ \n");
    print(" $$ |\\$$$ |$$  __$$ | \\____$$\\ $$ |  $$ |$$  __$$ |$$ |  $$ |$$\\   $$ |\n");
    print(" $$ | \\$$ |\\$$$$$$$ |$$$$$$$  |\\$$$$$$  |\\$$$$$$$ | $$$$$$  |\\$$$$$$  |\n");
    print(" \\__|  \\__| \\_______|\\_______/  \\______/  \\_______| \\______/  \\______/ \n\n");
}
