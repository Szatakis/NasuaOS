#include "shell.hpp"

#include "../../system/drivers/video/driver.hpp"

#include "../../system/sysfunc/sysfunc.h"

#include "../../libs/libc/libc.hpp"

extern char input_buffer[128];
extern uint32_t cursor_x;
extern uint32_t cursor_y;

void shell()
{
    if(strcmp(input_buffer,"help"))
    {
        print(
            "Commands:\n"
            "help\n"
            "clear\n"
            "info\n"
            "fetch\n"
            "version\n"
            "reboot\n"
            "halt\n\n"
        );
    }
    else if(strcmp(input_buffer,"clear"))
    {
        clear_screen(background_color);
        cursor_x = 0;
        cursor_y = 0;
    }
    else if(strcmp(input_buffer,"info"))
    {
        print(
            "Mode: protected mode\n"
            "Kernel: NasuaOS 32bit\n"
            "Graphics: framebuffer\n\n"
        );
    }
    else if(strcmp(input_buffer,"fetch"))
    {
        fetch();
    }
    else if(strcmp(input_buffer,"version"))
    {
        print(
            "NasuaOS 32bit\n"
            "Version: 0.8.0\n\n"
        );
    }
    else if(strcmp(input_buffer,"reboot"))
    {
        reboot();
    }
    else if(strcmp(input_buffer,"halt"))
    {
        halt();
    }
    else
    {
        print("Unknown command\n");
    }
}
