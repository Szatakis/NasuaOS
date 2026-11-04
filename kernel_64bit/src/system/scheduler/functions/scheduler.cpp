#include "../scheduler.hpp"

#include "../pr_map/pr_map.hpp"
#include "system/interrupts/functions/gdt.hpp"
#include "system/interrupts/functions/isr.hpp"
#include "system/sysfunc/logger/logger.hpp"
#include "drivers/timer/driver.hpp"

#include <stddef.h>
#include <stdint.h>

namespace Memory::heap
{
    void* kmalloc(size_t size);
    void kfree(void* ptr);
}

namespace Scheduler
{
    enum class TaskState : uint8_t
    {
        Empty,
        Ready,
        Running,
        Sleeping,
        Dead
    };

    struct Task
    {
        uint32_t id;
        TaskState state;
        TaskEntry entry;
        void* argument;
        void* stack;
        Registers* context;
        uint64_t wake_tick;
    };

    static Task tasks[MAX_TASKS];
    static uint32_t running_task_id = 0;
    static uint32_t next_task_id = 2;
    static bool initialized = false;

    static uint64_t disable_interrupts()
    {
#ifdef SCHEDULER_TEST
        return 0;
#else
        uint64_t flags;
        asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        return flags;
#endif
    }

    static void restore_interrupts(uint64_t flags)
    {
#ifndef SCHEDULER_TEST
        asm volatile("pushq %0; popfq" : : "r"(flags) : "memory");
#else
        (void)flags;
#endif
    }

    static Task* find_task(uint32_t id)
    {
        for (uint32_t index = 0; index < MAX_TASKS; ++index)
        {
            if (tasks[index].id == id && tasks[index].state != TaskState::Empty)
            {
                return &tasks[index];
            }
        }
        return nullptr;
    }

    static void reap_dead_tasks()
    {
        for (uint32_t index = 1; index < MAX_TASKS; ++index)
        {
            Task& task = tasks[index];
            if (task.state != TaskState::Dead || task.id == running_task_id)
            {
                continue;
            }

            Memory::heap::kfree(task.stack);
            task = {};
        }
    }

    static bool tick_deadline_reached(uint64_t now, uint64_t deadline)
    {
        return now - deadline < (1ULL << 63);
    }

    static void wake_sleeping_tasks()
    {
        const uint64_t now = Timer::pit_get_ticks();
        for (uint32_t index = 1; index < MAX_TASKS; ++index)
        {
            Task& task = tasks[index];
            if (task.state == TaskState::Sleeping && tick_deadline_reached(now, task.wake_tick))
            {
                task.state = TaskState::Ready;
            }
        }
    }

    static Task* next_ready_task(uint32_t current_id)
    {
        const uint32_t count = PrMap::size();
        uint32_t candidate_id = current_id;
        for (uint32_t index = 0; index < count; ++index)
        {
            candidate_id = PrMap::next(candidate_id);
            Task* candidate = find_task(candidate_id);
            if (candidate != nullptr && candidate->state == TaskState::Ready)
            {
                return candidate;
            }
        }
        return nullptr;
    }

    static void task_trampoline(Task* task)
    {
        task->entry(task->argument);
        exit_current();
    }

    void init()
    {
        const uint64_t flags = disable_interrupts();
        for (uint32_t index = 0; index < MAX_TASKS; ++index)
        {
            tasks[index] = {};
        }
        PrMap::init();

        tasks[0].id = 1;
        tasks[0].state = TaskState::Running;
        running_task_id = tasks[0].id;
        next_task_id = 2;
        initialized = PrMap::add(tasks[0].id);
        restore_interrupts(flags);

        if (!initialized)
        {
            log(ERROR, "SCHEDULER", "Failed to register the kernel task");
        }
    }

    uint32_t create_task(TaskEntry entry, void* argument)
    {
        if (!initialized || entry == nullptr)
        {
            log(ERROR, "SCHEDULER", "Cannot create a task before initialization or without an entry point");
            return 0;
        }

        const uint64_t flags = disable_interrupts();
        reap_dead_tasks();

        Task* slot = nullptr;
        for (uint32_t index = 1; index < MAX_TASKS; ++index)
        {
            if (tasks[index].state == TaskState::Empty)
            {
                slot = &tasks[index];
                break;
            }
        }
        if (slot == nullptr)
        {
            restore_interrupts(flags);
            log(ERROR, "SCHEDULER", "Task table is full");
            return 0;
        }

        void* stack = Memory::heap::kmalloc(TASK_STACK_SIZE);
        if (stack == nullptr)
        {
            restore_interrupts(flags);
            log(ERROR, "SCHEDULER", "Unable to allocate a task stack");
            return 0;
        }

        uint32_t id = next_task_id++;
        if (next_task_id < 2)
        {
            next_task_id = 2;
        }
        while (id == 0 || find_task(id) != nullptr)
        {
            id = next_task_id++;
            if (next_task_id < 2)
            {
                next_task_id = 2;
            }
        }

        slot->id = id;
        slot->state = TaskState::Ready;
        slot->entry = entry;
        slot->argument = argument;
        slot->stack = stack;

        uintptr_t stack_end = reinterpret_cast<uintptr_t>(stack) + TASK_STACK_SIZE;
        uintptr_t stack_top = (stack_end & ~static_cast<uintptr_t>(0xF)) - sizeof(uint64_t);
        uintptr_t frame_address = stack_top - offsetof(Registers, rsp);
        Registers* frame = reinterpret_cast<Registers*>(frame_address);
        for (uint32_t index = 0; index < offsetof(Registers, rsp) / sizeof(uint64_t); ++index)
        {
            reinterpret_cast<uint64_t*>(frame)[index] = 0;
        }
        frame->rdi = reinterpret_cast<uint64_t>(slot);
        frame->rip = reinterpret_cast<uint64_t>(task_trampoline);
        frame->cs = Gdt::KERNEL_CODE_SELECTOR;
        frame->rflags = 0x202;
        slot->context = frame;

        if (!PrMap::add(id))
        {
            Memory::heap::kfree(stack);
            *slot = {};
            restore_interrupts(flags);
            log(ERROR, "SCHEDULER", "Unable to register the new task");
            return 0;
        }

        restore_interrupts(flags);
        return id;
    }

    void yield()
    {
#ifndef SCHEDULER_TEST
        asm volatile("int $0x81" : : : "memory");
#endif
    }

    void sleep_ticks(uint64_t ticks)
    {
        if (ticks == 0)
        {
            yield();
            return;
        }

        const uint64_t flags = disable_interrupts();
        Task* current = find_task(running_task_id);
        if (current == nullptr || current->id == 1)
        {
            restore_interrupts(flags);
            const uint64_t max_ticks = UINT64_MAX / 10;
            Timer::sleep(ticks > max_ticks ? UINT64_MAX : ticks * 10);
            return;
        }

        current->wake_tick = Timer::pit_get_ticks() + ticks;
        current->state = TaskState::Sleeping;
#ifndef SCHEDULER_TEST
        asm volatile("int $0x81" : : : "memory");
#endif
        restore_interrupts(flags);
    }

    [[noreturn]] void exit_current()
    {
        const uint64_t flags = disable_interrupts();
        Task* current = find_task(running_task_id);
        if (current != nullptr && current->id != 1)
        {
            current->state = TaskState::Dead;
            PrMap::remove(current->id);
        }
        restore_interrupts(flags);
        yield();

        for (;;)
        {
            asm volatile("hlt");
        }
    }

    Registers* on_interrupt(Registers* frame)
    {
        if (!initialized || frame == nullptr)
        {
            return frame;
        }

        Task* current = find_task(running_task_id);
        if (current != nullptr && current->state == TaskState::Running)
        {
            current->context = frame;
            current->state = TaskState::Ready;
        }

        reap_dead_tasks();
        wake_sleeping_tasks();

        Task* next = next_ready_task(running_task_id);
        if (next == nullptr)
        {
            return frame;
        }

        running_task_id = next->id;
        next->state = TaskState::Running;
        if (next->stack != nullptr)
        {
            const uintptr_t stack_top =
                reinterpret_cast<uintptr_t>(next->stack) + TASK_STACK_SIZE;
            Gdt::set_kernel_stack(stack_top);
        }
        return next->context != nullptr ? next->context : frame;
    }

    uint32_t current_task_id()
    {
        return running_task_id;
    }

    uint32_t task_count()
    {
        return PrMap::size();
    }
}