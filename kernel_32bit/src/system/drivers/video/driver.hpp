#include <stdint.h>

struct framebuffer
{
    uint64_t addr;

    uint32_t pitch;

    uint32_t width;

    uint32_t height;

    uint8_t bpp;

    uint8_t type;

};

extern uint32_t text_color;
extern uint32_t background_color;

const uint32_t CHAR_WIDTH = 8;
const uint32_t CHAR_HEIGHT = 8;

void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void draw_cursor();
void draw_char(char c,uint32_t x, uint32_t y, uint32_t color);
void clear_cursor();
void putchar(char c);
void clear_char(uint32_t x,uint32_t y);
void print(const char* str);
bool graphics_init(uint32_t mbi_addr);
void fetch();