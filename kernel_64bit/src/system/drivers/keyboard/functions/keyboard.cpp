#include "../driver.hpp"

#include "system/drivers/drivers.hpp"
#include "system/interrupts/interrupts.hpp"

#include "applications/shell/commands.hpp"
#include "system/sysfunc/command_history/history.hpp"

#include "applications/applications.hpp"

#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/gui/gui.hpp"
#include "system/vars/info_vars/info_vars.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

char command_buffer[64];
size_t cmd_idx = 0;

bool shift_pressed = false;
bool caps_lock = false;
bool extended_scancode = false;

bool shell_input_enabled = true;

extern window_struct* apps[];

char scancode_to_ascii_normal(uint8_t scancode) 
{
    switch (scancode) {
        case 0x1E: return 'a'; case 0x30: return 'b'; case 0x2E: return 'c';
        case 0x20: return 'd'; case 0x12: return 'e'; case 0x21: return 'f';
        case 0x23: return 'h'; case 0x17: return 'i'; case 0x26: return 'l';
        case 0x32: return 'm'; case 0x31: return 'n'; case 0x19: return 'p';
        case 0x13: return 'r'; case 0x1F: return 's'; case 0x14: return 't';
        case 0x16: return 'u'; case 0x39: return ' '; case 0x22: return 'g';
        case 0x25: return 'k'; case 0x18: return 'o'; case 0x2F: return 'v';
        case 0x11: return 'w'; case 0x2C: return 'z'; case 0x15: return 'y';
        case 0x24: return 'j'; case 0x2D: return 'x'; case 0x10: return 'q';
        
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3';
        case 0x05: return '4'; case 0x06: return '5'; case 0x07: return '6';
        case 0x08: return '7'; case 0x09: return '8'; case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-'; case 0x33: return ','; case 0x34: return '.';
        case 0x35: return '/'; 
        case 0x27: return ';';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x0D: return '=';
        case 0x29: return '`';
        case 0x28: return '\'';
        case 0x2B: return '\\';
        default: return 0;
    }
}

char scancode_to_ascii_shift(uint8_t scancode) 
{
    switch (scancode) {
        case 0x1E: return 'A'; case 0x30: return 'B'; case 0x2E: return 'C';
        case 0x20: return 'D'; case 0x12: return 'E'; case 0x21: return 'F';
        case 0x23: return 'H'; case 0x17: return 'I'; case 0x26: return 'L';
        case 0x32: return 'M'; case 0x31: return 'N'; case 0x19: return 'P';
        case 0x13: return 'R'; case 0x1F: return 'S'; case 0x14: return 'T';
        case 0x16: return 'U'; case 0x39: return ' '; case 0x22: return 'G';
        case 0x25: return 'K'; case 0x18: return 'O'; case 0x2F: return 'V';
        case 0x11: return 'W'; case 0x2C: return 'Z'; case 0x15: return 'Y';
        case 0x24: return 'J'; case 0x2D: return 'X'; case 0x10: return 'Q';
        
        case 0x02: return '!'; case 0x03: return '@'; case 0x04: return '#';
        case 0x05: return '$'; case 0x06: return '%'; case 0x07: return '^';
        case 0x08: return '&'; case 0x09: return '*'; 
        case 0x0A: return '(';
        case 0x0B: return ')';
        case 0x0C: return '_'; 
        case 0x33: return '<';
        case 0x34: return '>';
        case 0x35: return '?';
        case 0x27: return ':';
        case 0x1A: return '{';
        case 0x1B: return '}';
        case 0x0D: return '+';
        case 0x29: return '~';
        case 0x28: return '"';
        case 0x2B: return '|';
        default: return 0;
    }
}

static char get_ascii(uint8_t scancode)
{
    char normal = scancode_to_ascii_normal(scancode);

    if (normal >= 'a' && normal <= 'z')
    {
        if (caps_lock ^ shift_pressed)
        {
            return normal - 'a' + 'A';
        }

        return normal;
    }

    return shift_pressed ? scancode_to_ascii_shift(scancode) : normal;
}

void keyboard_set_leds(uint8_t leds)
{
    // Wait until keyboard controller can accept data
    uint32_t timeout = 100000;

    while (timeout--)
    {
        if (!(inb(0x64) & 0x02))
        {
            break;
        }
    }

    // 0xED = Set LEDs
    outb(0x60, 0xED);

    // Wait for ACK
    timeout = 100000;

    while (timeout--)
    {
        if (inb(0x64) & 0x01)
        {
            break;
        }
    }

    uint8_t ack = inb(0x60);

    if (ack != 0xFA)
    {
        return;
    }


    // Send LED state
    timeout = 100000;

    while (timeout--)
    {
        if (!(inb(0x64) & 0x02))
        {
            break;
        }
    }

    outb(0x60, leds);


    // Wait for ACK
    timeout = 100000;

    while (timeout--)
    {
        if (inb(0x64) & 0x01)
        {
            break;
        }
    }

    inb(0x60);
}

void print_sc(uint8_t scancode) 
{
    char hex[3];

    const char* digits = "0123456789ABCDEF";

    hex[0] = digits[(scancode >> 4) & 0x0F];
    hex[1] = digits[scancode & 0x0F];
    hex[2] = '\0';

    Uart::puts("SC: 0x");
    Uart::puts(hex);
    Uart::puts("\n");
}

extern bool debug_mode;

extern char scancode_to_ascii_normal(uint8_t);
extern char scancode_to_ascii_shift(uint8_t);

static void replace_current_command(const char* new_cmd) 
{
    for (size_t i = 0; i < cmd_idx; i++) 
    {
        delete_last_char(8);
    }

    strcpy(command_buffer, new_cmd);
    cmd_idx = strlen(command_buffer);

    for (size_t i = 0; i < cmd_idx; i++) 
    {
        print_char8(command_buffer[i]);
    }

    render_frame();
}

void handle_keyboard() 
{
    uint8_t status = inb(0x64);

    // No data
    if (!(status & 1)) 
    {
        return;
    }

    // Mouse PS/2
    if (status & 0x20)
    {
        uint8_t mouse_data = inb(0x60);

        mouse_handle_byte(mouse_data);

        return;
    }

    uint8_t data = inb(0x60);

    uint8_t scancode = data;

    if (debug_mode) 
    {
        print_sc(scancode);
    }

    // Extended scancode support
    if (scancode == 0xE0) 
    {
        extended_scancode = true;
        return; 
    }

    // Handling extended scancode
    if (extended_scancode) 
    {
        extended_scancode = false;

        if (scancode & 0x80) 
        {
            return; 
        }

        // Left Windows
        if (scancode == 0x5B) 
        {
            if (!is_menu_start_open) 
            {
                open_start_menu();
            } 
            else 
            {
                close_start_menu();
            }
            return;
        }

        // Right Windows
        if (scancode == 0x5C) 
        {
            if (!is_menu_start_open) 
            {
                open_start_menu();
            } 
            else 
            {
                close_start_menu();
            }
            return;
        }

        // ARROW KEYS
        if (scancode == 0x48 || scancode == 0x50 || scancode == 0x4B || scancode == 0x4D) 
        {
            if (shift_pressed) 
            {
                if (scancode == 0x48) 
                { // SHIFT + UP
                    const char* cmd = history_navigate_up();
                    if (cmd != nullptr) 
                    {
                        replace_current_command(cmd);
                    }
                } 
                else if (scancode == 0x50) 
                { // SHIFT + DOWN
                    const char* cmd = history_navigate_down();
                    if (cmd != nullptr) 
                    {
                        replace_current_command(cmd);
                    }
                }
            } 
            else 
            {
                // Mouse moving
                int speed = 5;

                if (scancode == 0x48 && mouse_y >= speed)
                {
                    mouse_y -= speed;
                }
                else if (scancode == 0x50 && mouse_y + 5 < (int)fb->height)
                {
                    mouse_y += speed;
                }
                else if (scancode == 0x4B && mouse_x >= speed)
                {
                    mouse_x -= speed;
                }
                else if (scancode == 0x4D && mouse_x + 4 < (int)fb->width)
                {
                    mouse_x += speed;
                }
            }
            return;
        }
        return; 
    }

    // Break Code
    if (scancode & 0x80) 
    {
        uint8_t rel = scancode & 0x7F;

        if (rel == 0x2A || rel == 0x36) 
        {
            shift_pressed = false;
        }
        return;
    }

    // SHIFT DOWN
    if (scancode == 0x2A || scancode == 0x36) 
    {
        shift_pressed = true;
        return;
    }

    // CAPS LOCK
    if (scancode == 0x3A)
    {
        caps_lock = !caps_lock;

        keyboard_set_leds(caps_lock ? 0x04 : 0x00);

        return;
    }

    // BACKSPACE
    if (scancode == 0x0E) 
    {
        if(active_window)
        {
            if(is_mouse_over_any_window(mouse_x, mouse_y)) 
            {
                send_key_to_window('\b');
            }
        }
        if(shell_input_enabled && !is_mouse_over_any_window(mouse_x, mouse_y)) 
        {
            if (cmd_idx > 0) 
            {
                cmd_idx--;
                delete_last_char(8);
            }

            return;
        }
    }

    // ENTER
    if (scancode == 0x1C) 
    {
        handle_left_click(true);

        if(active_window)
        {
            if(is_mouse_over_any_window(mouse_x, mouse_y)) 
            {
                send_key_to_window('\n');
            }
        }
        return;
    }

    char c = get_ascii(scancode);

    if(c)
    {
        if(active_window)
        {
            if(is_mouse_over_any_window(mouse_x, mouse_y)) 
            {
                send_key_to_window(c);
            }
        }
        if(shell_input_enabled && !is_mouse_over_any_window(mouse_x, mouse_y))
        {
            if(cmd_idx < 63)
            {
                command_buffer[cmd_idx++] = c;
                print_char8(c);
            }
        }
    }
}