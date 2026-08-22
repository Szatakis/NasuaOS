#ifdef __i386__

#include <stdint.h>

#include "applications/shell/shell.hpp"

#include "system/drivers/drivers.hpp"
#include "system/sysfunc/sysfunc.hpp"

#include "../../resources/fonts/fonts.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

bool safe_mode = false;
bool debug_mode = false;

char Keyboard::input_buffer[128];
uint32_t Keyboard::input_pos = 0;

extern "C"
void kmain(uint32_t mbi)
{
    if(!Gpu::graphics_init(mbi))
    {
        while(1)
        {
            asm volatile("hlt");
        }
    }

    Gpu::clear_screen(0x000000);
    Keyboard::keyboard_init();

    Gpu::fetch();
    check_modules(mbi);

    while(true)
    {
        Gpu::print("> ");
        Keyboard::read_line();
        shell();
    }
}
#endif