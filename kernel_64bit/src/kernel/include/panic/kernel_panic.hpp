#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include <stdint.h>

extern bool kernel_panicked;
extern uint32_t debug_qr_code[128 * 128];

void kernel_panic(const char* message, const char* error_code = "Unknown", const char* rip = "Unknown", const char* rsp = "Unknown", const char* fault_address = "Unknown", const char* pid = "Unknown");

#endif