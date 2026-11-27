#pragma once

#include <stdint.h>

namespace Mouse
{
    #define CURSOR_W 12
    #define CURSOR_H 19

    extern int32_t x;
    extern int32_t y;

    extern bool mouse_connected;
    extern uint8_t mouse_buttons;

    void init();

    void handle_byte(uint8_t data);

    void update_position(int32_t dx, int32_t dy);

    void left_click(bool cmd_enter);
    void right_click(bool cmd_enter);
    void middle_click(bool cmd_enter);

    void back_click();
    void forward_click();

    void scroll_up();
    void scroll_down();

    extern const char arrow_cursor[CURSOR_H][CURSOR_W];
}