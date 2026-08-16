#include "../driver.hpp"

#include "system/drivers/drivers.hpp"

#include "applications/shell/commands.hpp"
#include "system/sysfunc/command_history/history.hpp"

#include "applications/applications.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/vars/info_vars/info_vars.hpp"
#include "system/gui/gui.hpp"

#include "libs/asm/asm.hpp"

int32_t mouse_x = 200;
int32_t mouse_y = 145;

bool mouse_connected = false;

uint8_t mouse_buttons = 0;
static uint8_t old_mouse_buttons = 0;

static uint8_t mouse_packet[4];
static uint8_t mouse_packet_index = 0;

static uint8_t mouse_id = 0;

extern window_struct* apps[];


// Wait for PS/2 controller to be ready for writing
static void mouse_wait_write()
{
    uint32_t timeout = 100000;

    while (timeout--)
    {
        if (!(inb(0x64) & 0x02))
            return;
    }
}


// Wait for data from PS/2 controller
static void mouse_wait_read()
{
    uint32_t timeout = 100000;

    while (timeout--)
    {
        if (inb(0x64) & 0x01)
            return;
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


// Initialize PS/2 mouse
void mouse_init()
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

    // Write configuration
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

    // Mouse ID
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


    // Get mouse ID
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

        Uart::puts("[MOUSE] Could not detect wheel\n");
    }


    // Enable data reporting
    mouse_write(0xF4);

    ack = mouse_read();

    if (ack != 0xFA)
    {
        Uart::puts("[MOUSE] Enable failed\n");
        return;
    }


    // Done
    mouse_packet_index = 0;
    mouse_buttons = 0;
    old_mouse_buttons = 0;

    mouse_connected = true;

    Uart::puts("[MOUSE] PS/2 mouse connected\n");
}


// Update cursor position
void mouse_update_position(int32_t dx, int32_t dy)
{
    mouse_x += dx;
    mouse_y -= dy;

    // Left boundary
    if (mouse_x < 0)
    {
        mouse_x = 0;
    }
    // Top boundary
    if (mouse_y < 0)
    {
        mouse_y = 0;
    }


    if (fb)
    {
        // Right boundary
        if (mouse_x >= (int32_t)fb->width)
        {
            mouse_x = fb->width - 1;
        }

        // Bottom boundary
        if (mouse_y >= (int32_t)fb->height)
        {
            mouse_y = fb->height - 1;
        }
    }
}


// Handle mouse packet byte
void mouse_handle_byte(uint8_t data)
{
    // First byte synchronization
    if (mouse_packet_index == 0)
    {
        // Bit 3 of first byte must always be 1
        if (!(data & 0x08))
        {
            return;
        }
    }


    mouse_packet[mouse_packet_index++] = data;


    // Normal mouse = 3 bytes
    // IntelliMouse = 4 bytes
    uint8_t packet_size = (mouse_id == 0x03) ? 4 : 3;


    if (mouse_packet_index < packet_size)
    {
        return;
    }


    // Packet complete
    mouse_packet_index = 0;
    uint8_t flags = mouse_packet[0];


    // Overflow
    if (flags & 0x40)
    {
        return;
    }

    if (flags & 0x80)
    {
        return;
    }


    // Movement
    int8_t dx = (int8_t)mouse_packet[1];
    int8_t dy = (int8_t)mouse_packet[2];


    // Update position BEFORE handling click
    mouse_update_position(dx, dy);


    // Buttons
    uint8_t new_buttons = flags & 0x07;


    // LEFT BUTTON
    if ((new_buttons & 0x01) &&
        !(old_mouse_buttons & 0x01))
    {
        handle_left_click(false);
    }


    // RIGHT BUTTON
    if ((new_buttons & 0x02) &&
        !(old_mouse_buttons & 0x02))
    {
        handle_right_click(false);
    }


    // MIDDLE BUTTON
    if ((new_buttons & 0x04) &&
        !(old_mouse_buttons & 0x04))
    {
        handle_middle_click(false);
    }


    // SCROLL
    if (mouse_id == 0x03)
    {
        // Wheel is signed.
        int8_t wheel = (int8_t)mouse_packet[3];

        if (wheel > 0)
        {
            mouse_scroll_up();
        }
        else if (wheel < 0)
        {
            mouse_scroll_down();
        }
    }


    // Save button state
    old_mouse_buttons = new_buttons;
    mouse_buttons = new_buttons;
}


// LEFT CLICK
void handle_left_click(bool cmd_enter)
{
    // START MENU BUTTON
    if (is_mouse_over_start(mouse_x, mouse_y))
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

    if (is_menu_start_open)
    {
        const int start_menu_left = (int)menu_x;
        const int start_menu_top = (int)menu_y;
        const int start_menu_right = (int)(menu_x + menu_w);
        const int start_menu_bottom = (int)(menu_y + menu_h);

        if (mouse_x < start_menu_left || mouse_x >= start_menu_right || mouse_y < start_menu_top || mouse_y >= start_menu_bottom)
        {
            close_start_menu();
        }
    }


    // TASKBAR
    if (is_mouse_over_taskbar(mouse_x, mouse_y))
    {
        handle_window_mouse_click(mouse_x, mouse_y);

        return;
    }


    // WINDOW
    if (is_mouse_over_any_window(mouse_x, mouse_y))
    {
        handle_window_mouse_click(mouse_x, mouse_y);

        return;
    }


    // START MENU
    if (is_menu_start_open)
    {
        // Applications
        for (int i = 0; i < 4; i++)
        {
            if (is_mouse_over_icon(mouse_x, mouse_y, icons_start_x + 50, icons_start_y + i * icons_offset, 250, 32))
            {
                apps[i]->visible = true;
                apps[i]->id = current_id++;

                register_window(apps[i]);

                return;
            }
        }


        // SHUTDOWN
        if (is_mouse_over_icon(mouse_x, mouse_y, icons_start_x, menu_y + menu_h - 80, 32, 32))
        {
            acpi_shutdown();

            return;
        }


        // REBOOT
        if (is_mouse_over_icon(mouse_x, mouse_y, icons_start_x, menu_y + menu_h - 40, 32, 32))
        {
            acpi_reboot();

            return;
        }
    }


    // SHELL ENTER
    else if (shell_input_enabled && cmd_enter)
    {
        command_buffer[cmd_idx] = '\0';

        history_add(command_buffer);
        history_reset_nav();

        execute_command(command_buffer);

        cmd_idx = 0;
        command_buffer[0] = '\0';
    }
}


// RIGHT CLICK
void handle_right_click(bool cmd_enter)
{
    (void)cmd_enter;
}


// MIDDLE CLICK
void handle_middle_click(bool cmd_enter)
{
    (void)cmd_enter;
}


// Mouse scroll up
void mouse_scroll_up()
{
    
}


// Mouse scroll down
void mouse_scroll_down()
{

}