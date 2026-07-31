#pragma once

#include <stdint.h>
#include <cstdint>
#include <cstddef>

void print_sc(uint8_t scancode); //Prints keyboard scancode to UART

void handle_keyboard(); // Handle keyboard inputs

extern bool shift_pressed;
extern bool extended_scancode;
extern bool shell_input_enabled;

extern char command_buffer[64];
extern size_t cmd_idx;