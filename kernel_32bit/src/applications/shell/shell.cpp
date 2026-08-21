#include "shell.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/keyboard/driver.hpp"

#include "system/sysfunc/sysfunc.hpp"

#include "libs/libc/libc.hpp"

void shell()
{
    char* cmd = Keyboard::input_buffer;

    if(strcmp(cmd,"help") == 0)
    {
        Gpu::print(
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
    else if(strcmp(cmd,"clear") == 0)
    {
        Gpu::clear_screen(Gpu::background_color);
        Gpu::cursor_x = 0;
        Gpu::cursor_y = 0;
    }
    else if(strcmp(cmd,"info") == 0)
    {
        Gpu::print(
            "Mode: protected mode\n"
            "Kernel: NasuaOS 32bit\n"
            "Graphics: framebuffer\n\n"
        );
    }
    else if(strcmp(cmd,"fetch") == 0)
    {
        Gpu::fetch();
    }
    else if(strcmp(cmd,"version") == 0)
    {
        Gpu::print(
            "NasuaOS 32bit\n"
            "Version: 0.8.0\n\n"
        );
    }
    else if(strcmp(cmd,"reboot") == 0)
    {
        reboot();
    }
    else if(strcmp(cmd,"halt") == 0)
    {
        halt();
    }
    else
    {
        Gpu::print("Unknown command\n");
    }
}
