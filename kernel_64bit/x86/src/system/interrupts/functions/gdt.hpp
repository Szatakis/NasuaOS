#pragma once

#include <stdint.h>

namespace Gdt
{
    inline constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
    inline constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
    inline constexpr uint16_t RING1_CODE_SELECTOR = 0x19;
    inline constexpr uint16_t RING1_DATA_SELECTOR = 0x21;
    inline constexpr uint16_t RING2_CODE_SELECTOR = 0x2A;
    inline constexpr uint16_t RING2_DATA_SELECTOR = 0x32;
    inline constexpr uint16_t USER_DATA_SELECTOR = 0x3B;
    inline constexpr uint16_t USER_CODE_SELECTOR = 0x43;
    inline constexpr uint16_t TSS_SELECTOR = 0x48;

    void init();
    void set_kernel_stack(uint64_t stack_top);
}
