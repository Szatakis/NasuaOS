/* NasuaOS boot manager for aarch64.
 *
 * Loaded by Limine through the "limine" protocol. Picks the best kernel
 * for the CPU (the 64-bit kernel on aarch64), loads its ELF image into
 * free physical memory, extends Limine's own page tables with an identity
 * map for low MMIO and the kernel's high-half range, fills in the kernel's
 * .limine_requests responses and jumps to its entry point.
 */

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define NULL ((void*)0)

/* ---- Limine protocol -------------------------------------------------- */

struct limine_file {
    u64 revision;
    u64 address;
    u64 size;
    char *path;
    char *string;
};

struct limine_module_response {
    u64 revision;
    u64 module_count;
    struct limine_file **modules;
};

struct limine_memmap_entry {
    u64 base;
    u64 length;
    u64 type;
};

struct limine_memmap_response {
    u64 revision;
    u64 entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_hhdm_response {
    u64 revision;
    u64 offset;
};

struct limine_framebuffer_response {
    u64 revision;
    u64 framebuffer_count;
    void **framebuffers;
};

struct limine_request {
    u64 id[4];
    u64 revision;
    void *response;
};

/* request IDs: {COMMON_MAGIC[2], id[2]} */
#define MAG0 0xc7b1dd30df4c8b88ULL
#define MAG1 0x0a82e883a194f07bULL

static const u64 ID_MODULE[4] = {MAG0, MAG1, 0x3e7e279702be32afULL, 0xca1c4f3bd1280ceeULL};
static const u64 ID_MEMMAP[4] = {MAG0, MAG1, 0x67cf3d9d378a806fULL, 0xe304acdfc50c3c62ULL};
static const u64 ID_HHDM[4]   = {MAG0, MAG1, 0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL};
static const u64 ID_FB[4]     = {MAG0, MAG1, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL};
static const u64 ID_BASEREV[2] = {0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL};

__attribute__((used, section(".limine_requests_start")))
static volatile u64 reqs_start[] = {
    0xf6b8f4b39de7d1aeULL, 0xfab91a6940fcb9cfULL,
    0x785c6ed015d3e316ULL, 0x181e920a7852b9d9ULL
};
__attribute__((used, section(".limine_requests")))
static volatile u64 base_rev_req[3] = {
    0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, 6
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_request module_req = {
    {ID_MODULE[0],ID_MODULE[1],ID_MODULE[2],ID_MODULE[3]}, 0, NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_request memmap_req = {
    {ID_MEMMAP[0],ID_MEMMAP[1],ID_MEMMAP[2],ID_MEMMAP[3]}, 0, NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_request hhdm_req = {
    {ID_HHDM[0],ID_HHDM[1],ID_HHDM[2],ID_HHDM[3]}, 0, NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_request fb_req = {
    {ID_FB[0],ID_FB[1],ID_FB[2],ID_FB[3]}, 0, NULL
};
__attribute__((used, section(".limine_requests_end")))
static volatile u64 reqs_end[] = {
    0xadc0e0531bb10d03ULL, 0x9572709f31764c62ULL
};

/* ---- tiny libc --------------------------------------------------------- */

static void *memset(void *d, int c, u64 n) {
    u8 *p = d;
    while (n--) *p++ = (u8)c;
    return d;
}
static void *memcpy(void *d, const void *s, u64 n) {
    u8 *dd = d; const u8 *ss = s;
    while (n--) *dd++ = *ss++;
    return d;
}
static u64 xstrlen(const char *s) { u64 n=0; while(s[n]) n++; return n; }
static int xcontains(const char *h, const char *n) {
    u64 nl = xstrlen(n);
    if (!nl) return 1;
    for (; *h; h++) {
        u64 i = 0;
        while (i < nl && h[i] == n[i]) i++;
        if (i == nl) return 1;
    }
    return 0;
}

/* ---- UART (PL011) ------------------------------------------------------ */

#define UART_PHYS 0x09000000UL
static u64 hhdm;
static volatile u32 *uart;
static int uart_on;

static void uart_putc(char c) {
    /* Limine's rev-6 pagemap has no low-half identity map and the HHDM
       skips MMIO holes, so the PL011 is unreachable until our own
       TTBR0 identity map is installed. */
    if (!uart_on) return;
    if (!uart) {
        uart = (volatile u32 *)UART_PHYS;
        uart[12] = 0x301;   /* UARTCR: UARTEN | TXE | RXE */
    }
    while (uart[6] & 0x20) {}
    uart[0] = (u32)(u8)c;
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_puthex(u64 v) {
    char buf[17]; int i = 16; buf[i] = 0;
    if (!v) { uart_putc('0'); return; }
    while (v) { int d = v & 0xf; buf[--i] = d < 10 ? '0'+d : 'a'+d-10; v >>= 4; }
    uart_puts(&buf[i]);
}

/* ---- page tables --------------------------------------------------------
   Descriptor format (AArch64 4K granule, same as Limine's vmm):
   bit0 VALID, bit1 TABLE/4K-page, bits[4:2] AttrIndx, bit6 USER,
   bit7 READONLY, bits[9:8] SH, bit10 ACCESS, bit11 NG.
   RAM flags = VALID|ACCESS|NG|SH_INNER = 0xF01 ; +TABLE bit for 4K page.
   AttrIndx 0 = normal WB, 2 = device-nGnRnE. */

#define PT_VALID    (1ULL << 0)
#define PT_TABLE    (1ULL << 1)
#define PT_ACCESS   (1ULL << 10)
#define PT_NG       (1ULL << 11)
#define PT_SH_INNER (3ULL << 8)
#define PT_ATTR_WB  0ULL
#define PT_ATTR_DEV (2ULL << 2)

#define PADDR_MASK  0x0000FFFFFFFFF000ULL

#define DESC_TABLE(p) ((p) | PT_VALID | PT_TABLE)
#define DESC_4K(p,f)  ((p) | PT_VALID | PT_TABLE | PT_ACCESS | PT_NG | PT_SH_INNER | (f))
#define DESC_2M(p,f)  ((p) | PT_VALID          | PT_ACCESS | PT_NG | PT_SH_INNER | (f))

static u64 *pt_v2p(u64 phys) { return (u64*)(hhdm + phys); }

/* bump allocator from the top of the chosen usable hole */
static u64 alloc_next = 0;
static u64 alloc_left = 0;

static u64 alloc_page(void) {
    if (alloc_left < 0x1000) return 0;
    alloc_next -= 0x1000;
    alloc_left -= 0x1000;
    return alloc_next;
}

static u64 *pt_l3(u64 top_phys, u64 virt, int create)
{
    u64 *l0 = pt_v2p(top_phys);
    u64 e = l0[(virt >> 39) & 0x1ff];
    if (!(e & PT_VALID)) {
        if (!create) return NULL;
        u64 p = alloc_page(); if (!p) return NULL;
        memset(pt_v2p(p), 0, 0x1000);
        l0[(virt >> 39) & 0x1ff] = DESC_TABLE(p);
        e = l0[(virt >> 39) & 0x1ff];
    }
    u64 *l1 = pt_v2p(e & PADDR_MASK);
    e = l1[(virt >> 30) & 0x1ff];
    if (!(e & PT_VALID)) {
        if (!create) return NULL;
        u64 p = alloc_page(); if (!p) return NULL;
        memset(pt_v2p(p), 0, 0x1000);
        l1[(virt >> 30) & 0x1ff] = DESC_TABLE(p);
        e = l1[(virt >> 30) & 0x1ff];
    }
    u64 *l2 = pt_v2p(e & PADDR_MASK);
    e = l2[(virt >> 21) & 0x1ff];
    if (!(e & PT_VALID)) {
        if (!create) return NULL;
        u64 p = alloc_page(); if (!p) return NULL;
        memset(pt_v2p(p), 0, 0x1000);
        l2[(virt >> 21) & 0x1ff] = DESC_TABLE(p);
        e = l2[(virt >> 21) & 0x1ff];
    }
    return pt_v2p(e & PADDR_MASK);
}

static void map_4k(u64 top, u64 virt, u64 phys, u64 flags)
{
    u64 *l3 = pt_l3(top, virt, 1);
    if (l3) l3[(virt >> 12) & 0x1ff] = DESC_4K(phys & ~0xFFFULL, flags);
}

static u64 read_ttbr(int which)
{
    u64 v;
    if (which == 0) asm volatile("mrs %0, ttbr0_el1" : "=r"(v));
    else            asm volatile("mrs %0, ttbr1_el1" : "=r"(v));
    return v & PADDR_MASK;
}

static void write_ttbr0(u64 phys)
{
    asm volatile("msr ttbr0_el1, %0" :: "r"(phys) : "memory");
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("tlbi vmalle1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

/* ---- ELF64 ------------------------------------------------------------- */

struct ehdr64 { u8 ident[16]; u16 type,machine; u32 version; u64 entry,phoff,shoff; u32 flags; u16 ehsize,phentsize,phnum,shentsize,shnum,shstrndx; };
struct phdr64 { u32 type,flags; u64 offset,vaddr,paddr,filesz,memsz,align; };

static u64 kentry, kmin_v, kmax_v;

static int load_elf64(u64 file_vaddr, u64 dest_phys)
{
    struct ehdr64 *eh = (struct ehdr64*)file_vaddr;
    if (eh->ident[0] != 0x7f || eh->ident[1] != 'E' || eh->ident[4] != 2) return 0;
    kentry = eh->entry;
    kmin_v = ~0ULL; kmax_v = 0;
    for (u64 i = 0; i < eh->phnum; i++) {
        struct phdr64 *ph = (struct phdr64*)(file_vaddr + eh->phoff + i*eh->phentsize);
        if (ph->type != 1) continue;
        u64 dp = dest_phys + (ph->vaddr - 0xffffffff80000000ULL);
        memcpy((void*)(hhdm + dp), (void*)(file_vaddr + ph->offset), ph->filesz);
        if (ph->memsz > ph->filesz)
            memset((void*)(hhdm + dp + ph->filesz), 0, ph->memsz - ph->filesz);
        if (ph->vaddr < kmin_v) kmin_v = ph->vaddr;
        if (ph->vaddr + ph->memsz > kmax_v) kmax_v = ph->vaddr + ph->memsz;
    }
    return kmin_v != ~0ULL;
}

/* ---- kernel .limine_requests -------------------------------------------- */

static const u64 MARK_START[4] = {0xf6b8f4b39de7d1aeULL,0xfab91a6940fcb9cfULL,0x785c6ed015d3e316ULL,0x181e920a7852b9d9ULL};
static const u64 MARK_END0 = 0xadc0e0531bb10d03ULL;
static const u64 MARK_END1 = 0x9572709f31764c62ULL;

static int id4_eq(const u64 *a, const u64 *b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

/* response copies that must live inside our own (still mapped) image */
static u64 fb_resp_copy[4];
static u64 fb_ptr_copy[2];
static u8  fb_struct_copy[128];

static void fill_kernel_requests(u64 kphys, u64 ksize)
{
    u64 *img = pt_v2p(kphys);
    u64 n = ksize / 8;
    for (u64 i = 0; i + 8 <= n; i++) {
        if (img[i]==MARK_START[0] && img[i+1]==MARK_START[1] &&
            img[i+2]==MARK_START[2] && img[i+3]==MARK_START[3]) {
            u64 j = i + 4;
            while (j + 2 <= n) {
                if (img[j]==MARK_END0 && img[j+1]==MARK_END1) break;
                /* base revision pseudo-request: 3 u64s, no response */
                if (img[j]==ID_BASEREV[0] && img[j+1]==ID_BASEREV[1]) { j += 3; continue; }
                /* full request: id[4] revision response = 6 u64s */
                if (j + 6 <= n && id4_eq(&img[j], ID_FB)) {
                    struct limine_framebuffer_response *src =
                        (struct limine_framebuffer_response*)fb_req.response;
                    if (src && src->framebuffer_count && src->framebuffers) {
                        fb_resp_copy[0] = src->revision;
                        fb_resp_copy[1] = 1;
                        fb_resp_copy[2] = (u64)fb_ptr_copy;
                        fb_ptr_copy[0] = (u64)fb_struct_copy;
                        memcpy(fb_struct_copy, src->framebuffers[0], 128);
                        img[j+5] = (u64)fb_resp_copy;
                    }
                }
                j += 6;
            }
            break;
        }
    }
}

/* ---- main ---------------------------------------------------------------- */

void kmain(void)
{
    struct limine_module_response *mods =
        (struct limine_module_response*)module_req.response;
    struct limine_hhdm_response *hh =
        (struct limine_hhdm_response*)hhdm_req.response;
    struct limine_memmap_response *mm =
        (struct limine_memmap_response*)memmap_req.response;

    if (!mods || !hh || !mm) for (;;) asm volatile("wfe");
    hhdm = hh->offset;

    /* find kernel module by path */
    u64 kfile_addr = 0, kfile_size = 0;
    for (u64 i = 0; i < mods->module_count; i++) {
        struct limine_file *f = mods->modules[i];
        if (f->path && xcontains(f->path, "/kernel_64bit/arm/kernel")) {
            kfile_addr = f->address; kfile_size = f->size; break;
        }
    }
    if (!kfile_addr && mods->module_count) {
        kfile_addr = mods->modules[0]->address;
        kfile_size = mods->modules[0]->size;
    }
    if (!kfile_addr) {
        uart_puts("[boot_mgr] no kernel module\n");
        for (;;) asm volatile("wfe");
    }
    uart_puts("[boot_mgr] kfile=0x"); uart_puthex(kfile_addr);
    uart_puts(" size=0x"); uart_puthex(kfile_size); uart_putc('\n');

    u64 kfile_vaddr = kfile_addr;   /* used directly: Limine addresses are valid vaddrs */

    /* carve allocation space: top of largest usable hole not overlapping modules */
    u64 best_base = 0, best_len = 0;
    for (u64 i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type != 0) continue;
        int overlap = 0;
        for (u64 m = 0; m < mods->module_count; m++) {
            u64 ma = mods->modules[m]->address;
            u64 mp = (ma >= hhdm) ? ma - hhdm : ma;
            u64 me = mp + mods->modules[m]->size;
            if (mp < e->base + e->length && me > e->base) { overlap = 1; break; }
        }
        if (!overlap && e->length > best_len) { best_len = e->length; best_base = e->base; }
    }
    if (!best_len) {
        uart_puts("[boot_mgr] no usable hole\n");
        for (;;) asm volatile("wfe");
    }
    uart_puts("[boot_mgr] hole=0x"); uart_puthex(best_base);
    uart_puts("+0x"); uart_puthex(best_len); uart_putc('\n');

    /* kernel image + page tables from the top of the hole */
    alloc_next = best_base + best_len;
    alloc_left = best_len;
    u64 kphys = alloc_next - 0x100000;   /* reserve top 1MB for kernel */
    alloc_next = kphys;

    /* TTBR0 first: Limine's rev-6 pagemap has no low-half map and the HHDM
       skips MMIO holes, so install our own identity map for the first 2GiB
       (PL011 UART, FW_CFG, RAMFB, GIC) before doing anything else. */
    {
        u64 l0p = alloc_page();
        if (l0p) {
            memset(pt_v2p(l0p), 0, 0x1000);
            u64 l1p = alloc_page();
            if (l1p) {
                memset(pt_v2p(l1p), 0, 0x1000);
                pt_v2p(l0p)[0] = DESC_TABLE(l1p);
                for (u64 t = 0; t < 2; t++) {
                    u64 l2p = alloc_page();
                    if (!l2p) break;
                    memset(pt_v2p(l2p), 0, 0x1000);
                    pt_v2p(l1p)[t] = DESC_TABLE(l2p);
                    for (u64 b = 0; b < 512; b++) {
                        u64 pa = (t * 512 + b) * 0x200000ULL;
                        u64 fl = PT_ATTR_WB;
                        if ((pa >= 0x08000000ULL && pa < 0x0A000000ULL) ||
                            (pa >= 0x48000000ULL && pa < 0x4A000000ULL))
                            fl = PT_ATTR_DEV;
                        pt_v2p(l2p)[b] = DESC_2M(pa, fl);
                    }
                }
                write_ttbr0(l0p);
                uart_on = 1;
            }
        }
    }
    uart_puts("[boot_mgr] ttbr0 ok, hole=0x"); uart_puthex(best_base);
    uart_puts("+0x"); uart_puthex(best_len); uart_putc('\n');

    if (!load_elf64(kfile_vaddr, kphys)) {
        uart_puts("[boot_mgr] elf load failed\n");
        for (;;) asm volatile("wfe");
    }
    uart_puts("[boot_mgr] elf ok entry=0x"); uart_puthex(kentry); uart_putc('\n');

    u64 kspan = (kmax_v - kmin_v + 0xFFF) & ~0xFFFULL;

    /* ---- debug output now possible? UART needs TTBR0 identity map ---- */

    u64 ttbr1 = read_ttbr(1);

    /* TTBR1: map kernel high-half range */
    {
        u64 kv0 = kmin_v & ~0xFFFULL;
        u64 kv1 = (kmax_v + 0xFFF) & ~0xFFFULL;
        u64 total = kv1 - kv0, done = 0;
        while (done < total) {
            u64 va = kv0 + done;
            u64 pa = kphys + (va - 0xffffffff80000000ULL);
            if (((va | pa) & 0x1FFFFFULL) == 0 && (total - done) >= 0x200000) {
                u64 *l0 = pt_v2p(ttbr1);
                u64 e = l0[(va >> 39) & 0x1ff];
                if (!(e & PT_VALID)) {
                    u64 p = alloc_page(); if (!p) goto out;
                    memset(pt_v2p(p), 0, 0x1000);
                    l0[(va >> 39) & 0x1ff] = DESC_TABLE(p);
                    e = l0[(va >> 39) & 0x1ff];
                }
                u64 *l1 = pt_v2p(e & PADDR_MASK);
                e = l1[(va >> 30) & 0x1ff];
                if (!(e & PT_VALID)) {
                    u64 p = alloc_page(); if (!p) goto out;
                    memset(pt_v2p(p), 0, 0x1000);
                    l1[(va >> 30) & 0x1ff] = DESC_TABLE(p);
                    e = l1[(va >> 30) & 0x1ff];
                }
                u64 *l2 = pt_v2p(e & PADDR_MASK);
                l2[(va >> 21) & 0x1ff] = DESC_2M(pa, PT_ATTR_WB);
                done += 0x200000;
            } else {
                map_4k(ttbr1, va, pa, PT_ATTR_WB);
                done += 0x1000;
            }
        }
    }

out:
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");

    uart_puts("[boot_mgr] k64 entry=0x"); uart_puthex(kentry);
    uart_puts(" phys=0x"); uart_puthex(kphys);
    uart_puts("\n");

    fill_kernel_requests(kphys, kspan);

    register u64 entry asm("x9") = kentry;
    asm volatile("mov x0, xzr\n\tblr %0" :: "r"(entry) :
        "x0","x1","x2","x3","x4","x5","x6","x7","x8","x10","x11","x12","x13",
        "x14","x15","x16","x17","x18","x19","x20","x21","x22","x23","x24","x25",
        "x26","x27","x28","x30","memory");
    for (;;) asm volatile("wfe");
}
