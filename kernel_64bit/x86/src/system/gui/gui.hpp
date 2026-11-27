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

void draw_desktop_icons();
void draw_desktop_overlay();
bool desktop_has_keyboard_focus();
bool desktop_handle_key(char key);
bool desktop_handle_mouse_click(int mouse_x, int mouse_y);
void desktop_handle_right_click(int mouse_x, int mouse_y);
void desktop_dismiss_overlays();

void update_windows_gui();

#endif // GUI_H
