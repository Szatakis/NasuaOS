#include "../driver.hpp"

const char keymap[128] =
{
    0,27,
    '1','2','3','4','5','6','7','8','9','0','-','=',
    8,9,

    'q','w','e','r','t','y','u','i','o','p','[',']',
    '\n',0,

    'a','s','d','f','g','h','j','k','l',';','\'',
    '`',0,'\\',

    'z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

void keyboard_init()
{
    uint8_t status;


    // czekaj aż kontroler będzie gotowy
    do
    {
        asm volatile(
            "inb $0x64, %0"
            : "=a"(status)
        );

    } while(status & 2);



    // włącz klawiaturę
    uint8_t cmd = 0xAE;

    asm volatile(
        "outb %0, $0x64"
        :
        : "a"(cmd)
    );



    // włącz skanowanie klawiatury
    uint8_t enable = 0xF4;

    asm volatile(
        "outb %0, $0x60"
        :
        : "a"(enable)
    );
}

uint8_t read_scancode()
{
    uint8_t status;


    while(true)
    {

        asm volatile(
            "inb $0x64, %0"
            : "=a"(status)
        );


        // dane w buforze klawiatury
        if(status & 1)
        {

            uint8_t code;


            asm volatile(
                "inb $0x60, %0"
                : "=a"(code)
            );


            return code;
        }

    }
}