#include "../driver.hpp"

#include "libs/asm/asm.hpp"
#include "drivers/gpu/driver.hpp"

namespace Keyboard
{
    bool shift_pressed = false;
    bool caps_lock = false;
    bool extended_scancode = false;
    bool shell_input_enabled = true;
    bool alt_pressed = false;
    char input_buffer[128] = {};
    uint32_t input_pos = 0;

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

    char translate_scancode(uint8_t scancode)
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

namespace Keyboard 
{
    void keyboard_init()
    {
        uint8_t status;

        do
        {
            status = inb(0x64);
        } while (status & 2);

        uint8_t cmd = 0xAE;
        outb(0x64, static_cast<uint8_t>(cmd));

        uint8_t enable = 0xF4;
        outb(0x60, static_cast<uint8_t>(enable));
    }

    uint8_t read_scancode()
    {
        uint8_t status;

        while(true)
        {
            status = inb(0x64);

            if(status & 1)
            {
                return inb(0x60);
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
        Gpu::backspace_console_char();
    }

    void read_line()
    {
        input_pos = 0;

        while(true)
        {

            uint8_t sc = read_scancode();

            if (sc == 0xE0)
            {
                extended_scancode = true;
                continue;
            }

            if (extended_scancode)
            {
                extended_scancode = false;
                continue;
            }

            if (sc & 0x80)
            {
                const uint8_t released = sc & 0x7F;
                if (released == 0x2A || released == 0x36)
                {
                    shift_pressed = false;
                }
                if (released == 0x38)
                {
                    alt_pressed = false;
                }
                continue;
            }

            if (sc == 0x2A || sc == 0x36)
            {
                shift_pressed = true;
                continue;
            }

            if (sc == 0x3A)
            {
                caps_lock = !caps_lock;
                keyboard_set_leds(caps_lock ? 0x04 : 0x00);
                continue;
            }

            if (sc == 0x38)
            {
                alt_pressed = true;
                continue;
            }

            if(sc == 0x1C)
            {
                input_buffer[input_pos]=0;

                Gpu::print_char8('\n');

                return;
            }

            if(sc == 0x0E)
            {
                backspace();
                continue;
            }

            char c = translate_scancode(sc);

            if(c)
            {
                if(input_pos < 127)
                {
                    input_buffer[input_pos++] = c;

                    Gpu::print_char8(c);
                }
            }
        }
    }
} // namespace Keyboard
