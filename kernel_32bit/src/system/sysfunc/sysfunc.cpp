#include "sysfunc.hpp"

#include "../../libs/libc/libc.hpp"
#include "../drivers/video/driver.hpp"
#include "../drivers/multiboot/multiboot.hpp"

extern bool safe_mode;
extern bool debug_mode;

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
