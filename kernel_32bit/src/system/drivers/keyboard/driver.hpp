#include <stdint.h>

extern const char keymap[128];

void keyboard_init();
uint8_t read_scancode();
void backspace();
void read_line();