#pragma once

#include "system/drivers/gpu/driver.hpp"

extern window_struct calculator;

void calculator_mouse(window_struct* win, int mx, int my);
void calculator_key(window_struct* win, char key);
void draw_calculator(window_struct* win);