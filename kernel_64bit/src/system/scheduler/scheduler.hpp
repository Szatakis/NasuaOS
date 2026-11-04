#pragma once

#include <stdint.h>

struct Registers;

namespace Scheduler
{
    using TaskEntry = void (*)(void*);

    inline constexpr uint32_t MAX_TASKS = 32;
    inline constexpr uint32_t TASK_STACK_SIZE = 16 * 1024;

    void init();
    uint32_t create_task(TaskEntry entry, void* argument = nullptr);
    void yield();
    void sleep_ticks(uint64_t ticks);
    [[noreturn]] void exit_current();
    uint32_t current_task_id();
    uint32_t task_count();
    Registers* on_interrupt(Registers* frame);
}