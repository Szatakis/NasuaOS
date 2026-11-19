#include <stdint.h>

#include "../../../../resources/fonts/font8x8.h"


#define MULTIBOOT2_MAGIC     0x36D76289

#define MB2_TAG_END          0
#define MB2_TAG_MODULE       3
#define MB2_TAG_FRAMEBUFFER  8


struct mb2_tag
{
    uint32_t type;
    uint32_t size;
};


struct mb2_tag_module
{
    uint32_t type;
    uint32_t size;

    uint32_t mod_start;
    uint32_t mod_end;

    char cmdline[0];
};


struct mb2_tag_framebuffer
{
    uint32_t type;
    uint32_t size;

    uint64_t framebuffer_addr;

    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;

    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;

    uint16_t reserved;
};


static uint8_t* framebuffer = nullptr;

static uint32_t fb_width  = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch  = 0;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;


static const uint32_t COLOR_BLACK = 0x00101820;
static const uint32_t COLOR_WHITE = 0x00FFFFFF;
static const uint32_t COLOR_GREEN = 0x0000FF00;
static const uint32_t COLOR_RED   = 0x00FF0000;
static const uint32_t COLOR_SEA   = 0x0063C7B7;


static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!framebuffer || x >= fb_width || y >= fb_height)
    {
        return;
    }

    uint32_t* pixel = (uint32_t*)(framebuffer + y * fb_pitch + x * 4);

    *pixel = color;
}


static void clear_screen(uint32_t color)
{
    for (uint32_t y = 0; y < fb_height; y++)
    {
        for (uint32_t x = 0; x < fb_width; x++)
        {
            put_pixel(x, y, color);
        }
    }
}


static void draw_char(char c, uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t ch = (uint8_t)c;

    for (uint32_t row = 0; row < 8; row++)
    {
        uint8_t bits = font8x8[ch][row];

        for (uint32_t col = 0; col < 8; col++)
        {
            if (bits & (1 << (7 - col)))
            {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}


static void print(const char* text, uint32_t color)
{
    while (*text)
    {
        char c = *text++;

        if (c == '\n')
        {
            cursor_x = 0;
            cursor_y += 12;
            continue;
        }

        draw_char(c, cursor_x, cursor_y, color);

        cursor_x += 8;

        if (cursor_x + 8 >= fb_width)
        {
            cursor_x = 0;
            cursor_y += 12;
        }
    }
}


static void print_hex32(uint32_t value)
{
    const char* hex = "0123456789ABCDEF";

    char buffer[11];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; i++)
    {
        buffer[2 + i] = hex[(value >> ((7 - i) * 4)) & 0xF];
    }

    buffer[10] = '\0';

    print(buffer, COLOR_WHITE);
}


static bool find_framebuffer(uint32_t mb_info)
{
    uint32_t total_size = *(uint32_t*)mb_info;
    uint8_t* current = (uint8_t*)mb_info + 8;
    uint8_t* end = (uint8_t*)mb_info + total_size;


    while (current < end)
    {
        mb2_tag* tag = (mb2_tag*)current;


        if (tag->type == MB2_TAG_END)
        {
            break;
        }


        if (tag->type == MB2_TAG_FRAMEBUFFER)
        {
            mb2_tag_framebuffer* fb = (mb2_tag_framebuffer*)tag;


            if (fb->framebuffer_type != 1 || fb->framebuffer_bpp != 32)
            {
                return false;
            }

            framebuffer = (uint8_t*)(uint32_t)fb->framebuffer_addr;
            fb_width = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch = fb->framebuffer_pitch;


            return true;
        }


        current += (tag->size + 7) & ~7;
    }


    return false;
}



static mb2_tag_module* modules[8];
static uint32_t module_count = 0;

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}


static bool find_modules(uint32_t mb_info)
{
    uint32_t total_size = *(uint32_t*)mb_info;
    uint8_t* current = (uint8_t*)mb_info + 8;
    uint8_t* end = (uint8_t*)mb_info + total_size;


    while (current < end)
    {
        mb2_tag* tag = (mb2_tag*)current;


        if (tag->type == MB2_TAG_END)
        {
            break;
        }


        if (tag->type == MB2_TAG_MODULE)
        {
            if (module_count < 8)
            {
                modules[module_count++] = (mb2_tag_module*)tag;
            }
        }


        current += (tag->size + 7) & ~7;
    }


    return module_count >= 2;
}

void start_kernel32bit(uint32_t mb_info)
{
    (void)mb_info;
    clear_screen(COLOR_BLACK);
    cursor_x = 20;
    cursor_y = 40;
    print("32-bit boot cannot be launched from inside this tool.\n", COLOR_RED);
    print("Restart and select NasuaOS 32-bit from the Limine menu.\n", COLOR_WHITE);
    for (;;)
    {
        asm volatile("hlt");
    }
    for (;;)
    {
        asm volatile("hlt");
    }
}

static uint8_t read_keyboard_scancode()
{
    while ((inb(0x64) & 1) == 0)
    {
        asm volatile("pause");
    }
    return inb(0x60);
}

static int choose_boot_target()
{
    int selected = 0;
    bool redraw = true;
    for (;;)
    {
        if (redraw)
        {
            clear_screen(COLOR_BLACK);
            cursor_x = 20;
            cursor_y = 40;
            print("NasuaOS Boot Manager\n", COLOR_SEA);
            print("--------------------\n\n", COLOR_WHITE);
            print(selected == 0 ? "> 1. NasuaOS 32-bit\n" : "  1. NasuaOS 32-bit\n",
                  selected == 0 ? COLOR_SEA : COLOR_WHITE);
            print(selected == 1 ? "> 2. NasuaOS 64-bit\n" :
                  "  2. NasuaOS 64-bit (requires Limine)\n",
                  selected == 1 ? COLOR_SEA : COLOR_WHITE);
            print("\nUse arrows or 1/2, Enter to start.\n", COLOR_WHITE);
            redraw = false;
        }

        uint8_t scancode = read_keyboard_scancode();
        if (scancode & 0x80)
        {
            continue;
        }
        if (scancode == 0x48 || scancode == 0x50)
        {
            selected = 1 - selected;
            redraw = true;
        }
        else if (scancode == 0x02)
        {
            selected = 0;
            redraw = true;
        }
        else if (scancode == 0x03)
        {
            selected = 1;
            redraw = true;
        }
        else if (scancode == 0x1C)
        {
            return selected;
        }
    }
}

extern "C" void kmain(uint32_t magic, uint32_t mb_info)
{
    if (magic != MULTIBOOT2_MAGIC)
    {
        for (;;)
        {
            asm volatile("hlt");
        }
    }


    find_framebuffer(mb_info);

    if (!find_modules(mb_info))
    {
        print("ERROR: kernel modules not found\n", COLOR_RED);

        for (;;)
        {
            asm volatile("hlt");
        }
    }


    print("kernel_32bit: ", COLOR_WHITE);
    print_hex32(modules[0]->mod_start);
    print("\n", COLOR_WHITE);
    print("kernel_64bit: ", COLOR_WHITE);
    print_hex32(modules[1]->mod_start);
    print("\n\n", COLOR_WHITE);


    const int target = choose_boot_target();
    if (target == 0)
    {
        print("\nLoading 32-bit kernel...\n", COLOR_GREEN);
        start_kernel32bit(mb_info);
    }

    clear_screen(COLOR_BLACK);
    cursor_x = 20;
    cursor_y = 40;
    print("64-bit kernel requires the Limine boot manager.\n", COLOR_RED);
    print("Restart and select NasuaOS 64-bit from the Limine menu.\n", COLOR_WHITE);
    for (;;)
    {
        asm volatile("hlt");
    }
}