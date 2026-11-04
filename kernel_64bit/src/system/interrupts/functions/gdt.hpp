#pragma once

#include <stdint.h>

namespace Gdt
{
    inline constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
    inline constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
    inline constexpr uint16_t USER_DATA_SELECTOR = 0x1B;
    inline constexpr uint16_t USER_CODE_SELECTOR = 0x23;
    inline constexpr uint16_t TSS_SELECTOR = 0x28;

    void init();
    void set_kernel_stack(uint64_t stack_top);
}
