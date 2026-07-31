#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#define CURSOR_W 12
#define CURSOR_H 19


extern int32_t mouse_x;
extern int32_t mouse_y;

extern const char arrow_cursor[CURSOR_H][CURSOR_W];

#endif