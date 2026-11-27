#pragma once

bool auth_init();
bool auth_is_capturing_input();
void auth_handle_key(char key);
bool auth_handle_mouse_click(int x, int y);
void auth_draw_screen();
bool auth_execute_command(const char* command, const char* args);
const char* auth_current_username();
bool auth_current_user_is_administrator();
bool auth_check_path_permission(const char* path, bool write);
