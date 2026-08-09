#include <stdint.h>

#include "font8x8.h"


// MULTIBOOT2
#define MULTIBOOT2_MAGIC       0x36D76289
#define MB2_TAG_END            0
#define MB2_TAG_FRAMEBUFFER    8


// MULTIBOOT2 STRUCTURES
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


// FRAMEBUFFER
static uint8_t* framebuffer = nullptr;

static uint32_t fb_width  = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch  = 0;
static uint32_t fb_bpp    = 0;


// COLORS
static const uint32_t COLOR_BG      = 0x00101820;
static const uint32_t COLOR_PANEL   = 0x00203040;
static const uint32_t COLOR_PANEL2  = 0x0018202C;
static const uint32_t COLOR_WHITE   = 0x00FFFFFF;
static const uint32_t COLOR_GRAY    = 0x00808090;
static const uint32_t COLOR_BLUE    = 0x004080FF;
static const uint32_t COLOR_GREEN   = 0x0044FF88;
static const uint32_t COLOR_YELLOW  = 0x00FFD34D;
static const uint32_t COLOR_RED     = 0x00FF5555;


// CURSOR
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;


// SETTINGS
struct system_settings
{
    uint8_t  fast_boot;
    uint8_t  usb_legacy;
    uint8_t  acpi;
    uint8_t  cpu_features;

    uint8_t  boot_mode;
    uint8_t  memory_test;

    uint8_t  secure_boot;
    uint8_t  virtualization;
    uint8_t  hyper_threading;
    uint8_t  numlock;

    uint8_t  boot_device;
    uint8_t  graphics_mode;
    uint8_t  keyboard_layout;

    uint8_t  sata_mode;
    uint8_t  network_boot;
    uint8_t  fan_control;

    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    uint8_t  day;
    uint8_t  month;

    uint16_t year;
};

static system_settings settings;


// DEFAULTS
static void load_defaults()
{
    settings.fast_boot       = 1;
    settings.usb_legacy      = 1;
    settings.acpi            = 1;
    settings.cpu_features   = 1;

    settings.boot_mode      = 0;
    settings.memory_test    = 0;

    settings.secure_boot    = 0;
    settings.virtualization = 1;
    settings.hyper_threading = 1;
    settings.numlock        = 1;

    settings.boot_device    = 0;
    settings.graphics_mode  = 0;
    settings.keyboard_layout = 0;

    settings.sata_mode      = 0;
    settings.network_boot   = 0;
    settings.fan_control    = 1;

    settings.hour            = 0;
    settings.minute          = 0;
    settings.second          = 0;

    settings.day             = 1;
    settings.month           = 1;
    settings.year            = 2020;
}


// PORT I/O
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}


// FRAMEBUFFER
static void put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height)
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
        uint32_t* row = (uint32_t*)(framebuffer + y * fb_pitch);

        for (uint32_t x = 0; x < fb_width; x++)
        {
            row[x] = color;
        }
    }
}


// CHARACTER
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

        draw_char(c, cursor_x, cursor_y, color);

        cursor_x += 8;
    }
}


// PRINT AT
static void print_at(uint32_t x, uint32_t y, const char* text, uint32_t color)
{
    cursor_x = x;
    cursor_y = y;

    print(text, color);
}


// DECIMAL
static void print_dec(uint32_t value, uint32_t color)
{
    char buffer[16];

    uint32_t i = 0;

    if (value == 0)
    {
        print("0", color);
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

        print(c, color);
    }
}


// BOOL
static void print_bool(uint8_t value)
{
    if (value)
    {
        print("Enabled", COLOR_GREEN);
    }
    else
    {
        print("Disabled", COLOR_RED);
    }
}


// LINES / RECTANGLES
static void line(uint32_t y, uint32_t color)
{
    for (uint32_t x = 0; x < fb_width; x++)
    {
        put_pixel(x, y, color);
    }
}

static void rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t yy = y; yy < y + h; yy++)
    {
        for (uint32_t xx = x; xx < x + w; xx++)
        {
            put_pixel(xx, yy, color);
        }
    }
}


// KEYBOARD
static bool keyboard_data_available()
{
    return (inb(0x64) & 0x01) != 0;
}

enum key_code
{
    KEY_NONE = 0,

    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    KEY_ENTER,
    KEY_ESC,

    KEY_F9,
    KEY_F10,

    KEY_TAB
};

static key_code keyboard_get_key()
{
    static bool extended = false;

    while (!keyboard_data_available())
    {
    }

    uint8_t scancode = inb(0x60);

    // E0 prefix
    if (scancode == 0xE0)
    {
        extended = true;
        return KEY_NONE;
    }

    // F0 is not used on standard PS/2 set 1,
    // but ignore it defensively.
    if (scancode == 0xF0)
    {
        extended = false;
        return KEY_NONE;
    }

    // Key release.
    if (scancode & 0x80)
    {
        extended = false;
        return KEY_NONE;
    }

    if (extended)
    {
        extended = false;

        switch (scancode)
        {
            case 0x48:
                return KEY_UP;

            case 0x50:
                return KEY_DOWN;

            case 0x4B:
                return KEY_LEFT;

            case 0x4D:
                return KEY_RIGHT;

            case 0x1C:
                return KEY_ENTER;

            case 0x53:
                return KEY_NONE;
        }

        return KEY_NONE;
    }

    switch (scancode)
    {
        case 0x1C:
            return KEY_ENTER;

        case 0x01:
            return KEY_ESC;

        case 0x43:
            return KEY_F9;

        case 0x44:
            return KEY_F10;

        case 0x0F:
            return KEY_TAB;

        /*
         * Some old keyboards/emulators may send
         * non-E0 arrow codes.
         */
        case 0x48:
            return KEY_UP;

        case 0x50:
            return KEY_DOWN;

        case 0x4B:
            return KEY_LEFT;

        case 0x4D:
            return KEY_RIGHT;
    }

    return KEY_NONE;
}


// ENUMS
enum menu_item
{
    MENU_TIME = 0,
    MENU_DATE,

    MENU_BOOT_MODE,

    MENU_FAST_BOOT,
    MENU_USB_LEGACY,
    MENU_ACPI,
    MENU_CPU_FEATURES,

    MENU_MEMORY_TEST,
    MENU_SECURE_BOOT,
    MENU_VIRTUALIZATION,
    MENU_HYPER_THREADING,
    MENU_NUMLOCK,

    MENU_BOOT_DEVICE,
    MENU_GRAPHICS_MODE,
    MENU_KEYBOARD_LAYOUT,

    MENU_SATA_MODE,
    MENU_NETWORK_BOOT,
    MENU_FAN_CONTROL,

    MENU_SAVE_EXIT,
    MENU_EXIT,

    MENU_COUNT
};


// PAGES
enum system_page
{
    PAGE_MAIN = 0,
    PAGE_ADVANCED,
    PAGE_BOOT,
    PAGE_SECURITY,
    PAGE_EXIT,

    PAGE_COUNT
};

static uint32_t current_page = PAGE_MAIN;


// PAGE NAMES
static const char* page_name(uint32_t page)
{
    switch (page)
    {
        case PAGE_MAIN:
            return "Main";

        case PAGE_ADVANCED:
            return "Advanced";

        case PAGE_BOOT:
            return "Boot";

        case PAGE_SECURITY:
            return "Security";

        case PAGE_EXIT:
            return "Exit";
    }

    return "?";
}


// ITEMS PER PAGE
static uint32_t page_item_count(uint32_t page)
{
    switch (page)
    {
        case PAGE_MAIN:
            return 2;

        case PAGE_ADVANCED:
            return 9;

        case PAGE_BOOT:
            return 5;

        case PAGE_SECURITY:
            return 4;

        case PAGE_EXIT:
            return 2;
    }

    return 0;
}


// PAGE -> ITEM
static uint32_t page_item(uint32_t page, uint32_t index)
{
    switch (page)
    {
        case PAGE_MAIN:
        {
            static const uint32_t items[] =
            {
                MENU_TIME,
                MENU_DATE
            };

            return items[index];
        }

        case PAGE_ADVANCED:
        {
            static const uint32_t items[] =
            {
                MENU_BOOT_MODE,
                MENU_FAST_BOOT,
                MENU_USB_LEGACY,
                MENU_ACPI,
                MENU_CPU_FEATURES,
                MENU_MEMORY_TEST,
                MENU_VIRTUALIZATION,
                MENU_HYPER_THREADING,
                MENU_NUMLOCK
            };

            return items[index];
        }

        case PAGE_BOOT:
        {
            static const uint32_t items[] =
            {
                MENU_BOOT_DEVICE,
                MENU_GRAPHICS_MODE,
                MENU_KEYBOARD_LAYOUT,
                MENU_SATA_MODE,
                MENU_NETWORK_BOOT
            };

            return items[index];
        }

        case PAGE_SECURITY:
        {
            static const uint32_t items[] =
            {
                MENU_SECURE_BOOT,
                MENU_FAN_CONTROL,
                MENU_SAVE_EXIT,
                MENU_EXIT
            };

            return items[index];
        }

        case PAGE_EXIT:
        {
            static const uint32_t items[] =
            {
                MENU_SAVE_EXIT,
                MENU_EXIT
            };

            return items[index];
        }
    }

    return MENU_TIME;
}


// PRINT VALUE
static void print_menu_value(uint32_t item, uint32_t color)
{
    switch (item)
    {
        case MENU_TIME:

            if (settings.hour < 10)
            {
                print("0", color);
            }

            print_dec(settings.hour, color);
            print(":", color);

            if (settings.minute < 10)
            {
                print("0", color);
            }

            print_dec(settings.minute, color);
            print(":", color);

            if (settings.second < 10)
            {
                print("0", color);
            }

            print_dec(settings.second, color);

            break;

        case MENU_DATE:

            if (settings.day < 10)
            {
                print("0", color);
            }

            print_dec(settings.day, color);
            print("/", color);

            if (settings.month < 10)
            {
                print("0", color);
            }

            print_dec(settings.month, color);
            print("/", color);

            print_dec(settings.year, color);

            break;

        case MENU_BOOT_MODE:

            if (settings.boot_mode == 0)
            {
                print("Normal", COLOR_GREEN);
            }
            else if (settings.boot_mode == 1)
            {
                print("Safe Mode", COLOR_YELLOW);
            }
            else
            {
                print("Recovery", COLOR_RED);
            }

            break;

        case MENU_FAST_BOOT:
            print_bool(settings.fast_boot);
            break;

        case MENU_USB_LEGACY:
            print_bool(settings.usb_legacy);
            break;

        case MENU_ACPI:
            print_bool(settings.acpi);
            break;

        case MENU_CPU_FEATURES:
            print_bool(settings.cpu_features);
            break;

        case MENU_MEMORY_TEST:
            print_bool(settings.memory_test);
            break;

        case MENU_SECURE_BOOT:
            print_bool(settings.secure_boot);
            break;

        case MENU_VIRTUALIZATION:
            print_bool(settings.virtualization);
            break;

        case MENU_HYPER_THREADING:
            print_bool(settings.hyper_threading);
            break;

        case MENU_NUMLOCK:
            print_bool(settings.numlock);
            break;

        case MENU_BOOT_DEVICE:

            switch (settings.boot_device)
            {
                case 0:
                    print("Hard Disk", color);
                    break;

                case 1:
                    print("USB", color);
                    break;

                case 2:
                    print("CD/DVD", color);
                    break;

                default:
                    print("Network", color);
                    break;
            }

            break;

        case MENU_GRAPHICS_MODE:

            if (settings.graphics_mode == 0)
            {
                print("Auto", color);
            }
            else if (settings.graphics_mode == 1)
            {
                print("Text", color);
            }
            else
            {
                print("Framebuffer", color);
            }

            break;

        case MENU_KEYBOARD_LAYOUT:

            if (settings.keyboard_layout == 0)
            {
                print("US", color);
            }
            else if (settings.keyboard_layout == 1)
            {
                print("UK", color);
            }
            else
            {
                print("PL", color);
            }

            break;

        case MENU_SATA_MODE:

            if (settings.sata_mode == 0)
            {
                print("AHCI", color);
            }
            else if (settings.sata_mode == 1)
            {
                print("IDE", color);
            }
            else
            {
                print("RAID", color);
            }

            break;

        case MENU_NETWORK_BOOT:
            print_bool(settings.network_boot);
            break;

        case MENU_FAN_CONTROL:
            print_bool(settings.fan_control);
            break;

        case MENU_SAVE_EXIT:
            print("Save", COLOR_GREEN);
            break;

        case MENU_EXIT:
            print("Exit", COLOR_RED);
            break;
    }
}


// DRAW PAGE BAR
static void draw_page_bar()
{
    uint32_t x = 32;

    for (uint32_t i = 0; i < PAGE_COUNT; i++)
    {
        print_at(x, 12, i == current_page ? "[" : " ", i == current_page ? COLOR_YELLOW : COLOR_GRAY);

        x += 8;

        print_at(x, 12, page_name(i), i == current_page ? COLOR_YELLOW : COLOR_GRAY);

        x += 8 * 1;

        // Find approximate width.
        if (i == PAGE_MAIN)
        {
            x += 32;
        }
        else if (i == PAGE_ADVANCED)
        {
            x += 64;
        }
        else if (i == PAGE_BOOT)
        {
            x += 32;
        }
        else if (i == PAGE_SECURITY)
        {
            x += 64;
        }
        else
        {
            x += 24;
        }

        print_at(x, 12, i == current_page ? "]" : " ", i == current_page ? COLOR_YELLOW : COLOR_GRAY);

        x += 24;
    }
}


// DRAW MENU ITEM
static void draw_item(uint32_t index, uint32_t selected, uint32_t y)
{
    uint32_t item = page_item(current_page, index);
    bool is_selected = index == selected;
    uint32_t color = is_selected ? COLOR_YELLOW : COLOR_WHITE;

    if (is_selected)
    {
        rect(24, y - 4, fb_width > 48 ? fb_width - 48 : 0, 18, COLOR_PANEL);

        print_at(32, y, ">", COLOR_YELLOW);
    }

    switch (item)
    {
        case MENU_TIME:
            print_at(56, y, "System Time", color);
            break;

        case MENU_DATE:
            print_at(56, y, "System Date", color);
            break;

        case MENU_BOOT_MODE:
            print_at(56, y, "Boot Mode", color);
            break;

        case MENU_FAST_BOOT:
            print_at(56, y, "Fast Boot", color);
            break;

        case MENU_USB_LEGACY:
            print_at(56, y, "USB Legacy Support", color);
            break;

        case MENU_ACPI:
            print_at(56, y, "ACPI", color);
            break;

        case MENU_CPU_FEATURES:
            print_at(56, y, "CPU Features", color);
            break;

        case MENU_MEMORY_TEST:
            print_at(56, y, "Memory Test", color);
            break;

        case MENU_SECURE_BOOT:
            print_at(56, y, "Secure Boot", color);
            break;

        case MENU_VIRTUALIZATION:
            print_at(56, y, "CPU Virtualization", color);
            break;

        case MENU_HYPER_THREADING:
            print_at(56, y, "Hyper-Threading", color);
            break;

        case MENU_NUMLOCK:
            print_at(56, y, "NumLock", color);
            break;

        case MENU_BOOT_DEVICE:
            print_at(56, y, "Boot Device", color);
            break;

        case MENU_GRAPHICS_MODE:
            print_at(56, y, "Graphics Mode", color);
            break;

        case MENU_KEYBOARD_LAYOUT:
            print_at(56, y, "Keyboard Layout", color);
            break;

        case MENU_SATA_MODE:
            print_at(56, y, "SATA Mode", color);
            break;

        case MENU_NETWORK_BOOT:
            print_at(56, y, "Network Boot", color);
            break;

        case MENU_FAN_CONTROL:
            print_at(56, y, "Fan Control", color);
            break;

        case MENU_SAVE_EXIT:
            print_at(56, y, "Save & Exit", color);
            break;

        case MENU_EXIT:
            print_at(56, y, "Exit Without Saving", color);
            break;
    }

    print_at(300, y, "", color);
    print_menu_value(item, is_selected ? COLOR_YELLOW : COLOR_WHITE);
}


// DRAW MENU
static void draw_menu(uint32_t selected, uint32_t selected_field)
{
    clear_screen(COLOR_BG);
    print_at(32, 0, "NASUAOS System SETUP", COLOR_GREEN);
    draw_page_bar();
    line(30, COLOR_BLUE);
    print_at(32, 42, "Configuration Utility", COLOR_GRAY);

    uint32_t count = page_item_count(current_page);
    uint32_t y = 70;

    for (uint32_t i = 0; i < count; i++)
    {
        draw_item(i, selected, y);

        y += 24;
    }


    // Field editor
    uint32_t item = page_item(current_page, selected);
    uint32_t footer_y = fb_height > 32 ? fb_height - 32 : 0;
    uint32_t editor_y = footer_y >= 48 ? footer_y - 34 : 300;

    if (item == MENU_TIME)
    {
        print_at(16, editor_y, "Time fields:", COLOR_GRAY);
        print_at(16, editor_y + 14, selected_field == 0 ? "[HOUR]" : " HOUR ", selected_field == 0 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(72, editor_y + 14, selected_field == 1 ? "[MIN]" : " MIN ", selected_field == 1 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(112, editor_y + 14, selected_field == 2 ? "[SEC]" : " SEC ", selected_field == 2 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(172, editor_y + 14, "LEFT/RIGHT = field", COLOR_GRAY);
    }

    if (item == MENU_DATE)
    {
        print_at(16, editor_y, "Date fields:", COLOR_GRAY);
        print_at(16, editor_y + 14, selected_field == 0 ? "[DAY]" : " DAY ", selected_field == 0 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(72, editor_y + 14, selected_field == 1 ? "[MON]" : " MON ", selected_field == 1 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(112, editor_y + 14, selected_field == 2 ? "[YEAR]" : " YEAR ", selected_field == 2 ? COLOR_YELLOW : COLOR_GRAY);
        print_at(172, editor_y + 14, "LEFT/RIGHT = field", COLOR_GRAY);
    }

    // Footer

    line(footer_y, COLOR_BLUE);

    print_at(16, fb_height > 24 ? fb_height - 24 : 0, "UP/DOWN Select   LEFT/RIGHT Change   TAB Page   ENTER Select   F9 Defaults   F10 Save", COLOR_GRAY);
}


// CHANGE TIME FIELD
static void change_time(uint32_t field, int direction)
{
    if (field == 0)
    {
        if (direction > 0)
        {
            settings.hour++;

            if (settings.hour >= 24)
            {
                settings.hour = 0;
            }
        }
        else
        {
            if (settings.hour == 0)
            {
                settings.hour = 23;
            }
            else
            {
                settings.hour--;
            }
        }
    }

    else if (field == 1)
    {
        if (direction > 0)
        {
            settings.minute++;

            if (settings.minute >= 60)
            {
                settings.minute = 0;
            }
        }
        else
        {
            if (settings.minute == 0)
            {
                settings.minute = 59;
            }
            else
            {
                settings.minute--;
            }
        }
    }

    else if (field == 2)
    {
        if (direction > 0)
        {
            settings.second++;

            if (settings.second >= 60)
            {
                settings.second = 0;
            }
        }
        else
        {
            if (settings.second == 0)
            {
                settings.second = 59;
            }
            else
            {
                settings.second--;
            }
        }
    }
}


// CHANGE DATE FIELD
static void change_date(uint32_t field, int direction)
{
    if (field == 0)
    {
        if (direction > 0)
        {
            settings.day++;

            if (settings.day > 31)
            {
                settings.day = 1;
            }
        }
        else
        {
            if (settings.day <= 1)
            {
                settings.day = 31;
            }
            else
            {
                settings.day--;
            }
        }
    }

    else if (field == 1)
    {
        if (direction > 0)
        {
            settings.month++;

            if (settings.month > 12)
            {
                settings.month = 1;
            }
        }
        else
        {
            if (settings.month <= 1)
            {
                settings.month = 12;
            }
            else
            {
                settings.month--;
            }
        }
    }

    else if (field == 2)
    {
        if (direction > 0)
        {
            settings.year++;

            if (settings.year > 9999)
            {
                settings.year = 1;
            }
        }
        else
        {
            if (settings.year <= 0)
            {
                settings.year = 9999;
            }
            else
            {
                settings.year--;
            }
        }
    }
}


// CHANGE OPTION
static void change_option(uint32_t item, uint32_t field, int direction)
{
    switch (item)
    {
        case MENU_TIME:
            change_time(field, direction);
            break;

        case MENU_DATE:
            change_date(field, direction);
            break;

        case MENU_BOOT_MODE:

            if (direction > 0)
            {
                settings.boot_mode++;

                if (settings.boot_mode > 2)
                {
                    settings.boot_mode = 0;
                }
            }
            else
            {
                if (settings.boot_mode == 0)
                {
                    settings.boot_mode = 2;
                }
                else
                {
                    settings.boot_mode--;
                }
            }

            break;

        case MENU_FAST_BOOT:
            settings.fast_boot ^= 1;
            break;

        case MENU_USB_LEGACY:
            settings.usb_legacy ^= 1;
            break;

        case MENU_ACPI:
            settings.acpi ^= 1;
            break;

        case MENU_CPU_FEATURES:
            settings.cpu_features ^= 1;
            break;

        case MENU_MEMORY_TEST:
            settings.memory_test ^= 1;
            break;

        case MENU_SECURE_BOOT:
            settings.secure_boot ^= 1;
            break;

        case MENU_VIRTUALIZATION:
            settings.virtualization ^= 1;
            break;

        case MENU_HYPER_THREADING:
            settings.hyper_threading ^= 1;
            break;

        case MENU_NUMLOCK:
            settings.numlock ^= 1;
            break;

        case MENU_BOOT_DEVICE:

            if (direction > 0)
            {
                settings.boot_device++;

                if (settings.boot_device > 3)
                {
                    settings.boot_device = 0;
                }
            }
            else
            {
                if (settings.boot_device == 0)
                {
                    settings.boot_device = 3;
                }
                else
                {
                    settings.boot_device--;
                }
            }

            break;

        case MENU_GRAPHICS_MODE:

            if (direction > 0)
            {
                settings.graphics_mode++;

                if (settings.graphics_mode > 2)
                {
                    settings.graphics_mode = 0;
                }
            }
            else
            {
                if (settings.graphics_mode == 0)
                {
                    settings.graphics_mode = 2;
                }
                else
                {
                    settings.graphics_mode--;
                }
            }

            break;

        case MENU_KEYBOARD_LAYOUT:

            if (direction > 0)
            {
                settings.keyboard_layout++;

                if (settings.keyboard_layout > 2)
                {
                    settings.keyboard_layout = 0;
                }
            }
            else
            {
                if (settings.keyboard_layout == 0)
                {
                    settings.keyboard_layout = 2;
                }
                else
                {
                    settings.keyboard_layout--;
                }
            }

            break;

        case MENU_SATA_MODE:

            if (direction > 0)
            {
                settings.sata_mode++;

                if (settings.sata_mode > 2)
                {
                    settings.sata_mode = 0;
                }
            }
            else
            {
                if (settings.sata_mode == 0)
                {
                    settings.sata_mode = 2;
                }
                else
                {
                    settings.sata_mode--;
                }
            }

            break;

        case MENU_NETWORK_BOOT:
            settings.network_boot ^= 1;
            break;

        case MENU_FAN_CONTROL:
            settings.fan_control ^= 1;
            break;
    }
}


// REBOOT
static void reboot_system()
{
    asm volatile("cli");

    // Wait until keyboard controller input buffer is free.
    for (uint32_t i = 0; i < 1000000; i++)
    {
        if ((inb(0x64) & 0x02) == 0)
        {
            break;
        }
    }

    // 8042 reset command.
    outb(0x64, 0xFE);

    for (;;)
    {
        asm volatile("hlt");
    }
}


// SAVE SCREEN
static void save_and_reboot()
{
    clear_screen(COLOR_BG);

    print_at(32, 40, "Saving configuration...", COLOR_WHITE);
    print_at(32, 64, "Configuration saved.", COLOR_GREEN);

    // Small busy delay.
    for (volatile uint32_t i = 0; i < 30000000; i++)
    {
    }

    print_at(32, 88, "Restarting...", COLOR_YELLOW);

    for (volatile uint32_t i = 0; i < 15000000;i++)
    {
    }

    reboot_system();
}


// EXIT
static void exit_setup()
{
    clear_screen(COLOR_BG);

    print_at(32, 40, "Leaving System Setup...", COLOR_WHITE);

    print_at(32, 64, "Restarting system...", COLOR_YELLOW);

    for (volatile uint32_t i = 0; i < 50000000; i++)
    {
    }

    reboot_system();
}


// HANDLE ENTER
static void handle_enter(uint32_t item)
{
    if (item == MENU_SAVE_EXIT)
    {
        save_and_reboot();
        return;
    }

    if (item == MENU_EXIT)
    {
        exit_setup();
        return;
    }

    change_option(item, 0, 1);
}


// SYSTEM SETUP
static void system_setup()
{
    load_defaults();

    current_page = PAGE_MAIN;

    uint32_t selected = 0;
    uint32_t selected_field = 0;

    draw_menu(selected, selected_field);

    for (;;)
    {
        key_code key = keyboard_get_key();

        if (key == KEY_NONE)
        {
            continue;
        }

        uint32_t count = page_item_count(current_page);

        // UP
        if (key == KEY_UP)
        {
            if (selected == 0)
            {
                selected = count - 1;
            }
            else
            {
                selected--;
            }

            selected_field = 0;

            draw_menu(selected, selected_field);

            continue;
        }

        // DOWN
        if (key == KEY_DOWN)
        {
            selected++;

            if (selected >= count)
            {
                selected = 0;
            }

            selected_field = 0;

            draw_menu(selected, selected_field);

            continue;
        }

        uint32_t item = page_item(current_page, selected);

        // LEFT
        if (key == KEY_LEFT)
        {
            if (item == MENU_TIME)
            {
                if (selected_field == 0)
                {
                    selected_field = 2;
                }
                else
                {
                    selected_field--;
                }

                draw_menu(selected, selected_field);

                continue;
            }

            if (item == MENU_DATE)
            {
                if (selected_field == 0)
                {
                    selected_field = 2;
                }
                else
                {
                    selected_field--;
                }

                draw_menu(selected, selected_field);

                continue;
            }

            change_option(item, 0, -1);
            draw_menu(selected, selected_field);

            continue;
        }

        // RIGHT
        if (key == KEY_RIGHT)
        {
            if (item == MENU_TIME)
            {
                selected_field++;

                if (selected_field > 2)
                {
                    selected_field = 0;
                }

                draw_menu(selected, selected_field);

                continue;
            }

            if (item == MENU_DATE)
            {
                selected_field++;

                if (selected_field > 2)
                {
                    selected_field = 0;
                }

                draw_menu(selected, selected_field);

                continue;
            }

            change_option(item, 0, 1);

            draw_menu(selected, selected_field);

            continue;
        }

        // ENTER
        if (key == KEY_ENTER)
        {
            if (item == MENU_TIME)
            {
                change_time(selected_field, 1);

                draw_menu(selected, selected_field);

                continue;
            }

            if (item == MENU_DATE)
            {
                change_date(selected_field, 1);

                draw_menu(selected, selected_field);

                continue;
            }

            handle_enter(item);

            draw_menu(selected, selected_field);

            continue;
        }

        // TAB
        if (key == KEY_TAB)
        {
            current_page++;

            if (current_page >= PAGE_COUNT)
            {
                current_page = PAGE_MAIN;
            }

            selected = 0;
            selected_field = 0;

            draw_menu(selected, selected_field);

            continue;
        }

        // F9
        if (key == KEY_F9)
        {
            load_defaults();

            selected_field = 0;

            draw_menu(selected, selected_field);

            continue;
        }

        // F10
        if (key == KEY_F10)
        {
            save_and_reboot();
        }

        // ESC
        if (key == KEY_ESC)
        {
            exit_setup();
        }
    }
}


// FIND FRAMEBUFFER
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

            if (fb->framebuffer_bpp != 32)
            {
                return false;
            }

            framebuffer = (uint8_t*)(uint32_t)fb->framebuffer_addr;
            fb_width = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch = fb->framebuffer_pitch;
            fb_bpp = fb->framebuffer_bpp;

            return true;
        }

        current += (tag->size + 7) & ~7;
    }

    return false;
}


// MAIN
extern "C" void kmain(uint32_t magic, uint32_t mb_info)
{
    if (magic != MULTIBOOT2_MAGIC)
    {
        return;
    }

    if (!find_framebuffer(mb_info))
    {
        return;
    }

    system_setup();

    for (;;)
    {
        asm volatile("hlt");
    }
}