#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Keyboard
{
    char scancode_to_ascii_normal(uint8_t scancode);
    char scancode_to_ascii_shift(uint8_t scancode);

    void keyboard_set_leds(uint8_t leds);

    extern bool shift_pressed;
    extern bool caps_lock;
    extern bool extended_scancode;
    extern bool shell_input_enabled;

    extern const char keymap[128];

#ifdef __x86_64__
    void print_sc(uint8_t scancode);
    void handle_keyboard();

    extern char command_buffer[64];
    extern size_t cmd_idx;
#endif

#ifdef __i386__
    extern char input_buffer[128];
    extern uint32_t input_pos;

    void keyboard_init();
    uint8_t read_scancode();
    void backspace();
    void read_line();
#endif
}
