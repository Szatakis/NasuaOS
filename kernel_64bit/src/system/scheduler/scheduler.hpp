#pragma once

#include <stdint.h>
#include "drivers/memory/user_address_space.hpp"

struct Registers;

namespace Scheduler
{
    using TaskEntry = void (*)(void*);

    inline constexpr uint32_t MAX_TASKS = 32;
    inline constexpr uint32_t TASK_STACK_SIZE = 16 * 1024;

    void init();
    uint32_t create_task(TaskEntry entry, void* argument = nullptr);
    uint32_t create_user_task(Memory::vmm::UserAddressSpace* address_space,
                              uint64_t entry, uint64_t user_stack_pointer,
                              uint32_t process_id = 0);
    const Memory::vmm::UserAddressSpace* current_user_address_space();
    void terminate_current_user_task(int exit_code = 0);
    void yield();
    void sleep_ticks(uint64_t ticks);
    [[noreturn]] void exit_current();
    uint32_t current_task_id();
    uint32_t task_count();
    Registers* on_interrupt(Registers* frame);
}