#include <stdint.h>

#include "../../../../resources/fonts/font8x8.h"


#define MULTIBOOT2_MAGIC     0x36D76289

#define MB2_TAG_END          0
#define MB2_TAG_ACPI_OLD     14
#define MB2_TAG_ACPI_NEW     15
#define MB2_TAG_MMAP         6
#define MB2_TAG_MODULE       3
#define MB2_TAG_FRAMEBUFFER  8


struct mb2_tag
{
    uint32_t type;
    uint32_t size;
};


struct mb2_tag_module
{
    uint32_t type;
    uint32_t size;

    uint32_t mod_start;
    uint32_t mod_end;

    char cmdline[0];
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


struct mb2_mmap_entry
{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};


// ---------------------------------------------------------------------------
// ELF structures
// ---------------------------------------------------------------------------

struct elf32_ehdr
{
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf32_phdr
{
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
};

struct elf64_ehdr
{
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf64_phdr
{
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};


// ---------------------------------------------------------------------------
// Limine protocol structures (subset needed by the NasuaOS 64-bit kernel)
// ---------------------------------------------------------------------------

#define LIMINE_COMMON_MAGIC_0 0xc7b1dd30df4c8b88ULL
#define LIMINE_COMMON_MAGIC_1 0x0a82e883a194f07bULL

#define LIMINE_BASE_REV_TAG_0  0xf9562b2d5c95a6c8ULL
#define LIMINE_BASE_REV_TAG_1  0x6a7b384944536bdcULL
#define LIMINE_SUPPORTED_REV   6

#define LIMINE_REQ_START_0 0xf6b8f4b39de7d1aeULL
#define LIMINE_REQ_START_1 0xfab91a6940fcb9cfULL
#define LIMINE_REQ_START_2 0x785c6ed015d3e316ULL
#define LIMINE_REQ_START_3 0x181e920a7852b9d9ULL
#define LIMINE_REQ_END_0   0xadc0e0531bb10d03ULL
#define LIMINE_REQ_END_1   0x9572709f31764c62ULL

#define LIMINE_ID_BOOTLOADER_INFO_2 0xf55038d8e2a1202fULL
#define LIMINE_ID_BOOTLOADER_INFO_3 0x279426fcf5f59740ULL
#define LIMINE_ID_FRAMEBUFFER_2     0x9d5827dcd881dd75ULL
#define LIMINE_ID_FRAMEBUFFER_3     0xa3148604f6fab11bULL
#define LIMINE_ID_MODULE_2          0x3e7e279702be32afULL
#define LIMINE_ID_MODULE_3          0xca1c4f3bd1280ceeULL
#define LIMINE_ID_MEMMAP_2          0x67cf3d9d378a806fULL
#define LIMINE_ID_MEMMAP_3          0xe304acdfc50c3c62ULL
#define LIMINE_ID_HHDM_2            0x48dcf1cb8ad2b852ULL
#define LIMINE_ID_HHDM_3            0x63984e959a98244bULL
#define LIMINE_ID_RSDP_2            0xc5e77b6b397e7b43ULL
#define LIMINE_ID_RSDP_3            0x27637845accdcf3cULL
#define LIMINE_ID_MP_2              0x95a67b819a1b857eULL
#define LIMINE_ID_MP_3              0xa0b61b723b6a73e0ULL

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6

#define HHDM_BASE 0xffff800000000000ULL
#define K64_VIRT_BASE 0xffffffff80000000ULL
#define K64_PHYS_BASE 0x2000000ULL          // 32 MiB, 2 MiB aligned


struct limine_bootloader_info_response
{
    uint64_t revision;
    uint64_t name;
    uint64_t version;
};

struct limine_video_mode;

struct limine_framebuffer
{
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  unused[7];
    uint64_t edid_size;
    uint64_t edid;
    uint64_t mode_count;
    uint64_t modes;
};

struct limine_video_mode
{
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
};

struct limine_framebuffer_response
{
    uint64_t revision;
    uint64_t framebuffer_count;
    uint64_t framebuffers;
};

struct limine_uuid
{
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t  d[8];
};

struct limine_file
{
    uint64_t revision;
    uint64_t address;
    uint64_t size;
    uint64_t path;
    uint64_t string;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    limine_uuid gpt_disk_uuid;
    limine_uuid gpt_part_uuid;
    limine_uuid part_uuid;
};

struct limine_module_response
{
    uint64_t revision;
    uint64_t module_count;
    uint64_t modules;
};

struct limine_memmap_entry
{
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response
{
    uint64_t revision;
    uint64_t entry_count;
    uint64_t entries;
};

struct limine_hhdm_response
{
    uint64_t revision;
    uint64_t offset;
};

struct limine_rsdp_response
{
    uint64_t revision;
    uint64_t address;
};

struct limine_mp_info
{
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t reserved;
    uint64_t goto_address;
    uint64_t extra_argument;
};

struct limine_mp_response
{
    uint64_t revision;
    uint32_t flags;
    uint32_t bsp_lapic_id;
    uint64_t cpu_count;
    uint64_t cpus;
};


// ---------------------------------------------------------------------------
// Small freestanding helpers
// ---------------------------------------------------------------------------

static void* kcopy(void* dst, const void* src, uint32_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
    return dst;
}

static void kfill(void* dst, uint8_t value, uint32_t n)
{
    uint8_t* d = (uint8_t*)dst;
    for (uint32_t i = 0; i < n; i++)
    {
        d[i] = value;
    }
}

static uint32_t kstrlen(const char* s)
{
    uint32_t n = 0;
    while (s[n])
    {
        n++;
    }
    return n;
}

static bool kcontains(const char* haystack, const char* needle)
{
    uint32_t n = kstrlen(needle);
    if (n == 0)
    {
        return true;
    }
    for (const char* p = haystack; *p; p++)
    {
        uint32_t i = 0;
        while (i < n && p[i] == needle[i])
        {
            i++;
        }
        if (i == n)
        {
            return true;
        }
    }
    return false;
}

static void* kalloc_aligned(uint8_t* pool, uint32_t pool_size, uint32_t* used, uint32_t size, uint32_t align)
{
    uint32_t addr = (*used + align - 1) & ~(align - 1);
    if (addr + size > pool_size)
    {
        return nullptr;
    }
    *used = addr + size;
    void* p = pool + addr;
    kfill(p, 0, size);
    return p;
}


// ---------------------------------------------------------------------------
// Framebuffer / display
// ---------------------------------------------------------------------------

static uint8_t* framebuffer = nullptr;

static uint32_t fb_width  = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch  = 0;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;


static const uint32_t COLOR_BLACK = 0x00101820;
static const uint32_t COLOR_WHITE = 0x00FFFFFF;
static const uint32_t COLOR_GREEN = 0x0000FF00;
static const uint32_t COLOR_RED   = 0x00FF0000;
static const uint32_t COLOR_SEA   = 0x0063C7B7;


static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!framebuffer || x >= fb_width || y >= fb_height)
    {
        return;
    }

    uint32_t* pixel = (uint32_t*)(framebuffer + y * fb_pitch + x * 4);

    *pixel = color;
}


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


            if (fb->framebuffer_type != 1 || fb->framebuffer_bpp != 32)
            {
                return false;
            }

            framebuffer = (uint8_t*)(uint32_t)fb->framebuffer_addr;
            fb_width = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch = fb->framebuffer_pitch;


            return true;
        }


        current += (tag->size + 7) & ~7;
    }


    return false;
}


// ---------------------------------------------------------------------------
// Modules provided by Limine
// ---------------------------------------------------------------------------

#define MAX_MODULES 16

struct boot_module
{
    uint32_t start;
    uint32_t end;
    const char* name;   // module_string passed through Limine, may be ""
};

static boot_module modules[MAX_MODULES];
static uint32_t module_count = 0;


static bool find_modules(uint32_t mb_info)
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


        if (tag->type == MB2_TAG_MODULE)
        {
            if (module_count < MAX_MODULES)
            {
                mb2_tag_module* m = (mb2_tag_module*)tag;
                modules[module_count].start = m->mod_start;
                modules[module_count].end = m->mod_end;
                modules[module_count].name = m->cmdline;
                module_count++;
            }
        }


        current += (tag->size + 7) & ~7;
    }


    return module_count != 0;
}


static int find_module_by_name(const char* needle)
{
    for (uint32_t i = 0; i < module_count; i++)
    {
        if (modules[i].name[0] != '\0' && kcontains(modules[i].name, needle))
        {
            return (int)i;
        }
    }
    return -1;
}


// ---------------------------------------------------------------------------
// Keyboard menu
// ---------------------------------------------------------------------------

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_putc(char c)
{
    while ((inb(0x3FD) & 0x20) == 0)
    {
    }
    outb(0x3F8, (uint8_t)c);
}

static void serial_puts(const char* s)
{
    while (*s)
    {
        serial_putc(*s++);
    }
}

static void serial_hex64(uint64_t value)
{
    const char* hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--)
    {
        serial_putc(hex[(value >> (i * 4)) & 0xF]);
    }
}

static void print_hex64_dbg(uint64_t value)
{
    serial_puts("0x");
    serial_hex64(value);
}


extern "C" int cpu_has_long_mode();


// ---------------------------------------------------------------------------
// ELF loaders
// ---------------------------------------------------------------------------

// Load an ELF32 image into physical memory at the addresses from its
// program headers. Returns the entry point, or 0 on failure.
static uint32_t load_elf32(uint32_t file_addr, uint32_t file_size,
                           uint32_t* min_addr, uint32_t* max_addr)
{
    if (file_size < sizeof(elf32_ehdr))
    {
        return 0;
    }

    elf32_ehdr* eh = (elf32_ehdr*)file_addr;

    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' || eh->ident[2] != 'L' ||
        eh->ident[3] != 'F' || eh->ident[4] != 1 || eh->ident[5] != 1)
    {
        return 0;
    }

    if ((uint32_t)eh->phoff + (uint32_t)eh->phentsize * eh->phnum > file_size)
    {
        return 0;
    }

    *min_addr = 0xFFFFFFFF;
    *max_addr = 0;

    for (uint16_t i = 0; i < eh->phnum; i++)
    {
        elf32_phdr* ph = (elf32_phdr*)(file_addr + eh->phoff + i * eh->phentsize);

        if (ph->type != 1) // PT_LOAD
        {
            continue;
        }

        if (ph->paddr + ph->memsz < ph->paddr ||
            ph->offset + ph->filesz > file_size ||
            ph->paddr + ph->memsz > 0xFFFFFFFF)
        {
            return 0;
        }

        kcopy((void*)(uint32_t)ph->paddr,
              (const void*)(file_addr + ph->offset),
              ph->filesz);

        if (ph->memsz > ph->filesz)
        {
            kfill((void*)(uint32_t)(ph->paddr + ph->filesz), 0,
                  ph->memsz - ph->filesz);
        }

        uint32_t seg_end = ph->paddr + ph->memsz;
        if (ph->paddr < *min_addr) *min_addr = ph->paddr;
        if (seg_end > *max_addr)   *max_addr = seg_end;
    }

    return eh->entry;
}


struct elf64_load_info
{
    uint64_t entry;
    uint64_t min_vaddr;
    uint64_t max_vaddr;
    uint64_t min_phys;
    uint64_t max_phys;
};


// Compute the physical span the ELF64 image would occupy for a given
// physical base: [base + min_off, base + max_off).
static bool elf64_image_span(uint32_t file_addr, uint32_t file_size,
                             uint64_t* min_off, uint64_t* max_off)
{
    if (file_size < sizeof(elf64_ehdr))
    {
        return false;
    }

    elf64_ehdr* eh = (elf64_ehdr*)file_addr;
    if ((uint64_t)eh->phoff + (uint64_t)eh->phentsize * eh->phnum > file_size)
    {
        return false;
    }

    *min_off = ~0ULL;
    *max_off = 0;
    for (uint16_t i = 0; i < eh->phnum; i++)
    {
        elf64_phdr* ph = (elf64_phdr*)(file_addr + eh->phoff + i * eh->phentsize);
        if (ph->type != 1)
        {
            continue;
        }
        uint64_t off = ph->vaddr - K64_VIRT_BASE;
        uint64_t end = off + ph->memsz;
        if (off < *min_off) *min_off = off;
        if (end > *max_off) *max_off = end;
    }
    return *min_off != ~0ULL;
}


// This binary's own physical footprint (see linker.lds). Under the
// multiboot2 protocol Limine does not mark the loaded kernel image as
// KERNEL_AND_MODULES, so we must protect it ourselves in the memmap we
// hand to the 64-bit kernel.
#define BOOTMGR_PHYS_BASE 0x8000000ULL
#define BOOTMGR_PHYS_SPAN 0x200000ULL


// Load an ELF64 higher-half kernel into physical memory. The virtual
// addresses are kept as linked; physical placement is phys_base +
// (vaddr - K64_VIRT_BASE).
static bool load_elf64(uint32_t file_addr, uint32_t file_size,
                       uint64_t phys_base, elf64_load_info* info)
{
    if (file_size < sizeof(elf64_ehdr))
    {
        return false;
    }

    elf64_ehdr* eh = (elf64_ehdr*)file_addr;

    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' || eh->ident[2] != 'L' ||
        eh->ident[3] != 'F' || eh->ident[4] != 2 || eh->ident[5] != 1)
    {
        return false;
    }

    if ((uint64_t)eh->phoff + (uint64_t)eh->phentsize * eh->phnum > file_size)
    {
        return false;
    }

    info->min_vaddr = ~0ULL;
    info->max_vaddr = 0;
    info->min_phys  = ~0ULL;
    info->max_phys  = 0;

    for (uint16_t i = 0; i < eh->phnum; i++)
    {
        elf64_phdr* ph = (elf64_phdr*)(file_addr + eh->phoff + i * eh->phentsize);

        if (ph->type != 1) // PT_LOAD
        {
            continue;
        }

        if (ph->vaddr < K64_VIRT_BASE ||
            ph->offset + ph->filesz > file_size ||
            ph->vaddr + ph->memsz < ph->vaddr)
        {
            return false;
        }

        uint64_t delta = K64_VIRT_BASE - phys_base;
        uint64_t dest   = ph->vaddr - delta;
        uint64_t vend   = ph->vaddr + ph->memsz;
        uint64_t pend   = dest + ph->memsz;

        if (pend > 0x100000000ULL)
        {
            return false;
        }

        kcopy((void*)(uint32_t)dest,
              (const void*)(file_addr + ph->offset),
              (uint32_t)ph->filesz);

        if (ph->memsz > ph->filesz)
        {
            kfill((void*)(uint32_t)(dest + ph->filesz), 0,
                  (uint32_t)(ph->memsz - ph->filesz));
        }

        if (ph->vaddr < info->min_vaddr) info->min_vaddr = ph->vaddr;
        if (vend > info->max_vaddr)         info->max_vaddr = vend;
        if (dest < info->min_phys)          info->min_phys  = dest;
        if (pend > info->max_phys)          info->max_phys  = pend;
    }

    if (info->min_vaddr == ~0ULL)
    {
        return false;
    }

    info->entry = eh->entry;
    return true;
}


// ---------------------------------------------------------------------------
// Page tables for the 64-bit handoff
// ---------------------------------------------------------------------------

#define PT_POOL_SIZE (1024 * 1024)
static uint8_t pt_pool[PT_POOL_SIZE] __attribute__((aligned(4096)));
static uint32_t pt_pool_used = 0;

static uint64_t* pml4 = nullptr;

static uint64_t* pt_alloc()
{
    return (uint64_t*)kalloc_aligned(pt_pool, PT_POOL_SIZE, &pt_pool_used, 4096, 4096);
}

// Return the child table referenced by *entry, allocating it when empty.
static uint64_t* pt_child(uint64_t* entry)
{
    if (*entry == 0)
    {
        *entry = (uint64_t)pt_alloc() | 0x3; // present | writable
    }
    return (uint64_t*)(*entry & ~0xFFFULL);
}

static void map_4k(uint64_t virt, uint64_t phys)
{
    uint64_t* pdpt = pt_child(&pml4[(virt >> 39) & 0x1FF]);
    uint64_t* pd   = pt_child(&pdpt[(virt >> 30) & 0x1FF]);
    uint64_t* pt   = pt_child(&pd[(virt >> 21) & 0x1FF]);
    pt[(virt >> 12) & 0x1FF] = phys | 0x3;
}

static void map_2m(uint64_t virt, uint64_t phys)
{
    uint64_t* pdpt = pt_child(&pml4[(virt >> 39) & 0x1FF]);
    uint64_t* pd   = pt_child(&pdpt[(virt >> 30) & 0x1FF]);
    pd[(virt >> 21) & 0x1FF] = phys | 0x3 | (1ULL << 7); // PS
}

static void map_range(uint64_t virt, uint64_t phys, uint64_t size)
{
    while (size != 0)
    {
        uint64_t step;
        if (((virt | phys) & 0x1FFFFFULL) == 0 && size >= 0x200000ULL)
        {
            map_2m(virt, phys);
            step = 0x200000ULL;
        }
        else
        {
            uint64_t advance = 0x1000ULL - (virt & 0xFFFULL);
            if (advance > size)
            {
                advance = size;
            }
            map_4k(virt & ~0xFFFULL, phys & ~0xFFFULL);
            step = advance;
        }
        virt += step;
        phys += step;
        size -= step;
    }
}


// ---------------------------------------------------------------------------
// Limine responses for the 64-bit kernel
// ---------------------------------------------------------------------------

template <typename T>
static inline uint64_t to_hhdm(T ptr)
{
    return HHDM_BASE + (uint64_t)(uint32_t)ptr;
}

static inline uint32_t get_bsp_lapic_id()
{
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    return (ebx >> 24) & 0xFF;
}

static limine_bootloader_info_response resp_bootinfo;
static char bootinfo_name[] = "NasuaOS Boot Manager";
static char bootinfo_version[] = "1.0";

static limine_framebuffer_response resp_fb;
static limine_framebuffer fb;
static limine_video_mode fb_mode;
static uint64_t fb_list[1];
static uint64_t fb_mode_list[1];

static limine_module_response resp_modules;
static limine_file files[MAX_MODULES];
static uint64_t file_ptrs[MAX_MODULES];
static char module_paths[MAX_MODULES][64];

static limine_memmap_response resp_memmap;
static limine_memmap_entry mem_entries[256];
static uint64_t mem_entry_ptrs[256];

static limine_hhdm_response resp_hhdm;
static limine_rsdp_response resp_rsdp;
static limine_mp_response resp_mp;
static limine_mp_info mp_cpu;
static uint64_t mp_cpu_ptrs[1];


// Memory map collected from the multiboot2 info structure.
static limine_memmap_entry mb2_memmap[64];
static uint32_t mb2_memmap_count = 0;

static void collect_memmap(uint32_t mb_info)
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

        if (tag->type == MB2_TAG_MMAP && mb2_memmap_count < 64)
        {
            // Multiboot2 mmap tag: type, size, entry_size, entry_version,
            // then entries. Limine stores its own internal type values.
            uint32_t entry_size = *(uint32_t*)(current + 8);
            if (entry_size < 24)
            {
                current += (tag->size + 7) & ~7;
                continue;
            }
            uint32_t entry_count = (tag->size - 16) / entry_size;
            uint8_t* src = current + 16;

            for (uint32_t i = 0; i < entry_count && mb2_memmap_count < 64; i++, src += entry_size)
            {
                uint64_t ltype;
                switch (*(uint32_t*)(src + 16))
                {
                    case 1:      ltype = LIMINE_MEMMAP_USABLE; break;
                    case 2:      ltype = LIMINE_MEMMAP_RESERVED; break;
                    case 3:      ltype = 2; break;  // ACPI reclaimable
                    case 4:      ltype = 3; break;  // ACPI NVS
                    case 5:      ltype = 4; break;  // bad memory
                    case 0x1000: ltype = 5; break;  // bootloader reclaimable
                    case 0x1001: ltype = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES; break;
                    case 0x1002: ltype = 7; break;  // framebuffer
                    case 0x1003: ltype = 8; break;  // reserved, mapped
                    case 0x2000: ltype = 5; break;  // EFI reclaimable
                    case 0:      ltype = LIMINE_MEMMAP_RESERVED; break;
                    default:     ltype = LIMINE_MEMMAP_RESERVED; break;
                }

                mb2_memmap[mb2_memmap_count].base   = *(uint64_t*)src;
                mb2_memmap[mb2_memmap_count].length = *(uint64_t*)(src + 8);
                mb2_memmap[mb2_memmap_count].type   = ltype;
                mb2_memmap_count++;
            }
        }

        current += (tag->size + 7) & ~7;
    }
}


struct protected_range
{
    uint64_t start;
    uint64_t end;
};

static protected_range protected_ranges[24];
static uint32_t protected_count = 0;

static void add_protected(uint64_t start, uint64_t end)
{
    if (end <= start || protected_count >= 24)
    {
        return;
    }
    protected_ranges[protected_count].start = start;
    protected_ranges[protected_count].end   = end;
    protected_count++;
}


// Build the Limine memory map response: copy the Multiboot2 map, then carve
// out the physical ranges occupied by the kernels we loaded ourselves so the
// kernel's PMM does not hand them out.
static void build_memmap_response(uint64_t cover_end)
{
    uint32_t out = 0;

    // Sort the (very few) protected ranges by start address.
    for (uint32_t i = 1; i < protected_count; i++)
    {
        protected_range key = protected_ranges[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && protected_ranges[j].start > key.start)
        {
            protected_ranges[j + 1] = protected_ranges[j];
            j--;
        }
        protected_ranges[j + 1] = key;
    }

    for (uint32_t i = 0; i < mb2_memmap_count && out < 250; i++)
    {
        limine_memmap_entry e = mb2_memmap[i];

        if (e.type != LIMINE_MEMMAP_USABLE)
        {
            mem_entries[out++] = e;
            continue;
        }

        uint64_t cur = e.base;
        uint64_t entry_end = e.base + e.length;

        for (uint32_t p = 0; p < protected_count; p++)
        {
            uint64_t ps = protected_ranges[p].start;
            uint64_t pe = protected_ranges[p].end;

            if (pe <= cur || ps >= entry_end)
            {
                continue;
            }

            if (ps > cur && out < 250)
            {
                mem_entries[out].base   = cur;
                mem_entries[out].length = ps - cur;
                mem_entries[out].type   = LIMINE_MEMMAP_USABLE;
                out++;
            }

            uint64_t os = ps > cur ? ps : cur;
            uint64_t oe = pe < entry_end ? pe : entry_end;
            if (oe > os && out < 250)
            {
                mem_entries[out].base   = os;
                mem_entries[out].length = oe - os;
                mem_entries[out].type   = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES;
                out++;
            }

            if (pe > cur)
            {
                cur = pe;
            }
        }

        if (cur < entry_end && out < 250)
        {
            mem_entries[out].base   = cur;
            mem_entries[out].length = entry_end - cur;
            mem_entries[out].type   = LIMINE_MEMMAP_USABLE;
            out++;
        }
    }

    // Make sure the whole covered range is at least identity/HHDM mapped;
    // entries above 4 GiB that were not in the firmware map stay reserved.
    (void)cover_end;

    for (uint32_t i = 0; i < out; i++)
    {
        mem_entry_ptrs[i] = to_hhdm(&mem_entries[i]);
    }

    resp_memmap.revision = 0;
    resp_memmap.entry_count = out;
    resp_memmap.entries = to_hhdm(mem_entry_ptrs);
}


// Find a physical base for the 64-bit kernel image that does not overlap
// any Limine module buffer or this binary itself.
static uint64_t find_kernel64_base(uint64_t span)
{
    if (span == 0 || span > 0x10000000ULL)
    {
        return 0;
    }

    // Occupied intervals: every module buffer plus our own image.
    struct interval { uint64_t s, e; };
    interval occupied[24];
    uint32_t n = 0;
    for (uint32_t i = 0; i < module_count && n < 23; i++)
    {
        occupied[n].s = modules[i].start;
        occupied[n].e = modules[i].end;
        n++;
    }
    occupied[n].s = BOOTMGR_PHYS_BASE;
    occupied[n].e = BOOTMGR_PHYS_BASE + BOOTMGR_PHYS_SPAN;
    n++;

    // Sort occupied intervals by start address.
    for (uint32_t i = 1; i < n; i++)
    {
        interval key = occupied[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && occupied[j].s > key.s)
        {
            occupied[j + 1] = occupied[j];
            j--;
        }
        occupied[j + 1] = key;
    }

    // Scan usable memory map entries for a hole big enough.
    for (uint32_t i = 0; i < mb2_memmap_count; i++)
    {
        if (mb2_memmap[i].type != LIMINE_MEMMAP_USABLE)
        {
            continue;
        }
        uint64_t cur = mb2_memmap[i].base;
        const uint64_t end = cur + mb2_memmap[i].length;

        for (uint32_t k = 0; k < n; k++)
        {
            uint64_t os = occupied[k].s > cur ? occupied[k].s : cur;
            uint64_t oe = occupied[k].e < end ? occupied[k].e : end;
            if (oe <= os)
            {
                continue;
            }
            // Free gap [cur, os) before this occupied interval.
            if (os - cur >= span)
            {
                uint64_t base = (cur + 0x1FFFFF) & ~0x1FFFFFULL;
                if (base + span <= os)
                {
                    return base;
                }
            }
            if (oe > cur)
            {
                cur = oe;
            }
        }
        if (end - cur >= span)
        {
            uint64_t base = (cur + 0x1FFFFF) & ~0x1FFFFFULL;
            if (base + span <= end)
            {
                return base;
            }
        }
    }
    return 0;
}


// Find the RSDP from Multiboot2 tags or the BIOS ROM area (0xE0000 - 0xFFFFF).
static void* find_rsdp(uint32_t mb_info)
{
    uint32_t total_size = *(uint32_t*)mb_info;
    uint8_t* current = (uint8_t*)mb_info + 8;
    uint8_t* end = (uint8_t*)mb_info + total_size;

    void* old_rsdp = nullptr;

    while (current < end)
    {
        mb2_tag* tag = (mb2_tag*)current;
        if (tag->type == MB2_TAG_END)
        {
            break;
        }
        if (tag->type == MB2_TAG_ACPI_NEW)
        {
            return (void*)(current + 8);
        }
        if (tag->type == MB2_TAG_ACPI_OLD && old_rsdp == nullptr)
        {
            old_rsdp = (void*)(current + 8);
        }
        current += (tag->size + 7) & ~7;
    }

    if (old_rsdp != nullptr)
    {
        return old_rsdp;
    }

    for (uint32_t addr = 0x000E0000; addr < 0x00100000; addr += 16)
    {
        const char* p = (const char*)addr;
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ')
        {
            return (void*)addr;
        }
    }
    return nullptr;
}


// ---------------------------------------------------------------------------
// Request handling for the 64-bit kernel
// ---------------------------------------------------------------------------

struct kernel_request
{
    uint64_t* ptr; // points at the request id inside the loaded kernel image
};

static kernel_request requests[32];
static uint32_t request_count = 0;

static void scan_kernel_requests(uint64_t image_base, uint64_t image_size)
{
    request_count = 0;

    bool in_requests = false;
    bool base_rev_done = false;

    for (uint64_t off = 0; off + 8 <= image_size; off += 8)
    {
        uint64_t* p = (uint64_t*)(image_base + off);

        // Base revision tag: {magic0, magic1, requested_revision}
        if (!base_rev_done && off + 24 <= image_size &&
            p[0] == LIMINE_BASE_REV_TAG_0 && p[1] == LIMINE_BASE_REV_TAG_1)
        {
            uint64_t requested = p[2];
            if (requested <= LIMINE_SUPPORTED_REV)
            {
                // Supported: store the negotiated revision in [1] and
                // clear [2] to signal support, like Limine does.
                p[1] = requested;
                p[2] = 0;
            }
            base_rev_done = true;
            continue;
        }

        if (off + 32 <= image_size &&
            p[0] == LIMINE_REQ_START_0 && p[1] == LIMINE_REQ_START_1 &&
            p[2] == LIMINE_REQ_START_2 && p[3] == LIMINE_REQ_START_3)
        {
            in_requests = true;
            continue;
        }

        if (off + 16 <= image_size &&
            p[0] == LIMINE_REQ_END_0 && p[1] == LIMINE_REQ_END_1)
        {
            break;
        }

        if (in_requests && off + 48 <= image_size &&
            p[0] == LIMINE_COMMON_MAGIC_0 && p[1] == LIMINE_COMMON_MAGIC_1)
        {
            if (request_count < 32)
            {
                requests[request_count].ptr = p;
                request_count++;
            }
        }
    }
}


static void fill_requests()
{
    for (uint32_t i = 0; i < request_count; i++)
    {
        uint64_t* req = requests[i].ptr;
        uint64_t id2 = req[2];
        uint64_t id3 = req[3];

        // The response pointer sits at offset 40 (index 5) in every
        // Limine request structure. Hand back higher-half direct map (HHDM)
        // pointers so the kernel can access responses across any CR3 context.
        if (id2 == LIMINE_ID_FRAMEBUFFER_2 && id3 == LIMINE_ID_FRAMEBUFFER_3)
        {
            req[5] = to_hhdm(&resp_fb);
        }
        else if (id2 == LIMINE_ID_MODULE_2 && id3 == LIMINE_ID_MODULE_3)
        {
            req[5] = to_hhdm(&resp_modules);
        }
        else if (id2 == LIMINE_ID_MEMMAP_2 && id3 == LIMINE_ID_MEMMAP_3)
        {
            req[5] = to_hhdm(&resp_memmap);
        }
        else if (id2 == LIMINE_ID_HHDM_2 && id3 == LIMINE_ID_HHDM_3)
        {
            req[5] = to_hhdm(&resp_hhdm);
        }
        else if (id2 == LIMINE_ID_RSDP_2 && id3 == LIMINE_ID_RSDP_3)
        {
            req[5] = to_hhdm(&resp_rsdp);
        }
        else if (id2 == LIMINE_ID_MP_2 && id3 == LIMINE_ID_MP_3)
        {
            req[5] = to_hhdm(&resp_mp);
        }
        else if (id2 == LIMINE_ID_BOOTLOADER_INFO_2 && id3 == LIMINE_ID_BOOTLOADER_INFO_3)
        {
            req[5] = to_hhdm(&resp_bootinfo);
        }
    }
}


static void prepare_responses(uint64_t fb_phys_addr, uint32_t mb_info)
{
    resp_bootinfo.revision = 0;
    resp_bootinfo.name = to_hhdm(bootinfo_name);
    resp_bootinfo.version = to_hhdm(bootinfo_version);

    fb.address = HHDM_BASE + (uint64_t)(uint32_t)fb_phys_addr;
    fb.width = fb_width;
    fb.height = fb_height;
    fb.pitch = fb_pitch;
    fb.bpp = 32;
    fb.memory_model = 1; // RGB
    fb.red_mask_size = 8;   fb.red_mask_shift = 16;
    fb.green_mask_size = 8; fb.green_mask_shift = 8;
    fb.blue_mask_size = 8;  fb.blue_mask_shift = 0;
    fb.edid_size = 0;
    fb.edid = 0;
    fb.mode_count = 1;
    fb.modes = to_hhdm(fb_mode_list);

    fb_mode.pitch = fb_pitch;
    fb_mode.width = fb_width;
    fb_mode.height = fb_height;
    fb_mode.bpp = 32;
    fb_mode.memory_model = 1;
    fb_mode.red_mask_size = 8;   fb_mode.red_mask_shift = 16;
    fb_mode.green_mask_size = 8; fb_mode.green_mask_shift = 8;
    fb_mode.blue_mask_size = 8;  fb_mode.blue_mask_shift = 0;
    fb_mode_list[0] = to_hhdm(&fb_mode);

    fb_list[0] = to_hhdm(&fb);
    resp_fb.revision = 1;
    resp_fb.framebuffer_count = 1;
    resp_fb.framebuffers = to_hhdm(fb_list);

    resp_hhdm.revision = 0;
    resp_hhdm.offset = HHDM_BASE;

    void* rsdp = find_rsdp(mb_info);
    resp_rsdp.revision = 0;
    resp_rsdp.address = rsdp ? to_hhdm(rsdp) : 0;

    uint32_t bsp_id = get_bsp_lapic_id();
    resp_mp.revision = 0;
    resp_mp.flags = 0;
    resp_mp.bsp_lapic_id = bsp_id;
    resp_mp.cpu_count = 1;
    mp_cpu.processor_id = 0;
    mp_cpu.lapic_id = bsp_id;
    mp_cpu.reserved = 0;
    mp_cpu.goto_address = 0;
    mp_cpu.extra_argument = 0;
    mp_cpu_ptrs[0] = to_hhdm(&mp_cpu);
    resp_mp.cpus = to_hhdm(mp_cpu_ptrs);
}


static void prepare_module_response()
{
    uint64_t count = 0;

    for (uint32_t i = 0; i < module_count && count < MAX_MODULES; i++)
    {
        // Skip the kernel images themselves; only payload modules are
        // passed on to the kernel.
        const char* name = modules[i].name;
        if (name[0] != '\0' &&
            (kcontains(name, "kernel_32bit") || kcontains(name, "kernel_64bit")))
        {
            continue;
        }

        const char* path = name;
        if (path[0] == '\0')
        {
            // No module_string configured; synthesize a generic path.
            char* buf = module_paths[count];
            buf[0] = '/';
            buf[1] = 'm';
            buf[2] = 'o';
            buf[3] = 'd';
            buf[4] = '_';
            buf[5] = '0' + (char)(((uint32_t)count) % 10);
            buf[6] = '\0';
            path = buf;
        }
        else if (kstrlen(path) >= 64)
        {
            path = module_paths[count];
            kcopy((void*)path, name, 63);
            ((char*)path)[63] = '\0';
        }

        files[count].revision = 0;
        files[count].address = HHDM_BASE + (uint64_t)modules[i].start;
        files[count].size = modules[i].end - modules[i].start;
        files[count].path = to_hhdm(path);
        files[count].string = to_hhdm(name);
        files[count].media_type = 0;
        files[count].unused = 0;
        files[count].tftp_ip = 0;
        files[count].tftp_port = 0;
        files[count].partition_index = 0;
        files[count].mbr_disk_id = 0;
        kfill(&files[count].gpt_disk_uuid, 0, sizeof(limine_uuid));
        kfill(&files[count].gpt_part_uuid, 0, sizeof(limine_uuid));
        kfill(&files[count].part_uuid, 0, sizeof(limine_uuid));

        file_ptrs[count] = to_hhdm(&files[count]);
        count++;
    }

    resp_modules.revision = 0;
    resp_modules.module_count = count;
    resp_modules.modules = to_hhdm(file_ptrs);
}


// ---------------------------------------------------------------------------
// GDT for the long mode transition (Limine layout: 7 descriptors)
// ---------------------------------------------------------------------------

static uint32_t gdt[14]; // 7 descriptors * 2 dwords
static uint8_t  gdt_desc[6]; // lgdt pseudo-descriptor {limit:16, base:32}

static void make_descriptor(uint32_t index, uint32_t limit_low, uint8_t access,
                            uint8_t flags, uint32_t limit_hi)
{
    // All descriptors have base = 0.
    gdt[index * 2]     = limit_low;                    // limit[15:0] | base[15:0]<<16
    gdt[index * 2 + 1] = (uint32_t)access << 8 |
                         (uint32_t)(flags << 4 | (limit_hi & 0xF)) << 16;
}

static void setup_gdt()
{
    kfill(gdt, 0, sizeof(gdt));

    // 0: null
    // 1: 16-bit code, base 0, limit 0xffff, readable
    make_descriptor(1, 0xFFFF, 0x9A, 0x0, 0xF);
    // 2: 16-bit data, base 0, limit 0xffff, writable
    make_descriptor(2, 0xFFFF, 0x92, 0x0, 0xF);
    // 3: 32-bit code, base 0, limit 0xffffffff, readable
    make_descriptor(3, 0xFFFF, 0x9A, 0xC, 0xF);
    // 4: 32-bit data, base 0, limit 0xffffffff, writable
    make_descriptor(4, 0xFFFF, 0x92, 0xC, 0xF);
    // 5: 64-bit code (L=1), base 0
    make_descriptor(5, 0xFFFF, 0x9A, 0xA, 0xF);
    // 6: 64-bit data, base 0
    make_descriptor(6, 0xFFFF, 0x92, 0xC, 0xF);

    uint32_t base = (uint32_t)gdt;
    gdt_desc[0] = (uint8_t)(sizeof(gdt) - 1);
    gdt_desc[1] = (uint8_t)((sizeof(gdt) - 1) >> 8);
    gdt_desc[2] = (uint8_t)base;
    gdt_desc[3] = (uint8_t)(base >> 8);
    gdt_desc[4] = (uint8_t)(base >> 16);
    gdt_desc[5] = (uint8_t)(base >> 24);
}


// ---------------------------------------------------------------------------
// 32-bit boot path: build a fresh Multiboot2 info and jump to the kernel
// ---------------------------------------------------------------------------

static uint8_t mb2_buf[16384] __attribute__((aligned(8)));

static void add_tag(uint8_t** cursor, uint32_t type, uint32_t size, const void* data)
{
    mb2_tag* tag = (mb2_tag*)*cursor;
    tag->type = type;
    tag->size = size;
    if (data && size > 8)
    {
        kcopy(*cursor + 8, data, size - 8);
    }
    *cursor += (size + 7) & ~7;
}


static void boot_kernel32(uint32_t mb_info, int kernel_module)
{
    uint32_t file_addr = modules[kernel_module].start;
    uint32_t file_size = modules[kernel_module].end - modules[kernel_module].start;

    uint32_t kmin = 0, kmax = 0;
    uint32_t entry = load_elf32(file_addr, file_size, &kmin, &kmax);

    if (entry == 0)
    {
        clear_screen(COLOR_BLACK);
        cursor_x = 20;
        cursor_y = 40;
        print("ERROR: failed to load 32-bit kernel ELF\n", COLOR_RED);
        for (;;)
        {
            asm volatile("hlt");
        }
    }

    add_protected(kmin, kmax);

    // Build the Multiboot2 information structure for the 32-bit kernel.
    uint8_t* cursor = mb2_buf + 8;

    // Framebuffer tag: reuse the framebuffer Limine gave us.
    {
        mb2_tag_framebuffer fbt;
        fbt.type = MB2_TAG_FRAMEBUFFER;
        fbt.size = sizeof(mb2_tag_framebuffer);
        fbt.framebuffer_addr = (uint64_t)(uint32_t)framebuffer;
        fbt.framebuffer_pitch = fb_pitch;
        fbt.framebuffer_width = fb_width;
        fbt.framebuffer_height = fb_height;
        fbt.framebuffer_bpp = 32;
        fbt.framebuffer_type = 1;
        fbt.reserved = 0;
        // add_tag() expects a pointer to the tag payload (past type+size).
        add_tag(&cursor, MB2_TAG_FRAMEBUFFER, sizeof(mb2_tag_framebuffer),
                (uint8_t*)&fbt + 8);
    }

    // Forward ACPI RSDP tags and the memory map verbatim.
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

            if (tag->type == MB2_TAG_ACPI_OLD || tag->type == MB2_TAG_ACPI_NEW ||
                tag->type == MB2_TAG_MMAP)
            {
                add_tag(&cursor, tag->type, tag->size, current + 8);
            }

            current += (tag->size + 7) & ~7;
        }
    }

    // Module tags for the payload modules.
    for (uint32_t i = 0; i < module_count; i++)
    {
        const char* name = modules[i].name;
        if (name[0] != '\0' &&
            (kcontains(name, "kernel_32bit") || kcontains(name, "kernel_64bit")))
        {
            continue;
        }

        const char* cmdline = name[0] != '\0' ? name : "";
        uint32_t cmdlen = kstrlen(cmdline) + 1;
        uint32_t size = 16 + cmdlen;
        uint32_t aligned = (size + 7) & ~7;

        mb2_tag_module* m = (mb2_tag_module*)cursor;
        m->type = MB2_TAG_MODULE;
        m->size = aligned;
        m->mod_start = modules[i].start;
        m->mod_end = modules[i].end;
        kcopy(m->cmdline, cmdline, cmdlen);

        // Zero the alignment padding.
        for (uint32_t j = size; j < aligned; j++)
        {
            cursor[j] = 0;
        }

        cursor += aligned;
    }

    // End tag.
    mb2_tag* end_tag = (mb2_tag*)cursor;
    end_tag->type = MB2_TAG_END;
    end_tag->size = 8;
    cursor += 8;

    // Fill in the Multiboot2 header.
    *(uint32_t*)mb2_buf = (uint32_t)(cursor - mb2_buf);
    *(uint32_t*)(mb2_buf + 4) = 0;

    clear_screen(COLOR_BLACK);
    cursor_x = 20;
    cursor_y = 40;
    print("Loading 32-bit kernel...\n", COLOR_GREEN);

    uint32_t info_addr = (uint32_t)mb2_buf;

    asm volatile(
        "cli\n\t"
        "movl %0, %%ebx\n\t"
        "movl %1, %%eax\n\t"
        "jmp *%%eax\n\t"
        :: "r"(info_addr), "r"(entry)
        : "eax", "ebx", "memory");

    for (;;)
    {
        asm volatile("hlt");
    }
}


// ---------------------------------------------------------------------------
// 64-bit boot path
// ---------------------------------------------------------------------------

extern "C" void boot_kernel64(uint32_t pml4, uint32_t gdtdesc,
                              uint32_t entry_lo, uint32_t entry_hi,
                              uint32_t stack_lo, uint32_t stack_hi);

static uint8_t k64_stack[256 * 1024] __attribute__((aligned(16)));

static void boot_kernel64_path(uint32_t mb_info, int kernel_module)
{
    uint32_t file_addr = modules[kernel_module].start;
    uint32_t file_size = modules[kernel_module].end - modules[kernel_module].start;

    serial_puts("[boot_mgr] entering 64-bit path\n");

    // Collect the memory map first: it drives both the kernel image
    // placement and the response we hand to the kernel.
    collect_memmap(mb_info);

    uint64_t span_min = 0, span_max = 0;
    uint64_t phys_base = 0;
    if (elf64_image_span(file_addr, file_size, &span_min, &span_max))
    {
        uint64_t span = span_max - span_min;
        phys_base = find_kernel64_base(span);
        serial_puts("[boot_mgr] k64 base=");
        print_hex64_dbg(phys_base);
        serial_putc('\n');
    }

    elf64_load_info info;
    if (phys_base == 0 || !load_elf64(file_addr, file_size, phys_base, &info))
    {
        clear_screen(COLOR_BLACK);
        cursor_x = 20;
        cursor_y = 40;
        print("ERROR: failed to load 64-bit kernel ELF\n", COLOR_RED);
        for (;;)
        {
            asm volatile("hlt");
        }
    }

    serial_puts("[boot_mgr] kernel64 loaded, entry=");
    print_hex64_dbg(info.entry);
    serial_putc('\n');

    add_protected(info.min_phys, info.max_phys);

    // Under the multiboot2 protocol Limine leaves the kernel image and the
    // module buffers marked usable, so carve them out of the memory map we
    // report: the kernel's PMM must never hand them out.
    add_protected(BOOTMGR_PHYS_BASE, BOOTMGR_PHYS_BASE + BOOTMGR_PHYS_SPAN);
    for (uint32_t i = 0; i < module_count; i++)
    {
        add_protected(modules[i].start, modules[i].end);
    }

    // Compute how much low memory must be mapped (identity + HHDM).
    uint64_t cover = info.max_phys;
    for (uint32_t i = 0; i < mb2_memmap_count; i++)
    {
        uint64_t end = mb2_memmap[i].base + mb2_memmap[i].length;
        if (end > cover)
        {
            cover = end;
        }
    }
    for (uint32_t i = 0; i < module_count; i++)
    {
        if (modules[i].end > cover)
        {
            cover = modules[i].end;
        }
    }
    uint64_t fb_end = (uint64_t)(uint32_t)framebuffer + (uint64_t)fb_pitch * fb_height;
    if (fb_end > cover)
    {
        cover = fb_end;
    }
    cover = (cover + 0x1FFFFFULL) & ~0x1FFFFFULL;
    if (cover > (512ULL << 30))
    {
        cover = 512ULL << 30;
    }
    // Always map the whole first 4 GiB: the kernel reaches firmware/MMIO
    // regions (LAPIC at 0xFEE00000, IOAPIC, etc.) through the HHDM.
    if (cover < 0x100000000ULL)
    {
        cover = 0x100000000ULL;
    }

    // Build page tables: identity map, HHDM and the kernel image.
    pml4 = pt_alloc();
    if (pml4 == nullptr)
    {
        clear_screen(COLOR_BLACK);
        cursor_x = 20;
        cursor_y = 40;
        print("ERROR: out of page table memory\n", COLOR_RED);
        for (;;)
        {
            asm volatile("hlt");
        }
    }

    map_range(0, 0, cover);
    map_range(HHDM_BASE, 0, cover);

    uint64_t kv = info.min_vaddr & ~0xFFFULL;
    uint64_t kp = info.min_phys  & ~0xFFFULL;
    uint64_t kend = (info.max_vaddr + 0xFFFULL) & ~0xFFFULL;
    map_range(kv, kp, kend - kv);

    build_memmap_response(cover);

    // Scan the loaded kernel image for Limine requests and the base
    // revision tag, then fill in the responses.
    uint64_t scan_base = info.min_phys & ~0xFFFULL;
    uint64_t scan_size = (info.max_phys - scan_base + 0xFFFULL) & ~0xFFFULL;
    // Only scan the file-backed portion to keep it fast; requests live in
    // the first loadable segments anyway.
    scan_kernel_requests(scan_base, scan_size);
    prepare_module_response();
    prepare_responses((uint64_t)(uint32_t)framebuffer, mb_info);
    fill_requests();

    setup_gdt();

    clear_screen(COLOR_BLACK);
    cursor_x = 20;
    cursor_y = 40;
    print("Loading 64-bit kernel...\n", COLOR_GREEN);

    uint64_t cr3 = (uint64_t)pml4;
    uint64_t entry = info.entry;

    // Give the kernel an HHDM stack: the kernel's VMM only copies the upper
    // half of the page tables into user address spaces, so a low physical
    // stack would become unmapped when it switches CR3.
    uint64_t stack = HHDM_BASE + (uint64_t)(uint32_t)(k64_stack + sizeof(k64_stack));

    boot_kernel64((uint32_t)cr3, (uint32_t)gdt_desc,
                  (uint32_t)entry, (uint32_t)(entry >> 32),
                  (uint32_t)stack, (uint32_t)(stack >> 32));

    for (;;)
    {
        asm volatile("hlt");
    }
}


// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

extern "C" void kmain(uint32_t magic, uint32_t mb_info)
{
    if (magic != MULTIBOOT2_MAGIC)
    {
        for (;;)
        {
            asm volatile("hlt");
        }
    }


    find_framebuffer(mb_info);

    if (!find_modules(mb_info))
    {
        print("ERROR: kernel modules not found\n", COLOR_RED);

        for (;;)
        {
            asm volatile("hlt");
        }
    }


    serial_puts("[boot_mgr] modules found\n");

    int k32 = find_module_by_name("kernel_32bit");
    int k64 = find_module_by_name("kernel_64bit");
    serial_puts("[boot_mgr] k32=");
    serial_putc('0' + (char)(k32 < 0 ? 9 : k32));
    serial_puts(" k64=");
    serial_putc('0' + (char)(k64 < 0 ? 9 : k64));
    serial_putc('\n');

    if (k32 < 0 || k64 < 0)
    {
        print("ERROR: kernel modules not found\n", COLOR_RED);
        print("Expected module_string 'kernel_32bit' and 'kernel_64bit'\n", COLOR_WHITE);

        for (;;)
        {
            asm volatile("hlt");
        }
    }


    // No menu: pick the best kernel for this CPU automatically.
    const int target = cpu_has_long_mode() ? 1 : 0;
    serial_puts("[boot_mgr] target=");
    serial_putc('0' + (char)target);
    serial_putc('\n');
    if (target == 0)
    {
        boot_kernel32(mb_info, k32);
    }

    boot_kernel64_path(mb_info, k64);
}
