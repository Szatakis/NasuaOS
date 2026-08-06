#include <stdint.h>

// Definicje struktur Multiboot 1
struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
};

struct multiboot_info
{
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;
    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    uint32_t syms[4];

    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;

    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed));

struct multiboot_vbe
{
    uint32_t control_info;
    uint32_t mode_info;
    uint16_t mode;
    uint16_t interface_seg;
    uint16_t interface_off;
    uint16_t interface_len;
};

struct vbe_mode_info
{
    uint16_t attributes;
    uint8_t winA;
    uint8_t winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA;
    uint16_t segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;

    uint16_t width;
    uint16_t height;

    uint8_t wChar;
    uint8_t yChar;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memoryModel;
    uint8_t bankSize;
    uint8_t imagePages;
    uint8_t reserved0;

    uint8_t redMask;
    uint8_t redPosition;

    uint8_t greenMask;
    uint8_t greenPosition;

    uint8_t blueMask;
    uint8_t bluePosition;

    uint8_t reservedMask;
    uint8_t reservedPosition;

    uint8_t directColorAttributes;

    uint32_t framebuffer;

} __attribute__((packed));

uint32_t syms1;
uint32_t syms2;
uint32_t syms3;
uint32_t syms4;

uint32_t mmap_length;
uint32_t mmap_addr;

uint32_t drives_length;
uint32_t drives_addr;

uint32_t config_table;

uint32_t boot_loader_name;

uint32_t apm_table;

multiboot_vbe* vbe;

uint32_t* framebuffer = nullptr;

uint32_t screen_width = 0;
uint32_t screen_height = 0;
uint32_t screen_pitch = 0;

bool graphics_init(multiboot_info* mbi)
{
    if (!(mbi->flags & (1 << 11)))
        return false;

    auto mode = (vbe_mode_info*)(uintptr_t)mbi->vbe_mode_info;

    framebuffer = (uint32_t*)(uintptr_t)mode->framebuffer;

    screen_width = mode->width;
    screen_height = mode->height;
    screen_pitch = mode->pitch / 4;

    return true;
}

//Debug vars
bool safe_mode = false;
bool debug_mode = false;

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


void hck_debug(struct multiboot_info* mbi)
{
    uint32_t count = mbi->mods_count;
    struct multiboot_module* mods = (struct multiboot_module*)mbi->mods_addr;

    for (uint32_t i = 0; i < count; i++) 
    {
        const char* data = (char*)mods[i].mod_start;
        if(contains(data, "SAFE_MODE"))
        {
            print("Safe mode ON\n");
            safe_mode = true;
        }
        if(contains(data, "DEBUG"))
        {
            print("Debug mode ON\n");
            debug_mode = true;
        }
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

extern "C" void kmain(multiboot_info* mbi)
{
    graphics_init(mbi);

    disable_cursor();
    keyboard_init();

    fetch();

    hck_debug(mbi);

    shell();
}