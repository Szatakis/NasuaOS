#pragma once
#include <cstdint>
#include "libs/asm/asm.h"

class Uart {
private:
    static constexpr uint16_t COM1_PORT = 0x3F8;
    static constexpr uint16_t COM2_PORT = 0x2F8;
    static constexpr uint16_t COM3_PORT = 0x3E8;
    static constexpr uint16_t COM4_PORT = 0x2E8;

    static inline bool is_transmit_empty() 
    {
        return (inb(COM1_PORT + 5) & 0x20) != 0;
    }

public:
    Uart() = delete;

    static void init();
    static void putc(char c);
    static void puts(const char* str);
    
    static void puthex(uint64_t val);
    static void putdec(uint64_t val);
};
