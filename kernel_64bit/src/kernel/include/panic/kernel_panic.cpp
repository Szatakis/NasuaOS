#include "kernel_panic.hpp"

#include "system/drivers/video/driver.hpp"
#include "system/drivers/cpu/driver.hpp"
#include "system/drivers/timer/driver.hpp"
#include "system/gui/vars/colors.hpp"

#include "libs/libc/libc.h"
#include "libs/asm/asm.h"

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────

#define CH_W        9          // char width  (8px glyph + 1px spacing)
#define CH_H        13         // char height (8px glyph + 5px leading)

#define BOX_COLS    55         // characters per line inside the box
#define BOX_ROWS    14         // number of text rows in the box

// Derived pixel dimensions
#define BOX_PX_W    (BOX_COLS * CH_W + 4)    // +4 for left/right border px
#define BOX_PX_H    (BOX_ROWS * CH_H + 4)    // +4 for top/bottom border px

// ─────────────────────────────────────────────────────────────────────────────
// Helpers – all drawing goes directly to the backbuffer via print_at8 /
// fill_block / draw_rect so that a single render_frame() call shows everything.
// ─────────────────────────────────────────────────────────────────────────────

// Top-left corner of the dialog box in pixels
static size_t box_px;   // x
static size_t box_py;   // y

// Current text-row inside the box (0-based)
static size_t prow;

// pixel Y for the current text row
static inline size_t row_y()
{
    return box_py + 2 + prow * CH_H;
}

// Draw a full-width horizontal bar of a solid colour (used for borders/header)
static void panic_fill_row(uint32_t color)
{
    fill_block(box_px, row_y(), color, BOX_PX_W, CH_H);
    prow++;
}

// Draw one text row: left border | <text padded to BOX_COLS> | right border
static void panic_text_row(const char* text, uint32_t text_color)
{
    size_t y = row_y();
    size_t x = box_px;

    // Left border stripe
    fill_block(x, y, 0xFF2D3748, 4, CH_H);
    x += 4;

    // Text content (up to BOX_COLS characters, space-padded)
    size_t col   = 0;
    size_t limit = BOX_COLS;

    while (*text && col < limit)
    {
        draw_char8((unsigned char)*text, x, y + 2, text_color);
        x   += CH_W;
        col++;
        text++;
    }

    // Space-pad the rest
    while (col < limit)
    {
        x += CH_W;
        col++;
    }

    // Right border stripe
    fill_block(x, y, 0xFF2D3748, 4, CH_H);

    prow++;
}

// Centered text row
static void panic_center_row(const char* text, uint32_t text_color)
{
    size_t len    = strlen(text);
    size_t pad    = (len < (size_t)BOX_COLS) ? (BOX_COLS - len) / 2 : 0;
    size_t y      = row_y();
    size_t x      = box_px + 4 + pad * CH_W;

    // Background for the whole row
    fill_block(box_px,      y, 0xFF1C242F, 4,        CH_H);   // left border
    fill_block(box_px + 4,  y, 0xFF1C242F, BOX_COLS * CH_W, CH_H);
    fill_block(box_px + 4 + BOX_COLS * CH_W, y, 0xFF1C242F, 4, CH_H); // right border

    // Draw text
    while (*text)
    {
        draw_char8((unsigned char)*text, x, y + 2, text_color);
        x += CH_W;
        text++;
    }

    prow++;
}

// Key: value row
static void panic_kv_row(const char* key, const char* value)
{
    char buf[BOX_COLS + 1];

    // Build "Key: value" string, truncated to BOX_COLS
    size_t ki = 0, bi = 0;

    while (key[ki] && bi < (size_t)BOX_COLS)
        buf[bi++] = key[ki++];

    size_t vi = 0;
    while (value[vi] && bi < (size_t)BOX_COLS)
        buf[bi++] = value[vi++];

    buf[bi] = '\0';

    panic_text_row(buf, COLOR_WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
// kernel_panic — the actual function
// ─────────────────────────────────────────────────────────────────────────────

void kernel_panic(const char* message,
                  const char* error_code,
                  const char* rip,
                  const char* rsp,
                  const char* fault_address,
                  const char* pid)
{
    // Gather CPU info before touching the screen
    char cpu_name[49];
    cpu_get_brand(cpu_name);

    // ── Set kernel_panicked so the main-loop stops redrawing ──────────────────
    kernel_panicked = true;

    // ── Clear backbuffer to dark background ───────────────────────────────────
    clear_screen();

    // ── Compute box position (centred) ────────────────────────────────────────
    size_t scr_w = fb->width;
    size_t scr_h = fb->height;

    box_px = (scr_w  > (size_t)BOX_PX_W) ? (scr_w  - BOX_PX_W) / 2 : 0;
    box_py = (scr_h  > (size_t)BOX_PX_H) ? (scr_h  - BOX_PX_H) / 2 : 0;
    prow   = 0;

    // ── Draw outer box background ─────────────────────────────────────────────
    fill_block(box_px, box_py, 0xFF1C242F, BOX_PX_W, BOX_PX_H);

    // ── Draw outer border (1-px rect) ─────────────────────────────────────────
    draw_rect(
        (int)box_px,
        (int)box_py,
        (int)(box_px + BOX_PX_W),
        (int)(box_py + BOX_PX_H),
        0xFF2D3748
    );

    // Skip 1 px top border
    prow = 0;

    // ── Row 0: red header bar ─────────────────────────────────────────────────
    panic_fill_row(0xFFBF616A);          // warm red

    // ── Row 1: "KERNEL PANIC" title ───────────────────────────────────────────
    panic_center_row("KERNEL PANIC", COLOR_WHITE);

    // ── Row 2: divider ────────────────────────────────────────────────────────
    panic_fill_row(0xFF2D3748);

    // ── Row 3: blank ─────────────────────────────────────────────────────────
    panic_text_row("", COLOR_WHITE);

    // ── Rows 4-10: key-value info ─────────────────────────────────────────────
    panic_kv_row("Reason:        ", message);
    panic_kv_row("Error code:    ", error_code);
    panic_kv_row("RIP:           ", rip);
    panic_kv_row("RSP:           ", rsp);
    panic_kv_row("Fault Address: ", fault_address);
    panic_kv_row("CPU:           ", cpu_name);
    panic_kv_row("PID:           ", pid);

    // ── Row 11: blank ─────────────────────────────────────────────────────────
    panic_text_row("", COLOR_WHITE);

    // ── Row 12: bottom hint ───────────────────────────────────────────────────
    panic_center_row("System halted. Please restart.", 0xFFAAAAAA);

    // ── Row 13: bottom border bar ─────────────────────────────────────────────
    panic_fill_row(0xFF2D3748);

    // ── Flush backbuffer to screen ────────────────────────────────────────────
    render_frame();

    // ── Halt ─────────────────────────────────────────────────────────────────
    disable_interrupts();

    while (true)
    {
        halt_cpu();
    }
}