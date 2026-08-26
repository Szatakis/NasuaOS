#pragma once

#include "drivers/gpu/driver.hpp"

#include "system/gui/vars/colors.hpp"

extern Gpu::Window_Manager::window_struct terminal;

extern bool active_terminal_redirect;

// When non-null, Gpu::print_char8 routes output through this callback instead
// of the on-screen text buffer. Used by NAPP applications (e.g. suaeid) to
// capture command output in their own integrated terminal.
extern void (*napp_print_redirect_cb)(char);

void terminal_write_char(char c);
void terminal_write_string(const char* str);
void terminal_clear_output();