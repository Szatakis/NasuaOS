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
#include <napp.h>

#include "drivers/gpu/functions/windows_manager/window.hpp"

extern bool debug_mode;

namespace Keyboard
{

    char command_buffer[64];
    size_t cmd_idx = 0;
    static char napp_key_codes[256];
    static Gpu::Window_Manager::window_struct* napp_key_windows[256];

    static void send_napp_key_event(uint8_t index, char key, bool pressed)
    {
        if (pressed)
        {
            Gpu::Window_Manager::window_struct* window = Gpu::Window_Manager::active_window;
            if (window == nullptr ||
                !Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
            {
                return;
            }
            if (window->key_event == nullptr)
            {
                Gpu::Window_Manager::send_key_to_window(key);
                return;
            }

            napp_key_codes[index] = key;
            napp_key_windows[index] = window;
            Gpu::Window_Manager::send_key_event_to_window(window, key, true);
            return;
        }

        Gpu::Window_Manager::window_struct* window = napp_key_windows[index];
        if (window != nullptr)
        {
            Gpu::Window_Manager::send_key_event_to_window(window, napp_key_codes[index], false);
            napp_key_windows[index] = nullptr;
            napp_key_codes[index] = 0;
        }
    }

    void release_window_keys(Gpu::Window_Manager::window_struct* window)
    {
        if (window == nullptr)
        {
            return;
        }

        for (uint32_t index = 0; index < 256; ++index)
        {
            if (napp_key_windows[index] == window)
            {
                Gpu::Window_Manager::send_key_event_to_window(
                    window, napp_key_codes[index], false);
                napp_key_windows[index] = nullptr;
                napp_key_codes[index] = 0;
            }
        }
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
                const uint8_t key = scancode & 0x7F;
                send_napp_key_event((uint8_t)(0x80 | key), 0, false);
                if (key == 0x38)
                {
                    alt_pressed = false;
                }
                return;
            }

            if (scancode == 0x38)
            {
                alt_pressed = true;
                if (Gpu::Window_Manager::active_window &&
                    Gpu::Window_Manager::active_window->key_event != nullptr &&
                    Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    send_napp_key_event((uint8_t)(0x80 | scancode), NAPP_KEY_ALT, true);
                }
                return;
            }

            if (scancode == 0xB8)
            {
                alt_pressed = false;
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
                const char arrow_key = scancode == 0x48 ? NAPP_KEY_UP :
                                       scancode == 0x50 ? NAPP_KEY_DOWN :
                                       scancode == 0x4B ? NAPP_KEY_LEFT : NAPP_KEY_RIGHT;
                if (Gpu::Window_Manager::active_window &&
                    Gpu::Window_Manager::active_window->key_event != nullptr &&
                    Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    send_napp_key_event((uint8_t)(0x80 | scancode), arrow_key, true);
                }
                else if (shift_pressed)
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

            send_napp_key_event(rel, 0, false);
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
            if (Gpu::Window_Manager::active_window &&
                Gpu::Window_Manager::active_window->key_event != nullptr &&
                Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
            {
                send_napp_key_event(scancode, NAPP_KEY_SHIFT, true);
            }
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
            if (Gpu::Window_Manager::active_window &&
                Gpu::Window_Manager::active_window->key_event != nullptr &&
                Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
            {
                send_napp_key_event(scancode, NAPP_KEY_ALT, true);
            }
            return;
        }

        if (scancode == 0x0E)
        {
            if(Gpu::Window_Manager::active_window)
            {
                if(Gpu::Window_Manager::is_mouse_over_any_window(Mouse::x, Mouse::y))
                {
                    send_napp_key_event(scancode, '\b', true);
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
                    send_napp_key_event(scancode, '\n', true);
                }
            }
            return;
        }

        if (scancode == 0x3F)
        {
            if (Gpu::Window_Manager::active_window &&
                Gpu::Window_Manager::active_window->key_event != nullptr)
            {
                send_napp_key_event(scancode, NAPP_KEY_F5, true);
            }
            else
            {
                Gpu::Window_Manager::send_key_to_window(NAPP_KEY_F5);
            }
            return;
        }

        if (scancode == 0x01)
        {
            if (Gpu::Window_Manager::active_window &&
                Gpu::Window_Manager::active_window->key_event != nullptr)
            {
                send_napp_key_event(scancode, NAPP_KEY_ESCAPE, true);
            }
            return;
        }

        if (scancode == 0x1D)
        {
            if (Gpu::Window_Manager::active_window &&
                Gpu::Window_Manager::active_window->key_event != nullptr)
            {
                send_napp_key_event(scancode, NAPP_KEY_CTRL, true);
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
                    send_napp_key_event(scancode, c, true);
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
