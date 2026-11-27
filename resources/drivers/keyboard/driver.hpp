#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Gpu::Window_Manager
{
    struct window_struct;
}

namespace Keyboard
{
    char scancode_to_ascii_normal(uint8_t scancode);
    char scancode_to_ascii_shift(uint8_t scancode);
    char translate_scancode(uint8_t scancode);

    void keyboard_set_leds(uint8_t leds);

    extern bool shift_pressed;
    extern bool caps_lock;
    extern bool extended_scancode;
    extern bool shell_input_enabled;
    extern bool alt_pressed;

    void print_sc(uint8_t scancode);
    void handle_keyboard();
    extern char command_buffer[64];
    extern size_t cmd_idx;

    extern char input_buffer[128];
    extern uint32_t input_pos;
    void keyboard_init();
    uint8_t read_scancode();
    void backspace();
    void read_line();
    void release_window_keys(Gpu::Window_Manager::window_struct* window);
}
