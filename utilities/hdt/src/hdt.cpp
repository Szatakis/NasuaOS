#include <stdint.h>

#include "font8x8.h"


// MULTIBOOT2
#define MULTIBOOT2_MAGIC 0x36D76289

#define MB2_TAG_END         0
#define MB2_TAG_CMDLINE     1
#define MB2_TAG_BOOTLOADER  2
#define MB2_TAG_MODULE      3
#define MB2_TAG_MMAP        6
#define MB2_TAG_FRAMEBUFFER 8


struct mb2_tag
{
    uint32_t type;
    uint32_t size;
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


struct mb2_tag_mmap
{
    uint32_t type;
    uint32_t size;

    uint32_t entry_size;
    uint32_t entry_version;
};


struct mb2_mmap_entry
{
    uint64_t addr;
    uint64_t len;

    uint32_t type;
    uint32_t reserved;
};


// FRAMEBUFFER
static uint8_t* framebuffer = nullptr;

static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;


// COLORS
static const uint32_t COLOR_BLACK   = 0x00101820;
static const uint32_t COLOR_WHITE   = 0x00FFFFFF;
static const uint32_t COLOR_GREEN   = 0x0044FF88;
static const uint32_t COLOR_BLUE    = 0x004080FF;
static const uint32_t COLOR_YELLOW  = 0x00FFD34D;
static const uint32_t COLOR_RED     = 0x00FF5555;
static const uint32_t COLOR_GRAY    = 0x00808090;


// FRAMEBUFFER PIXEL
static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height)
    {
        return;
    }

    uint32_t* pixel = (uint32_t*)(framebuffer + y * fb_pitch + x * 4);

    *pixel = color;
}


// CLEAR
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


// DRAW CHARACTER
static void draw_char(char c, uint32_t x, uint32_t y, uint32_t color, uint32_t scale)
{
    uint8_t ch = (uint8_t)c;

    for (uint32_t row = 0; row < 8; row++)
    {
        uint8_t bits = font8x8[ch][row];

        for (uint32_t col = 0; col < 8; col++)
        {
            if (bits & (1 << (7 - col)))
            {
                for (uint32_t sy = 0; sy < scale; sy++)
                {
                    for (uint32_t sx = 0; sx < scale; sx++)
                    {
                        put_pixel(
                            x + col * scale + sx,
                            y + row * scale + sy,
                            color
                        );
                    }
                }
            }
        }
    }
}


// PRINT
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

        draw_char(c, cursor_x, cursor_y, color, 1);

        cursor_x += 8;

        if (cursor_x + 8 >= fb_width)
        {
            cursor_x = 0;
            cursor_y += 12;
        }
    }
}


// PRINT HEX
static void print_hex32(uint32_t value)
{
    const char* hex = "0123456789ABCDEF";

    char buffer[11];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; i++)
    {
        buffer[2 + i] = hex[(value >> ((7 - i) * 4)) & 0xF];
    }

    buffer[10] = 0;

    print(buffer, COLOR_WHITE);
}


// PRINT DECIMAL
static void print_dec(uint32_t value)
{
    char buffer[12];

    int i = 0;

    if (value == 0)
    {
        print("0", COLOR_WHITE);
        return;
    }

    while (value)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i)
    {
        char c[2];

        c[0] = buffer[--i];
        c[1] = 0;

        print(c, COLOR_WHITE);
    }
}


// PRINT YES / NO
static void print_bool(bool value)
{
    if (value)
    {
        print("YES", COLOR_GREEN);
    }
    else
    {
        print("NO", COLOR_RED);
    }
}


// CPUID
static bool cpuid_supported()
{
    uint32_t before;
    uint32_t after;

    asm volatile("pushfl\n" "popl %0\n" : "=r"(before));

    uint32_t modified = before ^ (1 << 21);

    asm volatile("pushl %0\n" "popfl\n" : : "r"(modified));

    asm volatile("pushfl\n" "popl %0\n" : "=r"(after));

    return ((before ^ after) & (1 << 21)) != 0;
}


// CPU VENDOR
static void cpu_vendor()
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    char vendor[13];

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

    *(uint32_t*)&vendor[0] = ebx;
    *(uint32_t*)&vendor[4] = edx;
    *(uint32_t*)&vendor[8] = ecx;

    vendor[12] = 0;

    print("Vendor: ", COLOR_GRAY);
    print(vendor, COLOR_WHITE);
    print("\n", COLOR_WHITE);
}


// CPU FEATURES
static void cpu_features()
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));


    print("FPU: ", COLOR_GRAY);
    print_bool(edx & (1 << 0));
    print("\n", COLOR_WHITE);

    print("APIC: ", COLOR_GRAY);
    print_bool(edx & (1 << 9));
    print("\n", COLOR_WHITE);

    print("SSE: ", COLOR_GRAY);
    print_bool(edx & (1 << 25));
    print("\n", COLOR_WHITE);

    print("SSE2: ", COLOR_GRAY);
    print_bool(edx & (1 << 26));
    print("\n", COLOR_WHITE);

    print("SSE3: ", COLOR_GRAY);
    print_bool(ecx & (1 << 0));
    print("\n", COLOR_WHITE);

    print("SSSE3: ", COLOR_GRAY);
    print_bool(ecx & (1 << 9));
    print("\n", COLOR_WHITE);

    print("SSE4.1: ", COLOR_GRAY);
    print_bool(ecx & (1 << 19));
    print("\n", COLOR_WHITE);

    print("SSE4.2: ", COLOR_GRAY);
    print_bool(ecx & (1 << 20));
    print("\n", COLOR_WHITE);

    print("AVX: ", COLOR_GRAY);
    print_bool(ecx & (1 << 28));
    print("\n", COLOR_WHITE);
}


// CPU INFORMATION
static void cpu_info()
{
    print("\n[ CPU ]\n", COLOR_BLUE);

    if (!cpuid_supported())
    {
        print("CPUID: NO\n", COLOR_RED);
        return;
    }

    print("CPUID: YES\n", COLOR_GREEN);

    cpu_vendor();

    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));

    uint32_t family = (eax >> 8) & 0xF;
    uint32_t model = (eax >> 4) & 0xF;
    uint32_t stepping = eax & 0xF;


    print("Family: ", COLOR_GRAY);
    print_dec(family);

    print("\nModel: ", COLOR_GRAY);
    print_dec(model);

    print("\nStepping: ", COLOR_GRAY);
    print_dec(stepping);

    print("\n\n", COLOR_WHITE);

    cpu_features();
}


// LONG MODE
static void detect_long_mode()
{
    print("\nx86-64 support: ", COLOR_GRAY);

    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));

    if (eax < 0x80000001)
    {
        print("NO\n", COLOR_RED);
        return;
    }

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));

    print_bool(edx & (1 << 29));
    print("\n", COLOR_WHITE);
}


// MULTIBOOT2 FRAMEBUFFER
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

            if (fb->framebuffer_type != 1)
            {
                return false;
            }

            framebuffer = (uint8_t*)(uint32_t)fb->framebuffer_addr;
            fb_width =fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch = fb->framebuffer_pitch;
            fb_bpp = fb->framebuffer_bpp;

            if (fb_bpp != 32)
            {
                return false;
            }

            return true;
        }


        current += (tag->size + 7) & ~7;
    }

    return false;
}


// MEMORY MAP
static void memory_info(uint32_t mb_info)
{
    print("\n[ MEMORY ]\n", COLOR_BLUE);

    uint32_t total_size = *(uint32_t*)mb_info;
    uint8_t* current = (uint8_t*)mb_info + 8;
    uint8_t* end = (uint8_t*)mb_info + total_size;


    uint64_t total_ram = 0;

    while (current < end)
    {
        mb2_tag* tag = (mb2_tag*)current;

        if (tag->type == MB2_TAG_END)
        {
            break;
        }


        if (tag->type == MB2_TAG_MMAP)
        {
            mb2_tag_mmap* mmap = (mb2_tag_mmap*)tag;
            uint8_t* entry_ptr = current + 16;
            uint8_t* mmap_end = current + mmap->size;


            while (entry_ptr < mmap_end)
            {
                mb2_mmap_entry* entry = (mb2_mmap_entry*)entry_ptr;

                if (entry->type == 1)
                {
                    total_ram += entry->len;
                }

                entry_ptr += mmap->entry_size;
            }

            break;
        }


        current += (tag->size + 7) & ~7;
    }


    uint32_t megabytes = (uint32_t)(total_ram / (1024 * 1024));

    print("Available RAM: ", COLOR_GRAY);
    print_dec(megabytes);
    print(" MB\n", COLOR_WHITE);
}


// PCI
static uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFC);

    asm volatile("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xCF8));
    uint32_t result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"((uint16_t)0xCFC));

    return result;
}


static void pci_info()
{
    print("\n[ PCI ]\n", COLOR_BLUE);

    uint32_t found = 0;


    for (uint32_t bus = 0; bus < 256; bus++)
    {
        for (uint32_t device = 0; device < 32; device++)
        {
            for (uint32_t function = 0; function < 8; function++)
            {
                uint32_t id = pci_read(bus, device, function, 0);

                if (id == 0xFFFFFFFF)
                {
                    continue;
                }

                uint16_t vendor = id & 0xFFFF;
                uint16_t device_id = id >> 16;

                print("PCI ", COLOR_GRAY);
                print_hex32((bus << 16) | (device << 8) | function);
                print(": ", COLOR_GRAY);
                print_hex32(vendor);
                print(":", COLOR_WHITE);
                print_hex32(device_id);
                print("\n", COLOR_WHITE);

                found++;

                if (found >= 32)
                {
                    return;
                }
            }
        }
    }


    if (!found)
    {
        print("No PCI devices found.\n", COLOR_RED);
    }
}


// FRAMEBUFFER INFORMATION
static void framebuffer_info()
{
    print("\n[ FRAMEBUFFER ]\n", COLOR_BLUE);

    print("Resolution: ", COLOR_GRAY);
    print_dec(fb_width);
    print("x", COLOR_WHITE);
    print_dec(fb_height);

    print("\nBPP: ", COLOR_GRAY);
    print_dec(fb_bpp);

    print("\nPitch: ", COLOR_GRAY);
    print_dec(fb_pitch);

    print("\nAddress: ", COLOR_GRAY);
    print_hex32((uint32_t)framebuffer);

    print("\n", COLOR_WHITE);
}


// MAIN
extern "C" void kmain(uint32_t magic, uint32_t mb_info)
{
    if (magic != MULTIBOOT2_MAGIC)
    {
        return;
    }

    // First find framebuffer.
    if (!find_framebuffer(mb_info))
    {
        // No graphical framebuffer.
        //
        // At this point we don't touch VGA memory because
        // Multiboot2 may have booted us without VGA text mode.

        return;
    }

    // Clear screen
    clear_screen(COLOR_BLACK);
    cursor_x = 0;
    cursor_y = 10;


    // Header
    print("NASUAOS HARDWARE DETECTION TOOL\n", COLOR_GREEN);
    print("========================================\n", COLOR_BLUE);

    // Framebuffer
    framebuffer_info();

    // CPU
    cpu_info();
    detect_long_mode();

    // Memory
    memory_info(mb_info);

    // PCI
    pci_info();

    // Finished
    print("\n========================================\n", COLOR_BLUE);
    print("Hardware detection complete.\n", COLOR_GREEN);


    for (;;)
    {
        asm volatile("hlt");
    }
}