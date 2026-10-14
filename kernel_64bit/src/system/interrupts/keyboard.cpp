#include "system/drivers/drivers.hpp"
#include "system/interrupts/interrupts.hpp"

#include "applications/shell/shell.hpp"
#include "system/sysfunc/command_history/history.hpp"

#include "applications/applications.hpp"

#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/gui/gui.hpp"
#include "system/vars/info_vars/info_vars.hpp"

#include "libs/qr_code/qr_code.hpp"
#include "libs/libc/libc.hpp"

#include "drivers/gpu/functions/windows_manager/window.hpp"

extern bool debug_mode;

namespace Keyboard
{

    char command_buffer[64];
    size_t cmd_idx = 0;

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

            if (scancode == 0x38)
            {
                alt_pressed = true;
                return;
            }

            if (scancode == 0xB8)
            {
                alt_pressed = false;
                return;
            }

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

            if (scancode == 0x49)
            {
                Mouse::scroll_up();
                return;
            }

            if (scancode == 0x51)
            {
                Mouse::scroll_down();
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
            if (rel == 0x38)
            {
                alt_pressed = false;
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

        if (scancode == 0x38)
        {
            alt_pressed = true;
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

        char c = 0;

        if (scancode == 0x2B)
        {
            if (shift_pressed && alt_pressed)
            {
                c = '|';
            }
            else if (shift_pressed)
            {
                c = '\\';
            }
            else
            {
                Mouse::right_click(true);
                return;
            }
        }
        else
        {
            c = translate_scancode(scancode);
        }

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
