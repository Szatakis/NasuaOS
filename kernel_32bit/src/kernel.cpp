#include <stdint.h>

// Definicje struktur Multiboot 1
struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
};

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;  // Liczba modułów
    uint32_t mods_addr;   // Adres tablicy modułów
    // Dalsze pola są nieistotne dla modułów
};

//Debug vars
bool safe_mode = false;

volatile uint16_t* vga = (uint16_t*)0xB8000;
uint32_t cursor = 0;
char buffer[128];

const char keymap[128] =
{
    0, 27,
    '1','2','3','4','5','6','7','8','9','0','-','=',
    8,9,

    'q','w','e','r','t','y','u','i','o','p','[',']',
    '\n',0,

    'a','s','d','f','g','h','j','k','l',';','\'',
    '`',0,'\\',

    'z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};


void scroll_screen()
{
    for(int y = 1; y < 25; y++)
    {
        for(int x = 0; x < 80; x++)
        {
            vga[(y - 1) * 80 + x] = vga[y * 80 + x];
        }
    }

    for(int x = 0; x < 80; x++)
    {
        vga[24 * 80 + x] = (0x07 << 8) | ' ';
    }

    cursor = 24 * 80;
}


void putchar(char c)
{
    if(c == '\n')
    {
        cursor = ((cursor / 80) + 1) * 80;

        if(cursor >= 80 * 25)
        {
            scroll_screen();
        }

        return;
    }

    vga[cursor] = (0x07 << 8) | c;
    cursor++;

    if(cursor >= 80 * 25)
    {
        scroll_screen();
    }
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
    print(" _   _                        ____   _____\n"
          "| \\ | |                      / __ \\ / ____|\n"
          "|  \\| | __ _ ___ _   _  __ _| |  | | (___\n"
          "| . ` |/ _` / __| | | |/ _` | |  | |\\___ \\\n"
          "| |\\  | (_| \\__ \\ |_| | (_| | |__| |____) |\n"
          "|_| \\_|\\__,_|___/\\__,_|\\__,_|\\____/|_____/\n\n");
}


void clear_screen()
{
    for(int i=0;i<2000;i++)
    {
        vga[i] = (0x07 << 8) | ' ';
    }

    cursor = 0;

    fetch();
}


void disable_cursor()
{
    asm volatile(
        "mov $0x3D4, %%dx\n"
        "mov $0x0A, %%al\n"
        "out %%al, %%dx\n"

        "inc %%dx\n"

        "mov $0x20, %%al\n"
        "out %%al, %%dx\n"

        :
        :
        :"ax","dx"
    );
}


void keyboard_init()
{
    uint8_t status;

    do
    {
        asm volatile("inb $0x64, %0" :"=a"(status));

    }while(status & 2);


    uint8_t cmd = 0xAE;

    asm volatile("outb %0,$0x64" : :"a"(cmd));

    uint8_t enable = 0xF4;

    asm volatile("outb %0,$0x60" : :"a"(enable)
    );
}


uint8_t read_scancode()
{
    uint8_t status;


    while(true)
    {
        asm volatile("inb $0x64,%0" :"=a"(status));


        if(status & 1)
        {
            uint8_t code;

            asm volatile("inb $0x60,%0" :"=a"(code));

            return code;
        }
    }
}


void read_line()
{
    int pos=0;

    while(true)
    {
        uint8_t sc = read_scancode();

        if(sc & 0x80)
        {
            continue;
        }


        if(sc == 0x1C)
        {
            buffer[pos]=0;
            putchar('\n');
            return;
        }


        if(sc == 0x0E)
        {
            if(pos > 0 && cursor > 0)
            {
                pos--;
                cursor--;

                vga[cursor] = (0x07 << 8) | ' ';
            }

            continue;
        }


        char c = keymap[sc];


        if(c)
        {
            buffer[pos++]=c;
            putchar(c);
        }
    }
}


bool strcmp(const char* a,const char* b)
{
    while(*a && *b)
    {
        if(*a!=*b)
        {
            return false;
        }

        a++;
        b++;
    }

    return *a==0 && *b==0;
}

bool contains(const char* text,const char* find)
{
    while(*text)
    {
        const char* a=text;
        const char* b=find;

        while(*a && *b && *a==*b)
        {
            a++;
            b++;
        }

        if(*b==0)
            return true;

        text++;
    }

    return false;
}

void reboot()
{
    uint8_t status;

    // Wait for controller to be ready
    do
    {
        asm volatile("inb $0x64, %0" : "=a"(status));
    } while (status & 0x02);

    // Send CPU reset cmd
    asm volatile(
        "movb $0xFE, %%al\n"
        "outb %%al, $0x64"
        :
        :
        : "ax"
    );

    // If reboot fail fallback
    while (true)
    {
        asm volatile("hlt");
    }
}

void halt()
{
    while(true)
    {
        asm volatile("hlt");
    }
}


void shell()
{
    while(true)
    {
        print("> ");
        read_line();

        if(strcmp(buffer,"help"))
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
        else if(strcmp(buffer,"clear"))
        {
            clear_screen();
        }
        else if(strcmp(buffer,"info"))
        {
            print(
                "Mode: protected mode\n"
                "Kernel: NasuaOS 32bit\n"
                "Kernel 32bit is basic shell not full os\n\n"
            );
        }
        else if(strcmp(buffer,"halt"))
        {
            halt();
        }
        else if(strcmp(buffer,"fetch")) 
        {
            fetch();
        }
        else if(strcmp(buffer,"version"))
        {
            print(
                "NasuaOS 32bit\n"
                "Version: 0.1\n\n"
            );
        }
        else if(strcmp(buffer,"reboot"))
        {
            reboot();
        }
        else
        {
            print("Unknown command\n");
        }
    }
}

extern "C" void kmain(struct multiboot_info* mbi)
{
    disable_cursor();
    keyboard_init();
    fetch();

    if (mbi->flags & (1 << 3))
    {
        if (mbi->mods_count > 0)
        {
            print("SAFE MODE ENABLED\n");
            safe_mode = true;
        }
    }

    shell();
}