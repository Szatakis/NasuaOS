#pragma once

#include "drivers/gpu/driver.hpp"

#include "system/gui/vars/colors.hpp"

extern Gpu::Window_Manager::window_struct terminal;

extern bool active_terminal_redirect;

void terminal_write_char(char c);
void terminal_write_string(const char* str);
void terminal_clear_output();