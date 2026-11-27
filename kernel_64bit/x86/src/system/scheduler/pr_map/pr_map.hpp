#pragma once

#include <stdint.h>

namespace Scheduler::PrMap
{
    inline constexpr uint32_t CAPACITY = 32;

    void init();
    bool add(uint32_t task_id);
    bool remove(uint32_t task_id);
    uint32_t next(uint32_t current_task_id);
    uint32_t size();
}