#pragma once
#include <stdint.h>

enum LogLevel{INFO, WARN, ERROR, DEBUG};
LogLevel parse_log_level(const char* level);

void logger_init();
void log(LogLevel level, const char* subsystem, const char* message);
void print_logs();
void print_logs_level(LogLevel level);
void print_logs_subsystem(const char* subsystem);
void clear_logs();