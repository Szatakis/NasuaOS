#ifndef GUI_H
#define GUI_H

#include <cstdint>
#include <cstddef>

extern const uint32_t menu_w;
extern const uint32_t menu_h;
extern uint32_t menu_x;
extern uint32_t menu_y;
extern const int panel_width;

extern size_t bar_h;
extern size_t bar_h_scaled;

extern const uint32_t background_width;
extern const uint32_t background_height;
extern const uint32_t background_data[];

void update_gui_state(int mouse_x, int mouse_y);

void open_start_menu();
void close_start_menu();
void draw_start_menu();

void image_init();
void draw_background();

void update_windows_gui();

#endif // GUI_H
