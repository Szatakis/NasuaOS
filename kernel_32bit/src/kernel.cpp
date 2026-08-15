#include <stdint.h>

#include "applications/shell/shell.hpp"

#include "system/drivers/drivers.hpp"
#include "system/sysfunc/sysfunc.hpp"

#include "system/fonts/font8x8.h"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

//DEBUG VARS
bool safe_mode = false;
bool debug_mode = false;

//Sys vars
framebuffer fb;
uint32_t* buffer = nullptr;


uint32_t cursor_x = 0;
uint32_t cursor_y = 0;

uint32_t max_cols = 1280 / CHAR_WIDTH;
uint32_t max_rows = 720 / CHAR_HEIGHT;

bool cursor_state = true;

char input_buffer[128];

uint32_t input_pos = 0;

extern "C"
void kmain(uint32_t mbi)
{


    if(!graphics_init(mbi))
    {

        while(1)
            asm volatile("hlt");

    }

    clear_screen(0x000000);
    keyboard_init();


    fetch();
    check_modules(mbi);

    while(true)
    {
        print("> ");
        read_line();
        shell();
    }

}