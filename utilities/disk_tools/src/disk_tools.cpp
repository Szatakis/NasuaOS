#include <stdint.h>

#include "font8x8.h"


// MULTIBOOT2
#define MULTIBOOT2_MAGIC     0x36D76289
#define MB2_TAG_END          0
#define MB2_TAG_FRAMEBUFFER  8


// MULTIBOOT2 STRUCTURES
struct mb2_tag
{
    uint32_t type;
    uint32_t size;
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


// FRAMEBUFFER
static uint8_t* framebuffer = nullptr;

static uint32_t fb_width  = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch  = 0;
static uint32_t fb_bpp    = 0;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;


// COLORS
static const uint32_t COLOR_BLACK = 0x00101820;
static const uint32_t COLOR_WHITE = 0x00FFFFFF;


// PUT PIXEL
static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height)
    {
        return;
    }

    uint32_t* pixel = (uint32_t*)(framebuffer + y * fb_pitch + x * 4);

    *pixel = color;
}


// CLEAR SCREEN
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


// DRAW CHARACTER
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


// PRINT
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


// FIND FRAMEBUFFER
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

            if (fb->framebuffer_type != 1)
            {
                return false;
            }

            if (fb->framebuffer_bpp != 32)
            {
                return false;
            }

            framebuffer = (uint8_t*)(uint32_t)fb->framebuffer_addr;
            fb_width = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch = fb->framebuffer_pitch;
            fb_bpp = fb->framebuffer_bpp;

            return true;
        }

        current += (tag->size + 7) & ~7;
    }

    return false;
}


// KMAIN
extern "C" void kmain(uint32_t magic, uint32_t mb_info)
{
    if (magic != MULTIBOOT2_MAGIC)
    {
        return;
    }

    if (!find_framebuffer(mb_info))
    {
        return;
    }

    clear_screen(COLOR_BLACK);

    cursor_x = 0;
    cursor_y = 10;

    print("Disk Tools", COLOR_WHITE);

    for (;;)
    {
        asm volatile("hlt");
    }
}