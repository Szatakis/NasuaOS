#pragma once

#include <stdint.h>
#include <cstdint>
#include <cstddef>

namespace Keyboard
{
    char scancode_to_ascii_normal(uint8_t scancode);
    char scancode_to_ascii_shift(uint8_t scancode);

    void keyboard_set_leds(uint8_t leds);
    void print_sc(uint8_t scancode);
    void handle_keyboard();

    extern bool shift_pressed;
    extern bool caps_lock;
    extern bool extended_scancode;
    extern bool shell_input_enabled;

    extern char command_buffer[64];
    extern size_t cmd_idx;
}