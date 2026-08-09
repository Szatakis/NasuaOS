#include <stdint.h>

#include "font8x8.h"


// Multiboot2
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define MULTIBOOT_TAG_TYPE_END         0
#define MULTIBOOT_TAG_TYPE_CMDLINE     1
#define MULTIBOOT_TAG_TYPE_BOOTLOADER  2
#define MULTIBOOT_TAG_TYPE_MMAP        6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8


// Multiboot2 structures
struct multiboot_tag
{
    uint32_t type;
    uint32_t size;
};


struct multiboot_tag_mmap
{
    uint32_t type;
    uint32_t size;

    uint32_t entry_size;
    uint32_t entry_version;
};


struct multiboot_mmap_entry
{
    uint64_t addr;
    uint64_t len;

    uint32_t type;
    uint32_t zero;
};


struct multiboot_tag_framebuffer
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


// Framebuffer
static uint32_t* framebuffer = nullptr;

static uint32_t framebuffer_pitch = 0;
static uint32_t framebuffer_width = 0;
static uint32_t framebuffer_height = 0;
static uint32_t framebuffer_bpp = 0;


// Text
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static constexpr uint32_t FONT_WIDTH = 8;
static constexpr uint32_t FONT_HEIGHT = 8;
static constexpr uint32_t CHAR_SPACING = 1;


// Colors
static constexpr uint32_t COLOR_BLACK  = 0x00000000;
static constexpr uint32_t COLOR_WHITE  = 0x00FFFFFF;
static constexpr uint32_t COLOR_RED    = 0x00FF0000;
static constexpr uint32_t COLOR_GREEN  = 0x0000FF00;
static constexpr uint32_t COLOR_BLUE   = 0x000000FF;
static constexpr uint32_t COLOR_YELLOW = 0x00FFFF00;
static constexpr uint32_t COLOR_CYAN   = 0x0000FFFF;


// Test counters
static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

// Port I/O
static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}


static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}


static inline void outl(uint16_t port, uint32_t value)
{
    asm volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}


static inline uint32_t inl(uint16_t port)
{
    uint32_t value;

    asm volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}


// CPU
static inline void cpuid(uint32_t leaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx)
{
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf));
}


static inline uint64_t rdtsc()
{
    uint32_t low;
    uint32_t high;

    asm volatile ("rdtsc" : "=a"(low), "=d"(high));

    return (static_cast<uint64_t>(high) << 32) | low;
}


// Text rendering
static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (framebuffer == nullptr)
    {
        return;
    }

    if (x >= framebuffer_width)
    {
        return;
    }

    if (y >= framebuffer_height)
    {
        return;
    }

    uint8_t* base = reinterpret_cast<uint8_t*>(framebuffer);

    uint32_t* pixel = reinterpret_cast<uint32_t*>(base + y * framebuffer_pitch + x * 4);

    *pixel = color;
}


static void clear_screen(uint32_t color)
{
    if (framebuffer == nullptr)
    {
        return;
    }

    for (uint32_t y = 0; y < framebuffer_height; ++y)
    {
        uint8_t* row = reinterpret_cast<uint8_t*>(framebuffer) + y * framebuffer_pitch;
        uint32_t* pixels = reinterpret_cast<uint32_t*>(row);

        for (uint32_t x = 0; x < framebuffer_width; ++x)
        {
            pixels[x] = color;
        }
    }
}


static void draw_char(char character, uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t c = static_cast<uint8_t>(character);

    const uint8_t* glyph = font8x8[c];

    for (uint32_t row = 0; row < 8; ++row)
    {
        uint8_t bits = glyph[row];

        for (uint32_t col = 0; col < 8; ++col)
        {
            if (bits & (0x80 >> col))
            {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}


static void newline()
{
    cursor_x = 0;

    cursor_y += FONT_HEIGHT + 2;

    if (cursor_y + FONT_HEIGHT >= framebuffer_height)
    {
        cursor_y = 0;
    }
}


static void putchar_color(char character, uint32_t color)
{
    if (character == '\n')
    {
        newline();
        return;
    }

    if (character == '\r')
    {
        cursor_x = 0;
        return;
    }

    if (character == '\t')
    {
        cursor_x += 4 * (FONT_WIDTH + CHAR_SPACING);

        return;
    }

    draw_char(character, cursor_x, cursor_y, color);

    cursor_x += FONT_WIDTH + CHAR_SPACING;

    if (cursor_x + FONT_WIDTH >= framebuffer_width)
    {
        newline();
    }
}


static void putchar(char character)
{
    putchar_color(character, COLOR_WHITE);
}


static void print(const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    while (*text)
    {
        putchar(*text);
        ++text;
    }
}


static void print_color(const char* text, uint32_t color)
{
    if (text == nullptr)
    {
        return;
    }

    while (*text)
    {
        putchar_color(*text, color);

        ++text;
    }
}


static void print_hex(uint32_t value)
{
    const char* hex = "0123456789ABCDEF";

    print("0x");

    for (int i = 7; i >= 0; --i)
    {
        uint32_t digit = (value >> (i * 4)) & 0xF;

        putchar(hex[digit]);
    }
}

static void print_dec(uint32_t value)
{
    if (value == 0)
    {
        putchar('0');
        return;
    }

    char buffer[16];

    uint32_t i = 0;

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);

        value /= 10;
    }

    while (i > 0)
    {
        putchar(buffer[--i]);
    }
}


// Test result
static void test_ok(const char* name)
{
    print_color("[ OK ] ", COLOR_GREEN);

    print(name);
    print("\n");

    ++tests_passed;
}


static void test_fail(const char* name)
{
    print_color("[FAIL] ", COLOR_RED);

    print(name);
    print("\n");

    ++tests_failed;
}


// Framebuffer initialization
static bool init_framebuffer(uint32_t mbi_addr)
{
    uint8_t* mbi = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(mbi_addr));
    uint32_t total_size = *reinterpret_cast<uint32_t*>(mbi);
    uint8_t* current = mbi + 8;
    uint8_t* end = mbi + total_size;


    while (current < end)
    {
        multiboot_tag* tag = reinterpret_cast<multiboot_tag*>(current);


        if (tag->type ==MULTIBOOT_TAG_TYPE_END)
        {
            break;
        }


        if (tag->type ==MULTIBOOT_TAG_TYPE_FRAMEBUFFER)
        {
            multiboot_tag_framebuffer* fb = reinterpret_cast<multiboot_tag_framebuffer*>(current);

            if (fb->framebuffer_type != 1)
            {
                return false;
            }

            if (fb->framebuffer_bpp != 32)
            {
                return false;
            }

            if (fb->framebuffer_addr >
                0xFFFFFFFFULL)
            {
                return false;
            }

            framebuffer = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(fb->framebuffer_addr));
            framebuffer_pitch = fb->framebuffer_pitch;
            framebuffer_width = fb->framebuffer_width;
            framebuffer_height = fb->framebuffer_height;
            framebuffer_bpp = fb->framebuffer_bpp;


            return true;
        }


        uint32_t next =
            (tag->size + 7) & ~7;

        current += next;
    }

    return false;
}


// Memory ranges
struct memory_range
{
    uint64_t start;
    uint64_t end;
};

static memory_range protected_ranges[4];
static uint32_t protected_range_count = 0;

static void add_protected_range(uint64_t start, uint64_t end)
{
    if (start >= end)
    {
        return;
    }

    if (protected_range_count >= 4)
    {
        return;
    }

    protected_ranges[protected_range_count].start = start;

    protected_ranges[protected_range_count].end = end;

    ++protected_range_count;
}

// Test a RAM range
static bool test_ram_range(uint32_t start, uint32_t end, uint32_t& error_address)
{
    start += 3;
    start &= ~3U;

    end &= ~3U;


    if (start >= end)
        return true;


    volatile uint32_t* memory = reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(start));
    uint32_t words = (end - start) / 4;


    // Pattern 0xAAAAAAAA
    for (uint32_t i = 0; i < words; ++i)
    {
        memory[i] = 0xAAAAAAAA;
    }


    for (uint32_t i = 0; i < words; ++i)
    {
        if (memory[i] != 0xAAAAAAAA)
        {
            error_address = start + i * 4;

            return false;
        }
    }

    // Pattern 0x55555555
    for (uint32_t i = 0; i < words; ++i)
    {
        memory[i] = 0x55555555;
    }

    for (uint32_t i = 0; i < words; ++i)
    {
        if (memory[i] != 0x55555555)
        {
            error_address = start + i * 4;

            return false;
        }
    }

    // 0xFFFFFFFF
    for (uint32_t i = 0; i < words; ++i)
    {
        memory[i] = 0xFFFFFFFF;
    }


    for (uint32_t i = 0; i < words; ++i)
    {
        if (memory[i] != 0xFFFFFFFF)
        {
            error_address = start + i * 4;

            return false;
        }
    }

    // 0x00000000
    for (uint32_t i = 0; i < words; ++i)
    {
        memory[i] = 0x00000000;
    }


    for (uint32_t i = 0; i < words; ++i)
    {
        if (memory[i] != 0x00000000)
        {
            error_address = start + i * 4;

            return false;
        }
    }

    // Address pattern
    for (uint32_t i = 0; i < words; ++i)
    {
        memory[i] = start + i * 4;
    }


    for (uint32_t i = 0; i < words; ++i)
    {
        uint32_t expected = start + i * 4;

        if (memory[i] != expected)
        {
            error_address = start + i * 4;

            return false;
        }
    }


    return true;
}


// Full RAM test using Multiboot memory map
static bool test_all_ram(uint32_t mbi_addr)
{
    uint8_t* mbi = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(mbi_addr));
    uint32_t total_size = *reinterpret_cast<uint32_t*>(mbi);
    uint8_t* current = mbi + 8;
    uint8_t* end = mbi + total_size;


    bool found_ram = false;

    while (current < end)
    {
        multiboot_tag* tag = reinterpret_cast<multiboot_tag*>(current);


        if (tag->type == MULTIBOOT_TAG_TYPE_END)
        {
            break;
        }


        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            multiboot_tag_mmap* mmap = reinterpret_cast<multiboot_tag_mmap*>(current);
            uint8_t* entry_ptr = current + sizeof(multiboot_tag_mmap);
            uint8_t* mmap_end = current + mmap->size;


            while (entry_ptr < mmap_end)
            {
                multiboot_mmap_entry* entry = reinterpret_cast<multiboot_mmap_entry*>(entry_ptr);

                /*
                 * type 1 = available RAM
                 */

                if (entry->type == 1 && entry->len != 0)
                {
                    uint64_t start = entry->addr;

                    uint64_t end_addr = entry->addr + entry->len;

                    if (start < 0x100000000ULL)
                    {
                        uint64_t limit = end_addr;

                        if (limit > 0x100000000ULL)
                        {
                            limit = 0x100000000ULL;
                        }


                        uint64_t test_start = start;
                        uint64_t test_end = limit;
                        uint64_t pos = test_start;


                        while (pos < test_end)
                        {
                            uint64_t next = test_end;
                            bool skipped = false;


                            for (uint32_t i = 0; i < protected_range_count; ++i)
                            {
                                uint64_t p_start = protected_ranges[i].start;
                                uint64_t p_end = protected_ranges[i].end;

                                if (pos >= p_start && pos < p_end)
                                {
                                    pos = p_end;
                                    skipped = true;
                                    break;
                                }

                                if (p_start > pos && p_start < next)
                                {
                                    next = p_start;
                                }
                            }


                            if (skipped)
                            {
                                continue;
                            }


                            if (next <= pos)
                            {
                                break;
                            }


                            /*
                                32-bit range.
                            */

                            uint32_t a = static_cast<uint32_t>(pos);
                            uint32_t b = static_cast<uint32_t>(next);


                            if (a < b)
                            {
                                uint32_t error = 0;


                                if (!test_ram_range(a, b, error))
                                {
                                    print_color("RAM ERROR at ", COLOR_RED);
                                    print_hex(error);
                                    print("\n");

                                    return false;
                                }

                                found_ram = true;
                            }

                            pos = next;
                        }
                    }
                }


                entry_ptr += mmap->entry_size;
            }
        }


        uint32_t next = (tag->size + 7) & ~7;

        current += next;
    }


    return found_ram;
}


// CPU test
static bool test_cpu()
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;


    cpuid(0, eax, ebx, ecx, edx);


    if (eax == 0)
    {
        return false;
    }


    return true;
}


// CPU information
static void print_cpu_vendor()
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;


    cpuid(0, eax, ebx, ecx, edx);


    char vendor[13];

    vendor[0]  = ebx & 0xFF;
    vendor[1]  = (ebx >> 8) & 0xFF;
    vendor[2]  = (ebx >> 16) & 0xFF;
    vendor[3]  = (ebx >> 24) & 0xFF;

    vendor[4]  = edx & 0xFF;
    vendor[5]  = (edx >> 8) & 0xFF;
    vendor[6]  = (edx >> 16) & 0xFF;
    vendor[7]  = (edx >> 24) & 0xFF;

    vendor[8]  = ecx & 0xFF;
    vendor[9]  = (ecx >> 8) & 0xFF;
    vendor[10] = (ecx >> 16) & 0xFF;
    vendor[11] = (ecx >> 24) & 0xFF;

    vendor[12] = 0;


    print("CPU: ");
    print(vendor);
    print("\n");
}


// RDTSC test
static bool test_rdtsc()
{
    uint64_t a = rdtsc();

    for (volatile uint32_t i = 0; i < 1000;++i)
    {
    }

    uint64_t b = rdtsc();


    return b > a;
}


// PCI test
static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000 | (static_cast<uint32_t>(bus) << 16) | (static_cast<uint32_t>(device) << 11) | (static_cast<uint32_t>(function) << 8) | (offset & 0xFC);

    outl(0xCF8, address);


    return inl(0xCFC);
}


static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t value = pci_read32(bus, device, function, offset);


    if (offset & 2)
    {
        return value >> 16;
    }

    return value & 0xFFFF;
}


static bool test_pci()
{
    uint32_t devices = 0;


    for (uint16_t bus = 0; bus < 256; ++bus)
    {
        for (uint8_t device = 0; device < 32; ++device)
        {
            uint16_t vendor = pci_read16( static_cast<uint8_t>(bus), device, 0, 0);


            if (vendor == 0xFFFF)
            {
                continue;
            }

            ++devices;
        }
    }


    print("PCI devices: ");
    print_dec(devices);
    print("\n");

    return true;
}


// Keyboard controller test
static bool test_keyboard_controller()
{
    uint8_t status = inb(0x64);

    (void)status;

    return true;
}


// CMOS / RTC
static uint8_t cmos_read(uint8_t index)
{
    outb(0x70, index);

    return inb(0x71);
}


static bool test_rtc()
{
    uint8_t status_a = cmos_read(0x0A);

    uint8_t dv = status_a & 0x70;

    return dv != 0x70;
}


// PIT test
static bool test_pit()
{
    outb(0x43, 0x00);

    return true;
}


// Framebuffer test
static bool test_framebuffer()
{
    if (framebuffer == nullptr)
    {
        return false;
    }

    if (framebuffer_width == 0)
    {
        return false;
    }

    if (framebuffer_height == 0)
    {
        return false;
    }

    if (framebuffer_pitch == 0)
    {
        return false;
    }

    if (framebuffer_bpp != 32)
    {
        return false;
    }


    uint32_t old1 = framebuffer[0];
    uint32_t middle = (framebuffer_height / 2) * (framebuffer_pitch / 4) + (framebuffer_width / 2);
    uint32_t old2 = framebuffer[middle];

    framebuffer[0] = 0x00FFFFFF;
    framebuffer[middle] = 0x00FFFFFF;


    bool ok = framebuffer[0] == 0x00FFFFFF && framebuffer[middle] == 0x00FFFFFF;


    framebuffer[0] = old1;
    framebuffer[middle] = old2;

    return ok;
}


// Main
extern "C" void kmain(uint32_t magic, uint32_t mbi_addr)
{
    // Multiboot2 magic
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        while (true)
        {
            asm volatile ("cli");
            asm volatile ("hlt");
        }
    }


    // Framebuffer
    if (!init_framebuffer(mbi_addr))
    {
        while (true)
        {
            asm volatile ("cli");
            asm volatile ("hlt");
        }
    }


    // Protected memory
    extern uint8_t kernel_start;
    extern uint8_t kernel_end;

    /*
        Kernel.
    */
    add_protected_range(reinterpret_cast<uint32_t>(&kernel_start), reinterpret_cast<uint32_t>(&kernel_end));

    /*
        Multiboot2 information.
    */

    uint8_t* mbi = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(mbi_addr));
    uint32_t mbi_size = *reinterpret_cast<uint32_t*>(mbi);


    add_protected_range(mbi_addr, static_cast<uint64_t>(mbi_addr) + mbi_size);


    /*
        Framebuffer.
    */

    if (framebuffer != nullptr)
    {
        uint64_t fb_start = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(framebuffer));
        uint64_t fb_size = static_cast<uint64_t>(framebuffer_pitch) * framebuffer_height;

        add_protected_range(fb_start, fb_start + fb_size);
    }


    // Screen
    clear_screen(COLOR_BLACK);

    cursor_x = 20;
    cursor_y = 20;

    print_color("NasuaOS Hardware Diagnostic Test\n", COLOR_GREEN);

    print("\n");


    // Multiboot2
    test_ok("Multiboot2");


    // Framebuffer
    if (test_framebuffer())
    {
        test_ok("Framebuffer");
    }
    else
    {
        test_fail("Framebuffer");
    }


    print("Resolution: ");

    print_dec(framebuffer_width);
    print("x");
    print_dec(framebuffer_height);
    print("x");
    print_dec(framebuffer_bpp);
    print("\n");

    print("Pitch: ");

    print_dec(framebuffer_pitch);
    print("\n\n");


    // CPU
    if (test_cpu())
    {
        test_ok("CPU / CPUID");

        print_cpu_vendor();
    }
    else
    {
        test_fail("CPU / CPUID");
    }


    // RDTSC
    if (test_rdtsc())
    {
        test_ok("CPU timestamp counter");
    }
    else
    {
        test_fail("CPU timestamp counter");
    }


    // PCI
    if (test_pci())
    {
        test_ok("PCI bus access");
    }
    else
    {
        test_fail("PCI bus access");
    }


    // Keyboard controller
    if (test_keyboard_controller())
    {
        test_ok("8042 keyboard controller");
    }
    else
    {
        test_fail("8042 keyboard controller");
    }


    // RTC
    if (test_rtc())
    {
        test_ok("RTC / CMOS");
    }
    else
    {
        test_fail("RTC / CMOS");
    }


    // PIT
    if (test_pit())
    {
        test_ok("PIT I/O");
    }
    else
    {
        test_fail("PIT I/O");
    }


    // RAM
    print("\n");

    print_color("Testing ALL available RAM...\n", COLOR_YELLOW);
    print("This may take a while.\n\n");

    if (test_all_ram(mbi_addr))
    {
        test_ok("ALL available RAM");
    }
    else
    {
        test_fail("ALL available RAM");
    }


    // Summary
    print_color("\n==============================\n",COLOR_CYAN);
    print_color("          SUMMARY\n",COLOR_CYAN);
    print_color("==============================\n",COLOR_CYAN);

    print("Tests passed: ");
    print_dec(tests_passed);
    print("\n");

    print("Tests failed: ");
    print_dec(tests_failed);
    print("\n");


    if (tests_failed == 0)
    {
        print_color("\nSYSTEM HARDWARE: OK\n", COLOR_GREEN);
    }
    else
    {
        print_color("\nHARDWARE ERRORS DETECTED\n", COLOR_RED);
    }


    // Halt
    while (true)
    {
        asm volatile ("cli");
        asm volatile ("hlt");
    }
}