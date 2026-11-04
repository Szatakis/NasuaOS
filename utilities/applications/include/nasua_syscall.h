#pragma once

#include <stdint.h>

enum nasua_syscall_number
{
    NASUA_SYSCALL_WRITE = 1,
    NASUA_SYSCALL_EXIT = 2
};

static inline int64_t nasua_syscall3(uint64_t number, uint64_t arg0,
                                     uint64_t arg1, uint64_t arg2)
{
    uint64_t result = number;
    asm volatile("int $0x80"
                 : "+a"(result)
                 : "D"(arg0), "S"(arg1), "d"(arg2)
                 : "rcx", "r11", "memory");
    return static_cast<int64_t>(result);
}

static inline int64_t nasua_user_write(const char* text, uint64_t length)
{
    return nasua_syscall3(NASUA_SYSCALL_WRITE, 1,
                          reinterpret_cast<uint64_t>(text), length);
}

[[noreturn]] static inline void nasua_user_exit(int status)
{
    nasua_syscall3(NASUA_SYSCALL_EXIT, static_cast<uint64_t>(status), 0, 0);
    for (;;)
    {
        asm volatile("pause");
    }
}
