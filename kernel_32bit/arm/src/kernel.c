/* NasuaOS simple kernel: UART debug + graphical text console.
 *
 * The display is a simple-framebuffer taken from the device tree
 * (QEMU -device ramfb / firmware-provided DTB). If no framebuffer is
 * found the shell still works over the UART.
 *
 * Shared by the arm/risc-v kernels (32 and 64 bit).
 */

typedef unsigned long uptr;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned long long ull;

struct limine_framebuffer {
    void *address;
    ull width;
    ull height;
    ull pitch;
    ushort bpp;
    uchar memory_model;
};

struct limine_framebuffer_response {
    ull revision;
    ull framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    ull id[4];
    ull revision;
    struct limine_framebuffer_response *response;
};

__attribute__((used, section(".limine_requests_start")))
static volatile ull limine_requests_start[] = {
    0xf6b8f4b39de7d1aeULL, 0xfab91a6940fcb9cfULL,
    0x785c6ed015d3e316ULL, 0x181e920a7852b9d9ULL
};
__attribute__((used, section(".limine_requests")))
static volatile ull limine_base_revision[] = {
    0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, 6
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request limine_framebuffer_request = {
    { 0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL,
      0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL },
    0,
    0
};
__attribute__((used, section(".limine_requests_end")))
static volatile ull limine_requests_end[] = {
    0xadc0e0531bb10d03ULL, 0x9572709f31764c62ULL
};

#ifndef UART_BASE
#error "UART_BASE must be defined by the build"
#endif
#ifndef ARCH_NAME
#error "ARCH_NAME must be defined by the build"
#endif
#ifndef BITS
#error "BITS must be defined by the build"
#endif

#include "font.h"

/* QEMU ramfb via fw_cfg (etc/ramfb). The guest allocates the framebuffer
 * itself and DMA-writes a RAMFBCfg to fw_cfg; QEMU then scans it out.
 * Enable with -DUSE_FW_CFG_RAMFB and set FW_CFG_BASE/FB_PHYS per machine. */
#ifdef USE_FW_CFG_RAMFB
#ifndef FW_CFG_BASE
#define FW_CFG_BASE 0x09020000UL
#endif
#define FW_CFG_DMA_REG (FW_CFG_BASE + 0x10)
#define FW_CFG_DATA_REG (FW_CFG_BASE + 0x00)
#define FW_CFG_SEL_REG (FW_CFG_BASE + 0x08)
#define FW_CFG_FILE_DIR 0x0019
#ifndef FB_PHYS
#define FB_PHYS 0x48000000UL
#endif
#define FB_WIDTH 1024
#define FB_HEIGHT 768
#endif

/* ------------------------------------------------------------------ */
/* UART (debug / fallback console)                                     */
/* ------------------------------------------------------------------ */

#if defined(__aarch64__) || defined(__arm__)
#define UART_PL011 1
#else
#define UART_PL011 0
#endif

#if UART_PL011
static volatile uint *uart = (volatile uint *)UART_BASE;
#else
static volatile unsigned char *uart = (volatile unsigned char *)UART_BASE;
#endif

static void uart_init(void)
{
#if UART_PL011
    uart[12] = 0x301; /* UARTCR: UARTEN | TXE | RXE */
#endif
}

static void uart_putc(char c)
{
#if UART_PL011
    while (uart[6] & 0x20) { }
    uart[0] = (unsigned char)c;
#else
    while (!(uart[5] & 0x20)) { }
    uart[0] = (unsigned char)c;
#endif
}

static int uart_getc(void)
{
#if UART_PL011
    while (uart[6] & 0x10) { }
    return uart[0] & 0xff;
#else
    while (!(uart[5] & 0x01)) { }
    return uart[0];
#endif
}

static void uart_puts_dbg(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_puts_dbg_num(uint v)
{
    char b[11];
    int i = 10;
    b[i] = '\0';
    if (!v) {
        uart_putc('0');
        return;
    }
    while (v && i > 0) {
        b[--i] = (char)('0' + (v % 10));
        v /= 10;
    }
    uart_puts_dbg(&b[i]);
}

/* ------------------------------------------------------------------ */
/* Small libc replacements                                             */
/* ------------------------------------------------------------------ */

void *memset(void *d, int c, uptr n)
{
    uchar *p = (uchar *)d;
    while (n--)
        *p++ = (uchar)c;
    return d;
}

void *memcpy(void *d, const void *s, uptr n)
{
    uchar *dd = (uchar *)d;
    const uchar *ss = (const uchar *)s;
    while (n--)
        *dd++ = *ss++;
    return d;
}

void *memmove(void *d, const void *s, uptr n)
{
    uchar *dd = (uchar *)d;
    const uchar *ss = (const uchar *)s;
    if (dd < ss)
        while (n--)
            *dd++ = *ss++;
    else {
        dd += n;
        ss += n;
        while (n--)
            *--dd = *--ss;
    }
    return d;
}

static uptr xstrlen(const char *s)
{
    uptr n = 0;
    while (s[n])
        n++;
    return n;
}

static int xstreq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int xstrcontains(const char *s, const char *sub)
{
    uptr n = xstrlen(sub);
    while (*s) {
        uptr i = 0;
        while (i < n && s[i] == sub[i])
            i++;
        if (i == n)
            return 1;
        s++;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Device tree (FDT) parsing for the simple-framebuffer                */
/* ------------------------------------------------------------------ */

#define FDT_MAGIC 0xd00dfeed
#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

static uint fdt_rd32(const uchar *p)
{
    return ((uint)p[0] << 24) | ((uint)p[1] << 16) | ((uint)p[2] << 8) |
           (uint)p[3];
}

/* Sanity-check an FDT header so random 0xd00dfeed words in RAM are not
 * mistaken for a device tree. */
static int fdt_header_ok(const uchar *base)
{
    if (fdt_rd32(base) != FDT_MAGIC)
        return 0;
    uint total = fdt_rd32(base + 4);
    uint off_struct = fdt_rd32(base + 8);
    uint off_strings = fdt_rd32(base + 12);
    uint size_strings = fdt_rd32(base + 32);
    uint size_struct = fdt_rd32(base + 36);
    if (total < 40 || total >= 0x1000000)
        return 0;
    if (off_struct >= total || off_strings >= total)
        return 0;
    if (!size_strings || !size_struct)
        return 0;
    if (off_strings + size_strings > total ||
        off_struct + size_struct > total)
        return 0;
    return 1;
}

static int fdt_find_framebuffer(uptr dtb, uptr *fb_addr, ull *fb_size,
                                uint *fb_w, uint *fb_h, uint *fb_stride,
                                uint *fb_format)
{
    const uchar *base = (const uchar *)dtb;
    if (!fdt_header_ok(base))
        return 0;

    uint off_struct = fdt_rd32(base + 8);
    uint off_strings = fdt_rd32(base + 12);
    const uchar *struct_base = base + off_struct;
    const uchar *strings = base + off_strings;
    const uchar *pos = struct_base;

    int depth = 0;
    int fb_depth = -1;
    int found = 0;

    for (;;) {
        uint tok = fdt_rd32(pos);
        pos += 4;
        if (tok == FDT_END)
            break;
        if (tok == FDT_NOP)
            continue;

        if (tok == FDT_BEGIN_NODE) {
            const char *name = (const char *)pos;
            uptr nl = xstrlen(name) + 1;
            pos += (nl + 3) & ~(uptr)3;
            depth++;
            if (fb_depth < 0 && xstrcontains(name, "framebuffer"))
                fb_depth = depth;
            continue;
        }

        if (tok == FDT_END_NODE) {
            if (depth == fb_depth)
                fb_depth = -1;
            depth--;
            continue;
        }

        if (tok == FDT_PROP) {
            uint len = fdt_rd32(pos);
            uint nameoff = fdt_rd32(pos + 4);
            const uchar *data = pos + 8;
            const char *pname = (const char *)(strings + nameoff);
            pos += 8 + ((len + 3) & ~3u);

            if (fb_depth > 0 && !found) {
                if (xstreq(pname, "reg") && len >= 8) {
                    uint c0 = fdt_rd32(data);
                    uint c1 = fdt_rd32(data + 4);
                    if (len >= 16) {
                        uint c2 = fdt_rd32(data + 8);
                        uint c3 = fdt_rd32(data + 12);
                        *fb_addr = (uptr)(((ull)c0 << 32) | c1);
                        *fb_size = ((ull)c2 << 32) | c3;
                    } else {
                        *fb_addr = c0;
                        *fb_size = c1;
                    }
                } else if (xstreq(pname, "width")) {
                    *fb_w = fdt_rd32(data);
                } else if (xstreq(pname, "height")) {
                    *fb_h = fdt_rd32(data);
                } else if (xstreq(pname, "stride")) {
                    *fb_stride = fdt_rd32(data);
                } else if (xstreq(pname, "format")) {
                    *fb_format = fdt_rd32(data);
                }
            }
            continue;
        }
    }

    found = (*fb_addr != 0 && *fb_w != 0 && *fb_h != 0);
    return found;
}

/* ------------------------------------------------------------------ */
/* Framebuffer text console                                           */
/* ------------------------------------------------------------------ */

static volatile uint *fb = 0;      /* 32-bit pixels, XRGB8888 */
static uint fb_w, fb_h, fb_stride; /* stride in pixels */
static int fb_cols, fb_rows;
static int cx, cy;
static int have_fb;

#define FG 0x00ffffffu
#define BG 0x00000000u
#define CURSOR_COLOR 0x00aaaaaau

static void fb_init(uptr addr, uint w, uint h, uint stride_bytes, uint format)
{
    if (format != 1 || addr == 0 || w == 0 || h == 0)
        return; /* only XRGB8888 is supported */
    fb = (volatile uint *)addr;
    fb_w = w;
    fb_h = h;
    fb_stride = stride_bytes / 4;
    fb_cols = (int)(w / FONT_W);
    fb_rows = (int)(h / FONT_H);
    have_fb = fb_cols > 0 && fb_rows > 0;
}

static int limine_fb_init(void)
{
    struct limine_framebuffer_response *response =
        limine_framebuffer_request.response;
    if (!response || !response->framebuffer_count || !response->framebuffers)
        return 0;

    struct limine_framebuffer *limine_fb = response->framebuffers[0];
    if (!limine_fb || limine_fb->bpp != 32 || limine_fb->memory_model != 1)
        return 0;

    fb_init((uptr)limine_fb->address, (uint)limine_fb->width,
            (uint)limine_fb->height, (uint)limine_fb->pitch, 1);
    return have_fb;
}

static void fb_pixel(int x, int y, uint color)
{
    if (x < 0 || y < 0 || (uint)x >= fb_w || (uint)y >= fb_h)
        return;
    fb[(uint)y * fb_stride + (uint)x] = color;
}

static void fb_fill_cell(int col, int row, uint color)
{
    int x0 = col * FONT_W;
    int y0 = row * FONT_H;
    for (int y = 0; y < FONT_H; y++)
        for (int x = 0; x < FONT_W; x++)
            fb_pixel(x0 + x, y0 + y, color);
}

static void fb_draw_char(int col, int row, char c, uint fg, uint bg)
{
    fb_fill_cell(col, row, bg);
    if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT)
        c = '?';
    const ushort *glyph = font_data[c - FONT_FIRST];
    int x0 = col * FONT_W;
    int y0 = row * FONT_H;
    for (int y = 0; y < FONT_H; y++) {
        ushort bits = glyph[y];
        for (int x = 0; x < 7; x++)
            if (bits & (1u << (6 - x)))
                fb_pixel(x0 + x, y0 + y, fg);
    }
}

static void fb_clear(void)
{
    for (uint y = 0; y < fb_h; y++)
        for (uint x = 0; x < fb_w; x++)
            fb[y * fb_stride + x] = BG;
    cx = cy = 0;
}

static void fb_scroll(void)
{
    uptr bytes = (uptr)(fb_h - FONT_H) * fb_stride * 4;
    memmove((void *)fb, (void *)(fb + (uptr)FONT_H * fb_stride), bytes);
    for (uint y = fb_h - FONT_H; y < fb_h; y++)
        for (uint x = 0; x < fb_w; x++)
            fb[y * fb_stride + x] = BG;
    cy = fb_rows - 1;
}

static void fb_putc(char c)
{
    if (!have_fb) {
        uart_putc(c);
        return;
    }
    uart_putc(c);
    if (c == '\n') {
        cx = 0;
        if (++cy >= fb_rows)
            fb_scroll();
        return;
    }
    if (c == '\r') {
        cx = 0;
        return;
    }
    if (c == '\b') {
        if (cx > 0)
            cx--;
        fb_draw_char(cx, cy, ' ', FG, BG);
        return;
    }
    if (c < 0x20)
        return;
    fb_draw_char(cx, cy, c, FG, BG);
    if (++cx >= fb_cols) {
        cx = 0;
        if (++cy >= fb_rows)
            fb_scroll();
    }
}

static void fb_print(const char *s)
{
    while (*s)
        fb_putc(*s++);
}

static int fb_getc(void)
{
    return uart_getc();
}

static int fb_getline(char *buf, int max)
{
    int n = 0;
    for (;;) {
        int c = fb_getc();
        if (c == '\r' || c == '\n') {
            fb_print("\r\n");
            buf[n] = '\0';
            return n;
        }
        if (c == '\b' || c == 0x7f) {
            if (n > 0) {
                n--;
                fb_putc('\b');
            }
            continue;
        }
        if (c >= 0x20 && c < 0x7f && n < max - 1) {
            buf[n++] = (char)c;
            fb_putc((char)c);
        }
    }
}

#ifndef SCAN_BASE
#define SCAN_BASE 0x40000000UL
#endif
#ifndef SCAN_SIZE
#define SCAN_SIZE 0x4000000UL
#endif

/* QEMU -kernel does not always pass the DTB in a register; scan low RAM
 * for the FDT magic as a fallback (works for qemu -kernel and is harmless
 * when a real bootloader already provided the pointer). */
static uptr fdt_scan_memory(void)
{
    const uchar *p = (const uchar *)SCAN_BASE;
    uptr n = SCAN_SIZE / 4;
    for (uptr i = 0; i < n; i++) {
        if (fdt_rd32(p + i * 4) == FDT_MAGIC) {
            uptr cand = SCAN_BASE + i * 4;
            if (fdt_header_ok((const uchar *)cand))
                return cand;
        }
    }
    return 0;
}

#ifdef USE_FW_CFG_RAMFB

static uint bswap32(uint v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
           ((v >> 8) & 0xff00) | ((v >> 24) & 0xff);
}

static ull bswap64(ull v)
{
    return ((ull)bswap32((uint)v) << 32) | bswap32((uint)(v >> 32));
}

static ushort bswap16(ushort v)
{
    return (ushort)((v >> 8) | (v << 8));
}

struct fw_dma_access {
    uint control;
    uint length;
    ull address;
};

/* All fields are big-endian on the wire. */
static int fw_cfg_dma(uint control, uint len, void *buf)
{
    struct fw_dma_access d;
    d.control = bswap32(control);
    d.length = bswap32(len);
    d.address = bswap64((ull)buf);
    {
        /* The DMA address register is a 64-bit big-endian value. It is
         * written as two 32-bit words, most-significant half first
         * (offset 0); writing the least-significant half (offset 4)
         * triggers the transfer. bswap64() makes the low word the
         * big-endian high half and vice versa. */
        volatile uint *r = (volatile uint *)FW_CFG_DMA_REG;
        ull a = bswap64((ull)&d);
        r[0] = (uint)a;
        r[1] = (uint)(a >> 32);
    }
    volatile struct fw_dma_access *dp = &d;
    for (;;) {
        uint c = bswap32(dp->control);
        if (!(c & ~1u))
            return (c & 1) ? -1 : 0;
    }
}

struct fw_cfg_file_ent {
    uint size;
    ushort select;
    ushort reserved;
    char name[56];
};

/* The file directory item is a big-endian count followed by entries. */
struct fw_cfg_file_dir {
    uint count;
    struct fw_cfg_file_ent f[64];
};

static struct fw_cfg_file_dir fw_dir;

/* Locate the selector of the fw_cfg file "etc/ramfb"; returns selector
 * or 0xffff on failure. */
static uint fw_cfg_find_ramfb(void)
{
    /* The first fw_cfg DMA access after boot can come back empty; retry. */
    uint count = 0;
    for (int t = 0; t < 8; t++) {
        memset(&fw_dir, 0, sizeof(fw_dir));
        /* One shot: count + up to 64 entries; the tail is zero-filled. */
        if (fw_cfg_dma((FW_CFG_FILE_DIR << 16) | 0x08 | 0x02,
                       (uint)sizeof(fw_dir), &fw_dir))
            continue;
        count = bswap32(fw_dir.count);
        if (count && count <= 64)
            break;
        count = 0;
    }
    if (!count)
        return 0xffff;

    for (uint i = 0; i < count; i++) {
        if (xstreq(fw_dir.f[i].name, "etc/ramfb"))
            return bswap16(fw_dir.f[i].select);
    }
    return 0xffff;
}

/* Allocate FB_WIDTH x FB_HEIGHT XRGB8888 at FB_PHYS and hand it to QEMU. */
static int fw_cfg_setup_ramfb(void)
{
    uint sel = fw_cfg_find_ramfb();
    if (sel == 0xffff)
        return 0;

    /* clear the framebuffer */
    volatile uint *p = (volatile uint *)FB_PHYS;
    for (uint i = 0; i < (uint)FB_WIDTH * FB_HEIGHT; i++)
        p[i] = 0;

    {
        /* Must match QEMU's packed RAMFBCfg exactly (28 bytes). */
        struct __attribute__((packed)) {
            ull addr;
            uint fourcc;
            uint flags;
            uint width;
            uint height;
            uint stride;
        } cfg;
        cfg.addr = bswap64((ull)FB_PHYS);
        cfg.fourcc = bswap32(0x34325258u); /* DRM_FORMAT_XRGB8888 'XR24' */
        cfg.flags = 0;
        cfg.width = bswap32(FB_WIDTH);
        cfg.height = bswap32(FB_HEIGHT);
        cfg.stride = bswap32(FB_WIDTH * 4);
        int wrc = fw_cfg_dma((sel << 16) | 0x08 | 0x10, (uint)sizeof(cfg), &cfg);
#ifdef FW_CFG_DEBUG
        {
            char rb[32];
            memset(rb, 0, sizeof(rb));
            int rrc = fw_cfg_dma((sel << 16) | 0x08 | 0x02, (uint)sizeof(rb), rb);
            uart_puts_dbg("wrc="); uart_puts_dbg_num(wrc);
            uart_puts_dbg(" rrc="); uart_puts_dbg_num(rrc);
            uart_puts_dbg(" rb=");
            for (uint i = 0; i < 8; i++) {
                uint v = ((uint)(uchar)rb[i*4]<<24)|((uint)(uchar)rb[i*4+1]<<16)|((uint)(uchar)rb[i*4+2]<<8)|(uchar)rb[i*4+3];
                uart_puts_dbg_num(v); uart_putc(' ');
            }
            uart_putc('\n');
        }
#endif
        fb_init(FB_PHYS, FB_WIDTH, FB_HEIGHT, FB_WIDTH * 4, 1);
        return have_fb;
    }
}
#endif /* USE_FW_CFG_RAMFB */

/* ------------------------------------------------------------------ */
/* Shell                                                                */
/* ------------------------------------------------------------------ */

static void help(void)
{
    fb_print("commands:\r\n");
    fb_print("  help   - show this message\r\n");
    fb_print("  uname  - print system name and architecture\r\n");
    fb_print("  echo   - echo the rest of the line\r\n");
    fb_print("  about  - print build information\r\n");
}

void kmain(uptr dtb)
{
    static char line[128];
    uptr fb_addr = 0;
    ull fb_size = 0;
    uint fb_w = 0, fb_h = 0, fb_stride = 0, fb_format = 0;

    uart_init();

    {
        static const char banner[] = "NasuaOS kernel boot\r\n";
        for (const char *p = banner; *p; p++)
            uart_putc(*p);
    }

    int have_limine_fb = limine_fb_init();
    int have_dtb = !have_limine_fb && dtb &&
        fdt_find_framebuffer(dtb, &fb_addr, &fb_size, &fb_w, &fb_h,
                             &fb_stride, &fb_format);
    if (!have_dtb) {
        uptr scanned = fdt_scan_memory();
        if (scanned)
            have_dtb = fdt_find_framebuffer(scanned, &fb_addr, &fb_size,
                                            &fb_w, &fb_h, &fb_stride,
                                            &fb_format);
    }

    if (!fb) {
#ifdef USE_FW_CFG_RAMFB
        fw_cfg_setup_ramfb();
#endif
    }

    if (!fb && have_dtb)
        fb_init(fb_addr, fb_w, fb_h, fb_stride, fb_format);

    {
        static const char msg[] = "fb: ";
        for (const char *p = msg; *p; p++)
            uart_putc(*p);
        const char *st = fb ? "found\r\n" : "not found\r\n";
        for (const char *p = st; *p; p++)
            uart_putc(*p);
    }

    if (have_fb)
        fb_clear();

    fb_print("\r\n========================================\r\n");
    fb_print(" NasuaOS ");
    fb_print(ARCH_NAME);
    fb_print(" ");
    fb_print(BITS == 64 ? "64" : "32");
    fb_print("-bit -- simple shell (");
    fb_print(ARCH_NAME);
    fb_print(")\r\n");
    fb_print("========================================\r\n");
    fb_print("type 'help' for commands\r\n\r\n");

    for (;;) {
        fb_print("nasua> ");
        fb_getline(line, (int)sizeof(line));

        if (!line[0])
            continue;

        if (xstreq(line, "help")) {
            help();
        } else if (xstreq(line, "uname")) {
            fb_print("NasuaOS ");
            fb_print(ARCH_NAME);
            fb_print(" ");
            fb_print(BITS == 64 ? "64" : "32");
            fb_print("-bit\r\n");
        } else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' &&
                   line[3] == 'o' && line[4] == ' ') {
            fb_print(&line[5]);
            fb_print("\r\n");
        } else if (xstreq(line, "about")) {
            fb_print("NasuaOS experimental kernel\r\n");
            fb_print("arch: ");
            fb_print(ARCH_NAME);
            fb_print(", bits: ");
            fb_print(BITS == 64 ? "64\r\n" : "32\r\n");
            fb_print("display: ");
            fb_print(have_fb ? "framebuffer\r\n" : "uart only\r\n");
        } else {
            fb_print("unknown command: ");
            fb_print(line);
            fb_print("\r\n");
        }
        fb_print("\r\n");
    }
}
