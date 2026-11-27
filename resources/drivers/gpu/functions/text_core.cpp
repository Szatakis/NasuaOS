#include "../driver.hpp"

namespace Gpu
{
    uint32_t current_text_color = 0xFFFFFFFF;

    void print(const char* text)
    {
        while (*text)
        {
            if (*text == '\x1B' && text[1] == '[')
            {
                switch (text[2])
                {
                    case 'w': current_text_color = 0xFFFFFFFF; break;
                    case 'r': current_text_color = 0xFFFF5555; break;
                    case 'g': current_text_color = 0xFF59E81B; break;
                    case 'b': current_text_color = 0xFF59AEED; break;
                    case 'y': current_text_color = 0xFFFFD966; break;
                    case 'a': current_text_color = 0xFFAAAAAA; break;
                    case 'e': current_text_color = 0xFF7EC2D6; break;
                    case 't': current_text_color = 0xFFEBCB8B; break;
                    case 'u': current_text_color = 0xFFBF616A; break;
                    default: break;
                }

                text += 3;
                continue;
            }

            print_char8(*text++);
        }
    }

    void print_int(int value)
    {
        char digits[12];
        size_t count = 0;
        unsigned int magnitude;

        if (value < 0)
        {
            print_char8('-');
            magnitude = 0U - static_cast<unsigned int>(value);
        }
        else
        {
            magnitude = static_cast<unsigned int>(value);
        }

        do
        {
            digits[count++] = static_cast<char>('0' + magnitude % 10);
            magnitude /= 10;
        } while (magnitude != 0);

        while (count != 0)
        {
            print_char8(digits[--count]);
        }
    }

    void print_hex(uint32_t value)
    {
        static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
        print("0x");

        bool started = false;
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            const uint8_t digit = static_cast<uint8_t>((value >> shift) & 0xF);
            if (digit != 0 || started || shift == 0)
            {
                started = true;
                print_char8(HEX_DIGITS[digit]);
            }
        }
    }

    void print_num8(uint32_t value)
    {
        char digits[10];
        size_t count = 0;

        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);

        while (count != 0)
        {
            print_char8(digits[--count]);
        }
    }

    void fetch()
    {
        print("\x1B[w\n $$\\   $$\\                                          $$$$$$\\   $$$$$$\\  \n");
        print(" $$$\\  $$ |                                        $$  __$$\\ $$  __$$\\ \n");
        print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
        print(" $$$$\\ $$ | $$$$$$\\   $$$$$$$\\ $$\\   $$\\  $$$$$$\\  $$ /  $$ |$$ /  \\__|\n");
        print(" $$ $$\\$$ | \\____$$\\ $$  _____|$$ |  $$ | \\____$$\\ $$ |  $$ |\\$$$$$$\\  \n");
        print(" $$ \\$$$$ | $$$$$$$ |\\$$$$$$\\  $$ |  $$ | $$$$$$$ |$$ |  $$ | \\____$$\\ \n");
        print(" $$ |\\$$$ |$$  __$$ | \\____$$\\ $$ |  $$ |$$  __$$ |$$ |  $$ |$$\\   $$ |\n");
        print(" $$ | \\$$ |\\$$$$$$$ |$$$$$$$  |\\$$$$$$  |\\$$$$$$$ | $$$$$$  |\\$$$$$$  |\n");
        print(" \\__|  \\__| \\_______|\\_______/  \\______/  \\_______/ \\______/  \\______/ \n\n");
    }
}
