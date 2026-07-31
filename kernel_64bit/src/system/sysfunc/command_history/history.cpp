#include "history.hpp"

#include "libs/libc/libc.h"

constexpr size_t HISTORY_SIZE = 10;
constexpr size_t COMMAND_SIZE = 64;

static char command_history[HISTORY_SIZE][COMMAND_SIZE];
static size_t history_count = 0;
static size_t history_head = 0;
static int history_index = -1;

void history_add(const char* cmd) 
{
    if (cmd[0] == '\0') return;

    if (history_count > 0) {
        int last_pos = (history_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(command_history[last_pos], cmd) == 0) {
            history_index = -1;
            return;
        }
    }

    strcpy(command_history[history_head], cmd);
    history_head = (history_head + 1) % HISTORY_SIZE;

    if (history_count < HISTORY_SIZE) {
        history_count++;
    }

    history_index = -1;
}

void history_reset_nav() 
{
    history_index = -1;
}

const char* history_navigate_up() 
{
    if (history_count == 0) return nullptr;

    if (history_index == -1) {
        history_index = history_count - 1;
    } else if (history_index > 0) {
        history_index--;
    }

    int pos = history_head - history_count + history_index;
    while (pos < 0) pos += HISTORY_SIZE;
    
    return command_history[pos];
}

const char* history_navigate_down() 
{
    if (history_count == 0 || history_index == -1) return nullptr;

    if (history_index < (int)history_count - 1) {
        history_index++;
        int pos = history_head - history_count + history_index;
        while (pos < 0) pos += HISTORY_SIZE;
        return command_history[pos];
    } else {
        history_index = -1;
        return "";
    }
}