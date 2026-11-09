#ifndef GUI_H
#define GUI_H

#include <cstdint>
#include <cstddef>

extern uint32_t menu_w;
extern uint32_t menu_h;
extern uint32_t menu_x;
extern uint32_t menu_y;
extern const int panel_width;
extern bool menu_start_open;

extern size_t bar_h;
extern size_t bar_h_scaled;

void update_gui_state(int mouse_x, int mouse_y);

void open_start_menu();
void close_start_menu();
void draw_start_menu();
void scroll_start_menu(int direction, bool categories);

void image_init();
void draw_background();

void update_windows_gui();

#endif // GUI_H
