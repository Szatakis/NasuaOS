#pragma once

void history_add(const char* cmd);

void history_reset_nav();

const char* history_navigate_up();
const char* history_navigate_down();