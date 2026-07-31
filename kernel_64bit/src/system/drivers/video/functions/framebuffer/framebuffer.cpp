#include "../../driver.hpp"

#include "system/gui/vars/colors.hpp"
#include "applications/shell/commands.hpp"

// Deklaracja funkcji inicjalizującej bufor terminala z pliku tekstowego
extern void init_terminal_buffer();

limine_framebuffer* fb = nullptr;

// Backbuffer: static allocation for double buffering
// Maximum resolution support: 4096x2160 @ 32bpp = ~33 MB
static uint32_t backbuffer[4096 * 2160];
static size_t backbuffer_width = 0;
static size_t backbuffer_height = 0;
static size_t backbuffer_pitch = 0;

void init_backbuffer(size_t width, size_t height, size_t pitch) 
{
    backbuffer_width = width;
    backbuffer_height = height;
    backbuffer_pitch = pitch / 4;  // Convert to pixel pitch
}

uint32_t* get_backbuffer() 
{
    return backbuffer;
}

size_t get_backbuffer_pitch() 
{
    return backbuffer_pitch;
}

// Skopiowanie całego backbuffera do fizycznego framebuffera (VRAM)
void render_frame() 
{
    if (!fb || !fb->address) return;

    uint32_t* fb_ptr = (uint32_t*)fb->address;
    size_t fb_pitch = fb->pitch / 4; // Pitch fizycznego ekranu w pikselach

    // Optymalizacja: Kopiujemy całe linie pamięci zamiast pojedynczych pikseli
    for (size_t y = 0; y < backbuffer_height; y++) 
    {
        __builtin_memcpy(
            &fb_ptr[y * fb_pitch], 
            &backbuffer[y * backbuffer_pitch], 
            backbuffer_width * sizeof(uint32_t)
        );
    }
}

// Czyszczenie pamięci roboczej ekranu oraz wirtualnego bufora konsoli
void clear_screen() 
{
    if (!fb) return;

    // 2. Wypełnienie całego backbuffera jednolitym kolorem tła systemu
    uint32_t* bb_ptr = backbuffer;
    uint32_t bg_color = COLOR_NASUA_BG;

    for (size_t y = 0; y < backbuffer_height; y++) 
    {
        for (size_t x = 0; x < backbuffer_width; x++) 
        {
            bb_ptr[y * backbuffer_pitch + x] = bg_color;
        }
    }
}
