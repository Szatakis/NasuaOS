#include "../driver.hpp"

#include "system/drivers/drivers.hpp"

#include "applications/shell/commands.hpp"
#include "system/sysfunc/command_history/history.hpp"
#include "drivers/gpu/functions/windows_manager/window.hpp"

#include "applications/applications.hpp"
#include "system/applications/napp/napp.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/vars/info_vars/info_vars.hpp"
#include "system/gui/gui.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"

namespace Mouse
{
    int32_t x = 200;
    int32_t y = 145;

    bool mouse_connected = false;

    uint8_t mouse_buttons = 0;
    static uint8_t old_mouse_buttons = 0;

    static uint8_t mouse_packet[4];
    static uint8_t mouse_packet_index = 0;

    static uint8_t mouse_id = 0;


    // PS/2 CONTROLLER
    // Wait for PS/2 controller to be ready for writing
    static void mouse_wait_write()
    {
        uint32_t timeout = 100000;

        while (timeout--)
        {
            if (!(inb(0x64) & 0x02))
            {
                return;
            }
        }
    }


    // Wait for data from PS/2 controller
    static void mouse_wait_read()
    {
        uint32_t timeout = 100000;

        while (timeout--)
        {
            if (inb(0x64) & 0x01)
            {
                return;
            }
        }
    }


    // Send command to PS/2 controller
    static void ps2_write(uint8_t value)
    {
        mouse_wait_write();
        outb(0x64, value);
    }


    // Send command directly to mouse
    static void mouse_write(uint8_t value)
    {
        // Tell controller that next byte goes to mouse
        ps2_write(0xD4);

        mouse_wait_write();
        outb(0x60, value);
    }


    // Read byte from mouse/controller
    static uint8_t mouse_read()
    {
        mouse_wait_read();
        return inb(0x60);
    }


    // MOUSE INITIALIZATION
    void init()
    {
        mouse_connected = false;
        mouse_id = 0;
        mouse_packet_index = 0;
        old_mouse_buttons = 0;
        mouse_buttons = 0;

        // Enable second PS/2 port
        ps2_write(0xA8);


        // Read controller configuration
        ps2_write(0x20);

        uint8_t config = mouse_read();


        // Enable IRQ12
        config |= 0x02;

        // Enable second PS/2 clock
        config &= ~0x20;


        // Write controller configuration
        ps2_write(0x60);

        mouse_wait_write();
        outb(0x60, config);

        // Reset mouse
        mouse_write(0xFF);

        uint8_t ack = mouse_read();

        if (ack != 0xFA)
        {
            Uart::puts("[MOUSE] No ACK\n");
            return;
        }


        // BAT
        uint8_t bat = mouse_read();

        if (bat != 0xAA)
        {
            Uart::puts("[MOUSE] BAT failed\n");
            return;
        }


        // Initial mouse ID
        uint8_t id = mouse_read();

        if (id != 0x00)
        {
            Uart::puts("[MOUSE] Unknown mouse ID: ");

            char hex[3];
            const char* digits = "0123456789ABCDEF";

            hex[0] = digits[(id >> 4) & 0x0F];
            hex[1] = digits[id & 0x0F];
            hex[2] = '\0';

            Uart::puts(hex);
            Uart::puts("\n");

            return;
        }


        mouse_write(0xF3);
        ack = mouse_read();

        if (ack == 0xFA)
        {
            mouse_write(200);
            ack = mouse_read();
        }


        mouse_write(0xF3);
        ack = mouse_read();

        if (ack == 0xFA)
        {
            mouse_write(200);
            ack = mouse_read();
        }


        mouse_write(0xF3);
        ack = mouse_read();

        if (ack == 0xFA)
        {
            mouse_write(80);
            ack = mouse_read();
        }


        // Get mouse ID
        mouse_write(0xF2);

        ack = mouse_read();

        if (ack == 0xFA)
        {
            mouse_id = mouse_read();

            if (mouse_id == 0x04)
            {
                Uart::puts("[MOUSE] IntelliMouse Explorer 5-button enabled\n");
            }
        }

        if (mouse_id != 0x04)
        {
            mouse_write(0xF3);
            ack = mouse_read();

            if (ack == 0xFA)
            {
                mouse_write(200);
                ack = mouse_read();
            }


            mouse_write(0xF3);
            ack = mouse_read();

            if (ack == 0xFA)
            {
                mouse_write(100);
                ack = mouse_read();
            }


            mouse_write(0xF3);
            ack = mouse_read();

            if (ack == 0xFA)
            {
                mouse_write(80);
                ack = mouse_read();
            }


            // Get ID
            mouse_write(0xF2);

            ack = mouse_read();

            if (ack == 0xFA)
            {
                mouse_id = mouse_read();

                if (mouse_id == 0x03)
                {
                    Uart::puts("[MOUSE] IntelliMouse wheel enabled\n");
                }
                else
                {
                    mouse_id = 0;

                    Uart::puts("[MOUSE] Standard PS/2 mouse\n");
                }
            }
            else
            {
                mouse_id = 0;

                Uart::puts("[MOUSE] Could not detect mouse ID\n");
            }
        }


        // Enable data reporting
        mouse_write(0xF4);

        ack = mouse_read();

        if (ack != 0xFA)
        {
            Uart::puts("[MOUSE] Enable failed\n");
            return;
        }

        mouse_packet_index = 0;
        mouse_buttons = 0;
        old_mouse_buttons = 0;

        mouse_connected = true;

        Uart::puts("[MOUSE] PS/2 mouse connected\n");
    }


    // CURSOR POSITION
    void update_position(int32_t dx, int32_t dy)
    {
        x += dx;
        y -= dy;

        // Left boundary
        if (x < 0)
        {
            x = 0;
        }

        // Top boundary
        if (y < 0)
        {
            y = 0;
        }

        if (Gpu::fb)
        {
            // Right boundary
            if (x >= (int32_t)Gpu::fb->width)
            {
                x = Gpu::fb->width - 1;
            }

            // Bottom boundary
            if (y >= (int32_t)Gpu::fb->height)
            {
                y = Gpu::fb->height - 1;
            }
        }
    }


    // MOUSE PACKET
    void handle_byte(uint8_t data)
    {
        // First byte synchronization
        if (mouse_packet_index == 0)
        {
            // Bit 3 must always be 1
            if (!(data & 0x08))
            {
                return;
            }
        }


        mouse_packet[mouse_packet_index++] = data;


        // Packet size
        uint8_t packet_size = (mouse_id == 0x03 || mouse_id == 0x04) ? 4 : 3;

        if (mouse_packet_index < packet_size)
        {
            return;
        }


        // Packet complete
        mouse_packet_index = 0;

        uint8_t flags = mouse_packet[0];


        // Overflow
        if (flags & 0x40 || flags & 0x80)
        {
            return;
        }


        // Movement
        int8_t dx = (int8_t)mouse_packet[1];
        int8_t dy = (int8_t)mouse_packet[2];

        update_position(dx, dy);

        uint8_t new_buttons = flags & 0x07;


        // IntelliMouse Explorer buttons
        if (mouse_id == 0x04)
        {
            new_buttons |= (mouse_packet[3] >> 1) & 0x18;
        }


        // LEFT BUTTON
        if ((new_buttons & 0x01) && !(old_mouse_buttons & 0x01))
        {
            left_click(false);
        }


        // RIGHT BUTTON
        if ((new_buttons & 0x02) && !(old_mouse_buttons & 0x02))
        {
            right_click(false);
        }


        // MIDDLE BUTTON
        if ((new_buttons & 0x04) && !(old_mouse_buttons & 0x04))
        {
            middle_click(false);
        }


        // BUTTON 4 - BACK
        if ((new_buttons & 0x08) && !(old_mouse_buttons & 0x08))
        {
            back_click();
        }


        // BUTTON 5 - FORWARD
        if ((new_buttons & 0x10) && !(old_mouse_buttons & 0x10))
        {
            forward_click();
        }


        // WHEEL
        if (mouse_id == 0x03 || mouse_id == 0x04)
        {
            //Low 4 bits contain signed wheel movement.
            int8_t wheel = (int8_t)(mouse_packet[3] & 0x0F);

            // Sign extend 4-bit wheel value
            if (wheel & 0x08)
            {
                wheel |= 0xF0;
            }


            if (wheel > 0)
            {
                scroll_up();
            }
            else if (wheel < 0)
            {
                scroll_down();
            }
        }


        // Save button state
        old_mouse_buttons = new_buttons;
        mouse_buttons = new_buttons;
    }


    // LEFT CLICK
    void left_click(bool cmd_enter)
    {
        // START MENU BUTTON
        if (is_mouse_over_start(x, y))
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

        if (menu_start_open)
        {
            const int start_menu_left = (int)menu_x;
            const int start_menu_top = (int)menu_y;
            const int start_menu_right = (int)(menu_x + menu_w);
            const int start_menu_bottom = (int)(menu_y + menu_h);

            if (x < start_menu_left || x >= start_menu_right || y < start_menu_top || y >= start_menu_bottom)
            {
                close_start_menu();
            }
        }


        // TASKBAR
        if (Gpu::Window_Manager::is_mouse_over_taskbar(x, y))
        {
            Gpu::Window_Manager::handle_window_mouse_click(x, y, 0);

            return;
        }


        // WINDOW
        if (Gpu::Window_Manager::is_mouse_over_any_window(x, y))
        {
            Gpu::Window_Manager::handle_window_mouse_click(x, y, 0);

            return;
        }


        // START MENU
        if (menu_start_open)
        {
            // Kernel applications
            for (int i = 0; i < kernel_app_count; i++)
            {
                if (is_mouse_over_icon(x, y, icons_start_x + 50, icons_start_y + i * icons_offset, 250, 32))
                {
                    Gpu::Window_Manager::apps[i]->visible = true;
                    Gpu::Window_Manager::apps[i]->id = current_id++;

                    Gpu::Window_Manager::register_window(Gpu::Window_Manager::apps[i]);

                    return;
                }
            }


            // Applications shipped as .napp packages in the rootfs
            for (int i = 0; i < start_menu_napp_count; i++)
            {
                int row = kernel_app_count + i;

                if (is_mouse_over_icon(x, y, icons_start_x + 50, icons_start_y + row * icons_offset, 250, 32))
                {
                    napp_run(start_menu_napps[i], nullptr);

                    return;
                }
            }


            // SHUTDOWN
            if (is_mouse_over_icon(x, y, icons_start_x, menu_y + menu_h - 80, 32, 32))
            {
                Acpi::shutdown();

                return;
            }


            // REBOOT
            if (is_mouse_over_icon(x, y, icons_start_x, menu_y + menu_h - 40, 32, 32))
            {
                Acpi::reboot();

                return;
            }
        }


        // SHELL ENTER
        else if (Keyboard::shell_input_enabled && cmd_enter)
        {
            Keyboard::command_buffer[Keyboard::cmd_idx] = '\0';

            history_add(Keyboard::command_buffer);
            history_reset_nav();

            execute_command(Keyboard::command_buffer);

            Keyboard::cmd_idx = 0;
            Keyboard::command_buffer[0] = '\0';
        }
    }


    // RIGHT CLICK
    void right_click(bool cmd_enter)
    {
        (void)cmd_enter;

        if (menu_start_open)
        {
            // Clicking outside the start menu closes it
            const int start_menu_left = (int)menu_x;
            const int start_menu_top = (int)menu_y;
            const int start_menu_right = (int)(menu_x + menu_w);
            const int start_menu_bottom = (int)(menu_y + menu_h);

            if (x < start_menu_left || x >= start_menu_right || y < start_menu_top || y >= start_menu_bottom)
            {
                close_start_menu();
            }

            return;
        }

        // Forward right-click to window under cursor
        if (Gpu::Window_Manager::is_mouse_over_any_window(x, y))
        {
            Gpu::Window_Manager::handle_window_mouse_click(x, y, 1);
            return;
        }
    }


    // MIDDLE CLICK
    void middle_click(bool cmd_enter)
    {
        (void)cmd_enter;
    }


    // Mouse scroll up
    void scroll_up()
    {

    }


    // Mouse scroll down
    void scroll_down()
    {

    }


    // BACK CLICK
    void back_click()
    {

    }


    // FORWARD CLICK
    void forward_click()
    {

    }

    const char arrow_cursor[CURSOR_H][CURSOR_W] =
    {
        {'W','.','.','.','.','.','.','.','.','.','.','.'},
        {'W','W','.','.','.','.','.','.','.','.','.','.'},
        {'W','B','W','.','.','.','.','.','.','.','.','.'},
        {'W','B','B','W','.','.','.','.','.','.','.','.'},
        {'W','B','B','B','W','.','.','.','.','.','.','.'},
        {'W','B','B','B','B','W','.','.','.','.','.','.'},
        {'W','B','B','B','B','B','W','.','.','.','.','.'},
        {'W','B','B','B','B','B','B','W','.','.','.','.'},
        {'W','B','B','B','B','B','B','B','W','.','.','.'},
        {'W','B','B','B','B','B','B','B','B','W','.','.'},
        {'W','B','B','B','B','B','W','W','W','W','W','.'},
        {'W','B','B','W','B','B','W','.','.','.','.','.'},
        {'W','B','W','.','W','B','B','W','.','.','.','.'},
        {'W','W','.','.','W','B','B','W','.','.','.','.'},
        {'.','.','.','.','.','W','B','B','W','.','.','.'},
        {'.','.','.','.','.','W','B','B','W','.','.','.'},
        {'.','.','.','.','.','.','W','B','B','W','.','.'},
        {'.','.','.','.','.','.','W','B','B','W','.','.'},
        {'.','.','.','.','.','.','.','W','W','W','.','.'}
    };
}
