#include "pr_map.hpp"

namespace Scheduler::PrMap
{
    static uint32_t task_ids[CAPACITY];
    static uint32_t task_count = 0;

    void init()
    {
        for (uint32_t index = 0; index < CAPACITY; ++index)
        {
            task_ids[index] = 0;
        }
        task_count = 0;
    }

    bool add(uint32_t task_id)
    {
        if (task_id == 0 || task_count == CAPACITY)
        {
            return false;
        }
        for (uint32_t index = 0; index < task_count; ++index)
        {
            if (task_ids[index] == task_id)
            {
                return false;
            }
        }
        task_ids[task_count++] = task_id;
        return true;
    }

    bool remove(uint32_t task_id)
    {
        for (uint32_t index = 0; index < task_count; ++index)
        {
            if (task_ids[index] != task_id)
            {
                continue;
            }

            for (uint32_t next_index = index + 1; next_index < task_count; ++next_index)
            {
                task_ids[next_index - 1] = task_ids[next_index];
            }
            task_ids[--task_count] = 0;
            return true;
        }
        return false;
    }

    uint32_t next(uint32_t current_task_id)
    {
        if (task_count == 0)
        {
            return 0;
        }

        for (uint32_t index = 0; index < task_count; ++index)
        {
            if (task_ids[index] == current_task_id)
            {
                return task_ids[(index + 1) % task_count];
            }
        }
        return task_ids[0];
    }

    uint32_t size()
    {
        return task_count;
    }
}