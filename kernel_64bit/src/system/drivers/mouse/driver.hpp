#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#define CURSOR_W 12
#define CURSOR_H 19

extern int32_t mouse_x;
extern int32_t mouse_y;

extern bool mouse_connected;

extern uint8_t mouse_buttons;

void mouse_init();
void mouse_handle_byte(uint8_t data);
void mouse_update_position(int32_t dx, int32_t dy);

void handle_left_click(bool cmd_enter);
void handle_right_click(bool cmd_enter);
void handle_middle_click(bool cmd_enter);

void mouse_scroll_up();
void mouse_scroll_down();

const char arrow_cursor[CURSOR_H][CURSOR_W] = {
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

#endif