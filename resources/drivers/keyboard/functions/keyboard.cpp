#include "../driver.hpp"

#include "libs/asm/asm.hpp"
#include "libs/libc/libc.hpp"

namespace Keyboard
{
    bool shift_pressed = false;
    bool caps_lock = false;
    bool extended_scancode = false;
    bool shell_input_enabled = true;

    const char keymap[128] =
    {
        0,27,
        '1','2','3','4','5','6','7','8','9','0','-','=',
        8,9,

        'q','w','e','r','t','y','u','i','o','p','[',']',
        '\n',0,

        'a','s','d','f','g','h','j','k','l',';','\'',
        '`',0,'\\',

        'z','x','c','v','b','n','m',',','.','/',
        0,'*',0,' '
    };

    char scancode_to_ascii_normal(uint8_t scancode)
    {
        switch (scancode)
        {
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
        switch (scancode)
        {
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

    void keyboard_set_leds(uint8_t leds)
    {
        uint32_t timeout = 100000;

        while (timeout--)
        {
            if (!(inb(0x64) & 0x02))
            {
                break;
            }
        }

        outb(0x60, 0xED);

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

        timeout = 100000;

        while (timeout--)
        {
            if (!(inb(0x64) & 0x02))
            {
                break;
            }
        }

        outb(0x60, leds);

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

} // namespace Keyboard

#ifdef __x86_64__

#include "system/drivers/drivers.hpp"
#include "system/interrupts/interrupts.hpp"

#include "applications/shell/commands.hpp"
#include "system/sysfunc/command_history/history.hpp"

#include "applications/applications.hpp"

#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/gui/gui.hpp"
#include "system/vars/info_vars/info_vars.hpp"

#include "libs/qr_code/qr_code.hpp"

#include "drivers/gpu/functions/windows_manager/window.hpp"

extern bool debug_mode;

namespace Keyboard
{

    char command_buffer[64];
    size_t cmd_idx = 0;

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

    static void replace_current_command(const char* new_cmd)
    {
        for (size_t i = 0; i < cmd_idx; i++)
        {
            Gpu::delete_last_char(8);
        }

        strcpy(command_buffer, new_cmd);
        cmd_idx = strlen(command_buffer);

        for (size_t i = 0; i < cmd_idx; i++)
        {
            Gpu::print_char8(command_buffer[i]);
        }

        Gpu::render_frame();
    }

    void handle_keyboard()
    {
        uint8_t status = inb(0x64);

        if (!(status & 1))
        {
            return;
        }

        if (status & 0x20)
        {
            uint8_t mouse_data = inb(0x60);

            Mouse::handle_byte(mouse_data);

            return;
        }

        uint8_t data = inb(0x60);

        uint8_t scancode = data;

        if (debug_mode)
        {
            print_sc(scancode);
        }

        if (scancode == 0xE0)
        {
            extended_scancode = true;
            return;
        }

        if (extended_scancode)
        {
            extended_scancode = false;

            if (scancode & 0x80)
            {
                return;
            }

            if (scancode == 0x5B || scancode == 0x5C)
            {
                if (!menu_start_open)
                {
                    open_start_menu();
                }
                else
                {
                    close_start_menu();
                }
                return;
            }

            if (scancode == 0x48 || scancode == 0x50 || scancode == 0x4B || scancode == 0x4D)
            {
                if (shift_pressed)
                {
                    if (scancode == 0x48)
                    {
                        const char* cmd = history_navigate_up();
                        if (cmd != nullptr)
                        {
                            replace_current_command(cmd);
                        }
                    }
                    else if (scancode == 0x50)
                    {
                        const char* cmd = history_navigate_down();
                        if (cmd != nullptr)
                        {
                            replace_current_command(cmd);
                        }
                    }
                }
                else
                {
                    int speed = 5;

                    if (scancode == 0x48 && Mouse::y >= speed)
                    {
                        Mouse::y -= speed;
                    }
                    else if (scancode == 0x50 && Mouse::y + 5 < (int)Gpu::fb->height)
                    {
                        Mouse::y += speed;
                    }
                    else if (scancode == 0x4B && Mouse::x >= speed)
                    {
                        Mouse::x -= speed;
                    }
                    else if (scancode == 0x4D && Mouse::x + 4 < (int)Gpu::fb->width)
                    {
                        Mouse::x += speed;
                    }
                }
                return;
            }
            return;
        }

        if (scancode & 0x80)
        {
            uint8_t rel = scancode & 0x7F;

            if (rel == 0x2A || rel == 0x36)
            {
                shift_pressed = false;
            }
            return;
        }

        if (scancode == 0x2A || scancode == 0x36)
        {
            shift_pressed = true;
            return;
        }

        if (scancode == 0x3A)
        {
            caps_lock = !caps_lock;

            keyboard_set_leds(caps_lock ? 0x04 : 0x00);

            return;
        }

        if (scancode == 0x0E)
        {
            if(Gpu::Window_Manager::active_window)
            {
                if(Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    Gpu::Window_Manager::send_key_to_window('\b');
                }
            }
            if(shell_input_enabled && !Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
            {
                if (cmd_idx > 0)
                {
                    cmd_idx--;
                    Gpu::delete_last_char(8);
                }

                return;
            }
        }

        if (scancode == 0x1C)
        {
            Mouse::left_click(true);

            if(Gpu::Window_Manager::active_window)
            {
                if(Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    Gpu::Window_Manager::send_key_to_window('\n');
                }
            }
            return;
        }

        char c = get_ascii(scancode);

        if(c)
        {
            if(Gpu::Window_Manager::active_window)
            {
                if(Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    Gpu::Window_Manager::send_key_to_window(c);
                }
            }

            if(shell_input_enabled && !Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
            {
                if(cmd_idx < 63)
                {
                    command_buffer[cmd_idx++] = c;
                    Gpu::print_char8(c);
                }
            }
        }
    }
} // namespace Keyboard

#endif // __x86_64__

#ifdef __i386__

#include "drivers/gpu/driver.hpp"

namespace Keyboard 
{
    void keyboard_init()
    {
        uint8_t status;

        do
        {
            asm volatile("inb $0x64, %0" : "=a"(status));
        } while(status & 2);

        uint8_t cmd = 0xAE;

        asm volatile("outb %0, $0x64" : : "a"(cmd));

        uint8_t enable = 0xF4;

        asm volatile("outb %0, $0x60" : : "a"(enable));
    }

    uint8_t read_scancode()
    {
        uint8_t status;

        while(true)
        {
            asm volatile("inb $0x64, %0" : "=a"(status));

            if(status & 1)
            {
                uint8_t code;

                asm volatile("inb $0x60, %0" : "=a"(code));

                return code;
            }

        }
    }

    void backspace()
    {
        if(input_pos == 0)
        {
            return;
        }

        input_pos--;

        if(Gpu::cursor_x >= Gpu::CHAR_WIDTH)
        {
            Gpu::cursor_x -= Gpu::CHAR_WIDTH;
        }

        Gpu::clear_char(Gpu::cursor_x, Gpu::cursor_y);
    }

    void read_line()
    {
        input_pos = 0;

        while(true)
        {

            uint8_t sc = read_scancode();

            if(sc & 0x80)
            {
                continue;
            }

            if(sc == 0x1C)
            {
                input_buffer[input_pos]=0;

                Gpu::putchar('\n');

                return;
            }

            if(sc == 0x0E)
            {
                backspace();
                continue;
            }

            char c = keymap[sc];

            if(c)
            {
                if(input_pos < 127)
                {
                    input_buffer[input_pos++] = c;

                    Gpu::putchar(c);
                }
            }
        }
    }
} // namespace Keyboard

#endif // __i386__
