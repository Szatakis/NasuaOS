#include "kernel_panic.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/cpu/driver.hpp"
#include "drivers/timer/driver.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"
#include "libs/qr_code/qr_code.hpp"

// Layout constants
#define CH_W 9
#define CH_H 13

#define BOX_COLS 58
#define BOX_ROWS 16

// Derived pixel dimensions
#define HORIZ_PAD 8
#define BOX_PX_W (BOX_COLS * CH_W + (HORIZ_PAD * 2))
#define BOX_PX_H (BOX_ROWS * CH_H + 4)

// Right-side debug box
#define DBG_BOX_COLS 32
#define DBG_BOX_PX_W (DBG_BOX_COLS * CH_W + (HORIZ_PAD * 2))
#define DBG_BOX_PX_H BOX_PX_H

#define BOX_GAP 16

// Color Palette
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

uint32_t debug_qr_code[128 * 128];

static inline size_t row_y()
{
    return box_py + 2 + prow * CH_H;
}

static void draw_string_clipped(size_t x, size_t y, const char* str, uint32_t color, size_t max_chars)
{
    size_t drawn = 0;
    while (*str && drawn < max_chars)
    {
        Gpu::draw_char8((unsigned char)*str, x, y, color);
        x += CH_W;
        str++;
        drawn++;
    }
}

static void panic_fill_row(uint32_t color)
{
    Gpu::fill_block(box_px, row_y(), color, BOX_PX_W, CH_H);
    prow++;
}

static void panic_text_row(const char* text, uint32_t text_color)
{
    size_t y = row_y();
    size_t x = box_px;

    Gpu::fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD;

    Gpu::fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    draw_string_clipped(x, y + 2, text ? text : "", text_color, BOX_COLS);

    x += BOX_COLS * CH_W;
    Gpu::fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

static void panic_center_row(const char* text, uint32_t text_color)
{
    size_t len = text ? strlen(text) : 0;
    size_t pad_chars = (len < (size_t)BOX_COLS) ? (BOX_COLS - len) / 2 : 0;
    
    size_t y = row_y();
    size_t x = box_px;

    Gpu::fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD;

    Gpu::fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    draw_string_clipped(x + (pad_chars * CH_W), y + 2, text ? text : "", text_color, BOX_COLS - pad_chars);

    Gpu::fill_block(box_px + BOX_PX_W - HORIZ_PAD, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

static void panic_kv_row(const char* key, const char* value)
{
    size_t y = row_y();
    size_t x = box_px;

    Gpu::fill_block(x, y, COL_BORDER, HORIZ_PAD, CH_H);
    x += HORIZ_PAD;

    Gpu::fill_block(x, y, COL_PANEL, BOX_COLS * CH_W, CH_H);

    draw_string_clipped(x + CH_W, y + 2, key ? key : "", COL_TEXT_DIM, 13);

    size_t val_offset = 14 * CH_W;
    draw_string_clipped(x + val_offset, y + 2, (value && *value) ? value : "N/A", COL_TEXT_BRIGHT, BOX_COLS - 14);

    Gpu::fill_block(box_px + BOX_PX_W - HORIZ_PAD, y, COL_BORDER, HORIZ_PAD, CH_H);

    prow++;
}

static void draw_bitmap_128x128(size_t x, size_t y)
{
    generate_qr_code(128, "https://github.com/Szatakis/NasuaOS/blob/main/documentation/debug_instructions/debug_instructions.md#kernel_panic", debug_qr_code);

    for (size_t j = 0; j < 128; ++j)
    {
        for (size_t i = 0; i < 128; ++i)
        {
            uint32_t col = debug_qr_code[j * 128 + i];
            Gpu::put_pixel(x + i, y + j, col);
        }
    }
}

static void draw_debug_box(size_t scr_w, size_t scr_h)
{
    (void)scr_h;

    size_t total_w = BOX_PX_W + BOX_GAP + DBG_BOX_PX_W;
    if (scr_w < total_w)
    {
        return;
    }

    size_t dbg_px = box_px + BOX_PX_W + BOX_GAP;
    size_t dbg_py = box_py;

    Gpu::fill_block(dbg_px, dbg_py, COL_PANEL, DBG_BOX_PX_W, DBG_BOX_PX_H);
    Gpu::draw_rect((int)dbg_px, (int)dbg_py, (int)(dbg_px + DBG_BOX_PX_W), (int)(dbg_py + DBG_BOX_PX_H), COL_BORDER);

    size_t header_y = dbg_py + 2;
    Gpu::fill_block(dbg_px, header_y, COL_HEADER, DBG_BOX_PX_W, CH_H);

    size_t text_block_y = header_y + CH_H + 4;
    size_t text_block_h = CH_H * 3;

    Gpu::fill_block(dbg_px + HORIZ_PAD, text_block_y, COL_BORDER, DBG_BOX_COLS * CH_W, text_block_h);

    draw_string_clipped(dbg_px + HORIZ_PAD + CH_W, text_block_y + 2, "Scan this code for", COL_TEXT_DIM, DBG_BOX_COLS - 2);
    draw_string_clipped(dbg_px + HORIZ_PAD + CH_W, text_block_y + 2 + CH_H, "kernel debug information.", COL_TEXT_DIM, DBG_BOX_COLS - 2);

    size_t bmp_x = dbg_px + (DBG_BOX_PX_W - 128) / 2;
    size_t bmp_y = dbg_py + (DBG_BOX_PX_H - 128) / 2 + 20;

    draw_bitmap_128x128(bmp_x, bmp_y);
}


// Kernel Panic
void kernel_panic(const char* message, const char* error_code, const char* rip, const char* rsp, const char* fault_address, const char* pid)
{
    kernel_panicked = true;

    char cpu_name[49];
    Memory::memset(cpu_name, 0, sizeof(cpu_name));
    Cpu::get_brand(cpu_name);

    Gpu::clear_screen(COLOR_NASUA_BG);

    size_t scr_w = Gpu::fb ? Gpu::fb->width : 800;
    size_t scr_h = Gpu::fb ? Gpu::fb->height : 600;

    box_px = (scr_w > BOX_PX_W) ? (scr_w - BOX_PX_W) / 2 : 0;
    box_py = (scr_h > BOX_PX_H) ? (scr_h - BOX_PX_H) / 2 : 0;
    prow = 0;

    Gpu::fill_block(box_px, box_py, COL_PANEL, BOX_PX_W, BOX_PX_H);
    Gpu::draw_rect((int)box_px, (int)box_py, (int)(box_px + BOX_PX_W), (int)(box_py + BOX_PX_H), COL_BORDER);

    prow = 0;

    panic_fill_row(COL_HEADER);                               
    panic_center_row("KERNEL PANIC", COL_TEXT_BRIGHT); 
    panic_fill_row(COL_BORDER);
    panic_fill_row(COL_BORDER);                            
    panic_text_row("", COL_TEXT_BRIGHT);                          

    panic_kv_row("Reason:",       message);
    panic_kv_row("Error Code:",   error_code);
    panic_kv_row("RIP:",          rip);
    panic_kv_row("RSP:",          rsp);
    panic_kv_row("Fault Addr:",   fault_address);
    panic_kv_row("Process ID:",   pid);
    panic_kv_row("CPU Model:",    cpu_name[0] ? cpu_name : "Unknown CPU");

    panic_text_row("", COL_TEXT_BRIGHT);                          
    panic_center_row("System halted safely. Please reboot your computer.", COL_TEXT_DIM);
    panic_fill_row(COL_BORDER);                               

    draw_debug_box(scr_w, scr_h);

    Gpu::render_frame();

    disable_interrupts();

    while (true)
    {
        halt_cpu();
    }
}
