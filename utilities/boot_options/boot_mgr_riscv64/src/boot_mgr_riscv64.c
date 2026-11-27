/* NasuaOS boot manager for RISC-V (rv64).
 *
 * Loaded by Limine through the "limine" protocol. Picks the best kernel
 * for the CPU (the 64-bit kernel on riscv64), loads its ELF image into
 * free physical memory, extends Limine's own Sv48 page tables with an
 * identity map for low MMIO and the kernel's high-half range, fills in
 * the kernel's .limine_requests responses and jumps to its entry point.
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

/* ---- UART (NS16550) ---------------------------------------------------- */

#define UART_PHYS 0x10000000UL
static u64 hhdm;
static volatile u8 *uart;
static int uart_on;

static void uart_putc(char c) {
    if (!uart_on) return;
    if (!uart) uart = (volatile u8 *)UART_PHYS;
    while (!(uart[5] & 0x20)) {}
    uart[0] = (u8)c;
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_puthex(u64 v) {
    char buf[17]; int i = 16; buf[i] = 0;
    if (!v) { uart_putc('0'); return; }
    while (v) { int d = v & 0xf; buf[--i] = d < 10 ? '0'+d : 'a'+d-10; v >>= 4; }
    uart_puts(&buf[i]);
}

/* ---- page tables (Sv48) -------------------------------------------------
   PTE: bit0 V, bit1 R, bit2 W, bit3 X, bit5 G, bit6 A, bit7 D, bit8 U.
   Pointer entries have R=W=X=0. Leaf entries set R/W/X plus A|D|G.
   Address field = PPN = phys[55:12] stored at bits [53:10]. */

#define PTE_V   (1ULL << 0)
#define PTE_R   (1ULL << 1)
#define PTE_W   (1ULL << 2)
#define PTE_X   (1ULL << 3)
#define PTE_G   (1ULL << 5)
#define PTE_A   (1ULL << 6)
#define PTE_D   (1ULL << 7)
#define PTE_U   (1ULL << 8)

#define PPN_SHIFT 10
#define PTE_ADDR(p) ((((u64)(p)) >> 12) << PPN_SHIFT)
#define PTE_IS_PTR(e) ((e) & PTE_V && !((e) & (PTE_R|PTE_W|PTE_X)))

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

static u64 satp_root(void)
{
    u64 v;
    asm volatile("csrr %0, satp" : "=r"(v));
    return (v & 0x00000FFFFFFFFFFFULL) << 12;
}

static void sfence(void)
{
    asm volatile("sfence.vma" ::: "memory");
}

static u64 *pt_table_at(u64 root_phys, u64 virt, int level, int create)
{
    /* returns pointer to the page-table entry slot at `level` (3=root,2,1,0=leaf slot) */
    u64 *t = pt_v2p(root_phys);
    static const int shift[4] = {12, 21, 30, 39};  /* level -> vpn shift */
    for (int l = 3; l > level; l--) {
        u64 idx = (virt >> shift[l]) & 0x1ff;
        u64 e = t[idx];
        if (!PTE_IS_PTR(e)) {
            if (!create) return NULL;
            u64 p = alloc_page(); if (!p) return NULL;
            memset(pt_v2p(p), 0, 0x1000);
            e = PTE_V | PTE_ADDR(p);
            t[idx] = e;
        }
        t = pt_v2p((e >> PPN_SHIFT) << 12);
    }
    return &t[(virt >> shift[level]) & 0x1ff];
}

static void map_4k(u64 root, u64 virt, u64 phys, u64 flags)
{
    u64 *slot = pt_table_at(root, virt, 0, 1);
    if (slot) *slot = PTE_V|PTE_R|PTE_W|PTE_X|PTE_A|PTE_D|PTE_G|flags|PTE_ADDR(phys & ~0xFFFULL);
}

/* map [vbase, +size) -> pbase; 2MB blocks when aligned (leaf at level 1) */
static void map_vrange(u64 root, u64 vbase, u64 pbase, u64 size)
{
    u64 done = 0;
    while (done < size) {
        u64 va = vbase + done, pa = pbase + done;
        if (((va | pa) & 0x1FFFFFULL) == 0 && (size - done) >= 0x200000) {
            u64 *slot = pt_table_at(root, va, 1, 1);
            if (!slot) return;
            *slot = PTE_V|PTE_R|PTE_W|PTE_X|PTE_A|PTE_D|PTE_G|PTE_ADDR(pa & ~0x1FFFFFULL);
            done += 0x200000;
        } else {
            map_4k(root, va & ~0xFFFULL, pa & ~0xFFFULL, 0);
            done += 0x1000;
        }
    }
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
                if (img[j]==ID_BASEREV[0] && img[j+1]==ID_BASEREV[1]) { j += 3; continue; }
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

    if (!mods || !hh || !mm) for (;;) asm volatile("wfi");
    hhdm = hh->offset;

    u64 kfile_addr = 0;
    for (u64 i = 0; i < mods->module_count; i++) {
        struct limine_file *f = mods->modules[i];
        if (f->path && xcontains(f->path, "/kernel_64bit/risc-v/kernel")) {
            kfile_addr = f->address; kfile_size = f->size; break;
        }
    }
    if (!kfile_addr && mods->module_count) {
        kfile_addr = mods->modules[0]->address;
        kfile_size = mods->modules[0]->size;
    }
    if (!kfile_addr) for (;;) asm volatile("wfi");

    u64 kfile_vaddr = kfile_addr;

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
    if (!best_len) for (;;) asm volatile("wfi");

    alloc_next = best_base + best_len;
    alloc_left = best_len;
    u64 kphys = alloc_next - 0x100000;   /* reserve top 1MB for kernel */
    alloc_next = kphys;

    u64 root = satp_root();

    /* identity map low 2GiB (NS16550 UART @0x10000000, virtio, PLIC, CLINT) */
    map_vrange(root, 0x1000, 0x1000, 0x80000000ULL - 0x1000);
    sfence();
    uart_on = 1;
    uart_puts("[boot_mgr] identity ok, hole=0x"); uart_puthex(best_base);
    uart_puts("+0x"); uart_puthex(best_len); uart_putc('\n');

    if (!load_elf64(kfile_vaddr, kphys)) {
        uart_puts("[boot_mgr] elf load failed\n");
        for (;;) asm volatile("wfi");
    }
    uart_puts("[boot_mgr] elf ok entry=0x"); uart_puthex(kentry); uart_putc('\n');

    u64 kspan = (kmax_v - kmin_v + 0xFFF) & ~0xFFFULL;

    /* kernel high-half range */
    {
        u64 kv0 = kmin_v & ~0xFFFULL;
        u64 kv1 = (kmax_v + 0xFFF) & ~0xFFFULL;
        map_vrange(root, kv0, kphys + (kv0 - 0xffffffff80000000ULL), kv1 - kv0);
        sfence();
    }

    uart_puts("[boot_mgr] k64 entry=0x"); uart_puthex(kentry);
    uart_puts(" phys=0x"); uart_puthex(kphys);
    uart_puts("\n");

    fill_kernel_requests(kphys, kspan);

    register u64 entry asm("t0") = kentry;
    asm volatile("mv a0, zero\n\tjr %0" :: "r"(entry) :
        "ra","gp","tp","t1","t2","s0","s1","s2","s3","s4","s5","s6","s7",
        "s8","s9","s10","s11","a0","a1","a2","a3","a4","a5","a6","a7",
        "memory");
    for (;;) asm volatile("wfi");
}
