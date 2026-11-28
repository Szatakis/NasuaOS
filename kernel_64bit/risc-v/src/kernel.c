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

static int uart_getc_nb(void)
{
#if UART_PL011
    if (uart[6] & 0x10) return -1;
    return uart[0] & 0xff;
#else
    if (!(uart[5] & 0x01)) return -1;
    return uart[0];
#endif
}

/* Keyboard input: virtio-mmio (QEMU -device virtio-keyboard-device)   */
/* ------------------------------------------------------------------ */

#ifndef VIRTIO_MMIO_BASE
#define VIRTIO_MMIO_BASE 0x0a000000UL
#endif
#ifndef VIRTIO_MMIO_STEP
#define VIRTIO_MMIO_STEP 0x200u
#endif
#ifndef VIRTIO_MMIO_MAX
#define VIRTIO_MMIO_MAX  32u
#endif

#define VM_MAGIC      0x000u
#define VM_VERSION    0x004u
#define VM_DEVID      0x008u
#define VM_DEV_FEAT   0x010u
#define VM_DEV_FEAT_S 0x014u
#define VM_DRV_FEAT   0x020u
#define VM_DRV_FEAT_S 0x024u
#define VM_GUEST_PAGE 0x028u
#define VM_QSEL       0x030u
#define VM_QNUM_MAX   0x034u
#define VM_QNUM       0x038u
#define VM_QALIGN     0x03cu
#define VM_QPFN       0x040u
#define VM_QNOTIFY    0x050u
#define VM_STATUS     0x070u

#define VIRTIO_ID_INPUT 18u
#define VIRTIO_F_VERSION_1 (1u << 0)

#define VS_ACK       1u
#define VS_DRIVER    2u
#define VS_DRIVER_OK 4u
#define VS_FEAT_OK   8u

#define VRING_F_WRITE 2u

#define VQ_N 32u

struct vq_desc { ull addr; uint len; ushort flags; ushort next; };
struct vq_avail { ushort flags; ushort idx; ushort ring[VQ_N]; };
struct vq_used_elem { uint id; uint len; };
struct vq_used { ushort flags; ushort idx; struct vq_used_elem ring[VQ_N]; };
struct vin_event { ushort type; ushort code; uint value; };

/* Legacy virtio-mmio: one queue = contiguous page-aligned region holding
 * desc[num] + avail + pad + used (used aligned up to VM_QALIGN=4096). */
#define VQ_AREA_SIZE 8192u
static uchar vq_area[VQ_AREA_SIZE] __attribute__((aligned(4096)));
static struct vq_desc *lg_desc;
static struct vq_avail *lg_avail;
static struct vq_used *lg_used;
static struct vin_event vq_events[VQ_N] __attribute__((aligned(8)));

static uptr kbd_mmio;
static uint kbd_qn;
static ushort kbd_used_last, kbd_used_pos, kbd_avail_pos;
static int kbd_shift, kbd_caps, have_kbd;

static uint vm_rd(uint off) { return *(volatile uint *)(kbd_mmio + off); }
static void vm_wr(uint off, uint v) { *(volatile uint *)(kbd_mmio + off) = v; }

void *memset(void *d, int c, uptr n);

/* Limine executable-address request: lets us translate the virtqueue
 * buffer's virtual address to a physical one when booted via Limine
 * (MMU enabled, higher-half). Without Limine the response stays NULL
 * and we fall back to identity mapping. */
struct limine_exec_addr_response {
    ull revision;
    ull physical_base;
    ull virtual_base;
};
struct limine_exec_addr_request {
    ull id[4];
    ull revision;
    struct limine_exec_addr_response *response;
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_exec_addr_request limine_exec_addr_request = {
    { 0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL,
      0x71ba76863cc55f63ULL, 0xb2644a48c516a487ULL },
    0, 0
};

static int kbd_present(void)
{
    for (uint i = 0; i < VIRTIO_MMIO_MAX; i++) {
        uptr b = VIRTIO_MMIO_BASE + (uptr)i * VIRTIO_MMIO_STEP;
        if (*(volatile uint *)(b + VM_MAGIC) == 0x74726976u &&
            *(volatile uint *)(b + VM_VERSION) == 1u &&
            *(volatile uint *)(b + VM_DEVID) == VIRTIO_ID_INPUT) {
            kbd_mmio = b;
            return 1;
        }
    }
    return 0;
}

static ull kvirt2phys(uptr v)
{
    if (limine_exec_addr_request.response)
        return (ull)v - limine_exec_addr_request.response->virtual_base +
               limine_exec_addr_request.response->physical_base;
    return (ull)v;
}

static void vq_post(uint id)
{
    lg_desc[id].addr = kvirt2phys((uptr)&vq_events[id]);
    lg_desc[id].len = sizeof(struct vin_event);
    lg_desc[id].flags = VRING_F_WRITE;
    lg_desc[id].next = 0;
    __sync_synchronize();
    lg_avail->ring[kbd_avail_pos] = (ushort)id;
    __sync_synchronize();
    lg_avail->idx++;
    if (++kbd_avail_pos >= kbd_qn)
        kbd_avail_pos = 0;
}

static void vq_setup(void)
{
    uint qmax;
    vm_wr(VM_QSEL, 0);
    qmax = vm_rd(VM_QNUM_MAX);
    kbd_qn = qmax && qmax < VQ_N ? qmax : VQ_N;

    memset(vq_area, 0, VQ_AREA_SIZE);
    uptr base = (uptr)vq_area;
    lg_desc = (struct vq_desc *)base;
    lg_avail = (struct vq_avail *)(base + kbd_qn * sizeof(struct vq_desc));
    lg_used = (struct vq_used *)(((base + kbd_qn * sizeof(struct vq_desc)
                    + 4 + 2 * kbd_qn) + 4095u) & ~(uptr)4095u);

    ull phys = (ull)base;
    if (limine_exec_addr_request.response) {
        phys = (ull)base - limine_exec_addr_request.response->virtual_base +
               limine_exec_addr_request.response->physical_base;
    }
    vm_wr(VM_QNUM, kbd_qn);
    vm_wr(VM_QALIGN, 4096u);
    vm_wr(VM_QPFN, (uint)(phys >> 12));
}

static int kbd_init(void)
{
    if (!kbd_present())
        return 0;

    vm_wr(VM_STATUS, 0);
    vm_wr(VM_DRV_FEAT_S, 0);
    vm_wr(VM_DRV_FEAT, 0);
    vm_wr(VM_GUEST_PAGE, 4096u);
    vm_wr(VM_STATUS, VS_ACK | VS_DRIVER);
    vq_setup();
    for (uint i = 0; i < kbd_qn; i++)
        vq_post(i);
    vm_wr(VM_QNOTIFY, 0);
    vm_wr(VM_STATUS, VS_ACK | VS_DRIVER | VS_FEAT_OK | VS_DRIVER_OK);
    kbd_used_last = lg_used->idx;
    have_kbd = 1;
    return 1;
}

/* Linux input event types/codes (subset). */
#define EV_KEY 0x0001u
#define KEY_BACKSPACE 14u
#define KEY_TAB       15u
#define KEY_ENTER     28u
#define KEY_LEFTCTRL  29u
#define KEY_LEFTSHIFT 42u
#define KEY_RIGHTSHIFT 54u
#define KEY_LEFTALT   56u
#define KEY_SPACE     57u
#define KEY_CAPSLOCK  58u
#define KEY_RIGHTCTRL 97u
#define KEY_RIGHTALT  100u
#define KEY_RIGHTMETA 126u
#define KEY_LEFTMETA  125u

static int kbd_map(uint code, int upper)
{
    /* QWERTY rows: KEY_Q..P =16..25, KEY_A..L =30..38, KEY_Z..M =44..50 */
    if (code >= 16 && code <= 25) {
        static const char lo[] = "qwertyuiop";
        static const char hi[] = "QWERTYUIOP";
        return upper ? hi[code - 16] : lo[code - 16];
    }
    if (code >= 30 && code <= 38) {
        static const char lo[] = "asdfghjkl";
        static const char hi[] = "ASDFGHJKL";
        return upper ? hi[code - 30] : lo[code - 30];
    }
    if (code >= 44 && code <= 50) {
        static const char lo[] = "zxcvbnm";
        static const char hi[] = "ZXCVBNM";
        return upper ? hi[code - 44] : lo[code - 44];
    }
    if (code >= 2 && code <= 11) {           /* digits 1..0 */
        static const char d[] = "1234567890";
        static const char ds[] = "!@#$%^&*()";
        return upper ? ds[code - 2] : d[code - 2];
    }
    switch (code) {
    case 12:  return upper ? '_' : '-';
    case 13:  return upper ? '+' : '=';
    case 26:  return upper ? '{' : '[';
    case 27:  return upper ? '}' : ']';
    case 39:  return upper ? ':' : ';';
    case 40:  return upper ? '"' : '\'';
    case 41:  return upper ? '~' : '`';
    case 43:  return upper ? '|' : '\\';
    case 51:  return upper ? '<' : ',';
    case 52:  return upper ? '>' : '.';
    case 53:  return upper ? '?' : '/';
    }
    return -1;
}

static int kbd_translate(ushort code, uint value)
{
    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        kbd_shift = value ? 1 : 0;
        return -1;
    }
    if (code == KEY_CAPSLOCK) {
        if (value == 1) kbd_caps = !kbd_caps;
        return -1;
    }
    if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL ||
        code == KEY_LEFTALT || code == KEY_RIGHTALT ||
        code == KEY_LEFTMETA || code == KEY_RIGHTMETA)
        return -1;
    if (!value)                       /* key release */
        return -1;
    if (code == KEY_ENTER)    return '\r';
    if (code == KEY_BACKSPACE) return '\b';
    if (code == KEY_TAB)      return '\t';
    if (code == KEY_SPACE)    return ' ';
    return kbd_map(code, kbd_shift ^ kbd_caps);
}

static int kbd_getc(void)
{
    if (!have_kbd)
        return -1;
    for (;;) {
        ushort idx = lg_used->idx;
        if (idx == kbd_used_last)
            return -1;
        __sync_synchronize();
        struct vq_used_elem *e = &lg_used->ring[kbd_used_pos];
        struct vin_event ev = vq_events[e->id];
        kbd_used_last++;
        if (++kbd_used_pos >= kbd_qn)
            kbd_used_pos = 0;
        vq_post(e->id);
        __sync_synchronize();
        vm_wr(VM_QNOTIFY, 0);
        if (ev.type == EV_KEY) {
            int c = kbd_translate(ev.code, ev.value);
            if (c >= 0)
                return c;
        }
    }
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
    for (;;) {
        int c = kbd_getc();
        if (c >= 0)
            return c;
        c = uart_getc_nb();
        if (c >= 0)
            return c;
        for (volatile int i = 0; i < 2000; i++) { }
    }
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
    /* volatile: QEMU reads this struct when the DMA register write
     * below triggers the transfer, so every field must be stored before
     * that write; plain stores may be reordered after it. */
    volatile struct fw_dma_access d;
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
        if (wrc)
            return 0;
    }

    fb_init(FB_PHYS, FB_WIDTH, FB_HEIGHT, FB_WIDTH * 4, 1);
    return have_fb;
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
    have_kbd = kbd_init();

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

    fb_print("\x1B\n $$\\   $$\\                                          $$$$$$\\   $$$$$$\\  \n");
    fb_print(" $$$\\  $$ |                                        $$  __$$\\ $$  __$$\\ \n");
    fb_print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    fb_print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
    fb_print(" $$ $$\\$$ | \\____$$\\ $$  _____|$$ |  $$ | \\____$$\\ $$ |  $$ |\\$$$$$$\\  \n");
    fb_print(" $$ \\$$$$ | $$$$$$$ |\\$$$$$$\\  $$ |  $$ | $$$$$$$ |$$ |  $$ | \\____$$\\ \n");
    fb_print(" $$ |\\$$$ |$$  __$$ | \\____$$\\ $$ |  $$ |$$  __$$ |$$ |  $$ |$$\\   $$ |\n");
    fb_print(" $$ | \\$$ |\\$$$$$$$ |$$$$$$$  |\\$$$$$$  |\\$$$$$$$ | $$$$$$  |\\$$$$$$  |\n");
    fb_print(" \\__|  \\__| \\_______|\\_______/  \\______/  \\_______/ \\______/  \\______/ \n\n");

    for (;;) {
        fb_print("user@nasua-pc:/>  ");
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
