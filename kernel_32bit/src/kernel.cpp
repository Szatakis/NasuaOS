#include <stdint.h>

#include "system/drivers/drivers.hpp"

#include "system/fonts/font8x8.h"

#include "libs/libc/libc.h"
#include "libs/asm/asm.h"

//DEBUG VARS
bool safe_mode = false;
bool debug_mode = false;


framebuffer fb;
uint32_t* buffer = nullptr;


uint32_t cursor_x = 0;
uint32_t cursor_y = 0;

uint32_t max_cols = 1280 / CHAR_WIDTH;
uint32_t max_rows = 720 / CHAR_HEIGHT;

bool cursor_state = true;

char input_buffer[128];

uint32_t input_pos = 0;

void check_modules(uint32_t mbi_addr)
{
    multiboot_tag* tag;


    for(
        tag = (multiboot_tag*)(mbi_addr + 8);
        tag->type != 0;
        tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))
    )
    {

        if(tag->type == 3)
        {
            multiboot_tag_module* module =
                (multiboot_tag_module*)tag;


            const char* data =
                (const char*)(uintptr_t)module->mod_start;



            if(contains(data,"SAFE_MODE"))
            {
                safe_mode = true;
            }


            if(contains(data,"DEBUG"))
            {
                debug_mode = true;
            }
        }
    }


    if(safe_mode)
    {
        print("Safe mode ON\n");
    }


    if(debug_mode)
    {
        print("Debug mode ON\n");
    }
}

void halt()
{
    while(true)
    {
        asm volatile("hlt");
    }
}

void reboot()
{
    uint8_t status;


    // czekaj aż kontroler będzie gotowy
    do
    {
        asm volatile(
            "inb $0x64, %0"
            : "=a"(status)
        );

    } while(status & 0x02);



    // reset CPU
    asm volatile(
        "movb $0xFE, %%al\n"
        "outb %%al, $0x64"
        :
        :
        : "ax"
    );


    // awaryjnie zatrzymaj CPU
    while(true)
    {
        asm volatile("hlt");
    }
}

void clear_char(uint32_t x,uint32_t y)
{
    for(uint32_t yy=0; yy<CHAR_HEIGHT; yy++)
    {
        for(uint32_t xx=0; xx<CHAR_WIDTH; xx++)
        {
            put_pixel(
                x+xx,
                y+yy,
                background_color
            );
        }
    }
}

void backspace()
{
    if(input_pos == 0)
        return;


    input_pos--;

    if(cursor_x >= CHAR_WIDTH)
    {
        cursor_x -= CHAR_WIDTH;
    }


    clear_char(
        cursor_x,
        cursor_y
    );
}

void read_line()
{

    input_pos = 0;


    while(true)
    {

        uint8_t sc = read_scancode();


        if(sc & 0x80)
            continue;



        if(sc == 0x1C)
        {
            input_buffer[input_pos]=0;

            putchar('\n');

            return;
        }



        if(sc == 0x0E)
        {
            backspace();
            continue;
        }



        char c = keymap[sc];


        if(c)
        {

            if(input_pos < 127)
            {

                input_buffer[input_pos++] = c;


                putchar(c);

            }

        }

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
            "Version: 0.1\n\n"
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

bool graphics_init(uint32_t mbi_addr)
{

    multiboot_tag* tag;


    for(
        tag = (multiboot_tag*)(mbi_addr + 8);
        tag->type != 0;
        tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))
    )
    {


        if(tag->type == 8)
        {

            auto frame =
            (multiboot_tag_framebuffer*)tag;



            fb.addr =
                frame->framebuffer_addr;


            fb.pitch =
                frame->framebuffer_pitch;


            fb.width =
                frame->framebuffer_width;


            fb.height =
                frame->framebuffer_height;


            fb.bpp =
                frame->framebuffer_bpp;



            buffer =
            (uint32_t*)(uintptr_t)fb.addr;

            max_cols = fb.width / CHAR_WIDTH;
            max_rows = fb.height / CHAR_HEIGHT;

            return true;

        }

    }


    return false;

}



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