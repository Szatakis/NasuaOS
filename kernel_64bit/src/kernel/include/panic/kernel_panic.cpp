#include "kernel_panic.hpp"

#include "system/drivers/video/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/drivers/cpu/driver.hpp"
#include "system/drivers/timer/driver.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.h"
#include "libs/asm/asm.h"

// Layout constants
#define CH_W 9
#define CH_H 13

#define BOX_COLS 58
#define BOX_ROWS 16

// Derived pixel dimensions
#define HORIZ_PAD 8
#define BOX_PX_W (BOX_COLS * CH_W + (HORIZ_PAD * 2))
#define BOX_PX_H (BOX_ROWS * CH_H + 4)

// Color Palette (Modern Dark / Nord-inspired Crash Screen)
#define COL_BG          0xFF181C22 
#define COL_PANEL       0xFF21262D 
#define COL_BORDER      0xFF30363D 
#define COL_HEADER      0xFFDA3633 
#define COL_TEXT_DIM    0xFF8B949E 
#define COL_TEXT_BRIGHT 0xFFF0F6FC 

// State tracking
static size_t box_px;
static size_t box_py;
static size_t prow;

static inline size_t row_y()
{
    return box_py + 2 + prow * CH_H;
}

// Safely draw a string with bounds checking
static void draw_string_clipped(size_t x, size_t y, const char* str, uint32_t color, size_t max_chars)
{
    size_t drawn = 0;
    while (*str && drawn < max_chars)
    {
        draw_char8((unsigned char)*str, x, y, color);
        x += CH_W;
        str++;
        drawn++;
    }
}

// Draw a full-width horizontal structural bar
static void panic_fill_row(uint32_t color)
{
    fill_block(box_px, row_y(), color, BOX_PX_W, CH_H);
    prow++;
}

// Draw a standard text row padded inside the structural side borders
static void panic_text_row(const char* text, uint32_t text_color)
{
    size_t y = row_y();
    size_t x = box_px;

    // Left border stripe
    fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD;

    // Background fill for text area
    fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    // Draw text content
    draw_string_clipped(x, y + 2, text ? text : "", text_color, BOX_COLS);

    // Right border stripe
    x += BOX_COLS * CH_W;
    fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

// Centered text row (fixed centering calculation)
static void panic_center_row(const char* text, uint32_t text_color)
{
    size_t len = text ? strlen(text) : 0;
    // Calculate character padding relative to BOX_COLS available text space
    size_t pad_chars = (len < (size_t)BOX_COLS) ? (BOX_COLS - len) / 2 : 0;
    
    size_t y = row_y();
    size_t x = box_px;

    // Left border
    fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD;

    // Fill the inner content row background
    fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    // Draw centered text precisely positioned inside the inner panel area
    draw_string_clipped(x + (pad_chars * CH_W), y + 2, text ? text : "", text_color, BOX_COLS - pad_chars);

    // Right border
    fill_block(box_px + BOX_PX_W - HORIZ_PAD, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

// Structured Key-Value row with proper inner-box alignment
static void panic_kv_row(const char* key, const char* value)
{
    size_t y = row_y();
    size_t x = box_px;

    // Left border
    fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD; // x is now at the start of the inner panel content area

    // Background panel fill
    fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    // Draw Key (Dimmed) at a fixed offset from the inner panel start
    draw_string_clipped(x + CH_W, y + 2, key ? key : "", COL_TEXT_DIM, 15);

    // Draw Value (Bright) aligned further across
    size_t val_offset = 16 * CH_W;
    draw_string_clipped(x + val_offset, y + 2, (value && *value) ? value : "N/A", COL_TEXT_BRIGHT, BOX_COLS - 16);

    // Right border
    fill_block(box_px + BOX_PX_W - HORIZ_PAD, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

// Kernel Panic
void kernel_panic(const char* message, const char* error_code, const char* rip, const char* rsp, const char* fault_address, const char* pid)
{
    kernel_panicked = true;

    char cpu_name[49];
    memset(cpu_name, 0, sizeof(cpu_name));
    cpu_get_brand(cpu_name);

    clear_screen();

    size_t scr_w = fb ? fb->width : 800;
    size_t scr_h = fb ? fb->height : 600;

    box_px = (scr_w > (size_t)BOX_PX_W) ? (scr_w - BOX_PX_W) / 2 : 0;
    box_py = (scr_h > (size_t)BOX_PX_H) ? (scr_h - BOX_PX_H) / 2 : 0;
    prow = 0;

    fill_block(box_px, box_py, COL_PANEL, BOX_PX_W, BOX_PX_H);
    draw_rect((int)box_px, (int)box_py, (int)(box_px + BOX_PX_W), (int)(box_py + BOX_PX_H), COL_BORDER);

    prow = 0;

    // Render Dialog Content
    panic_fill_row(COL_HEADER);                               
    panic_center_row("KERNEL SECURITY EXCEPTION", COLOR_WHITE); 
    panic_fill_row(COL_BORDER);
    panic_fill_row(COL_BORDER);                            
    panic_text_row("", COLOR_WHITE);                          

    panic_kv_row("Reason:", message);
    panic_kv_row("Error Code:", error_code);
    panic_kv_row("RIP:", rip);
    panic_kv_row("RSP:", rsp);
    panic_kv_row("Fault Addr:", fault_address);
    panic_kv_row("Process ID:", pid);
    panic_kv_row("CPU Model:", cpu_name[0] ? cpu_name : "Unknown x86_64 CPU");

    panic_text_row("", COLOR_WHITE);                          
    panic_center_row("System halted safely. Please reboot your computer.", COL_TEXT_DIM);
    panic_fill_row(COL_BORDER);                               

    render_frame();

    disable_interrupts();

    while (true)
    {
        halt_cpu();
    }
}