#include "system/scheduler/scheduler.hpp"
#include "system/interrupts/functions/isr.hpp"
#include "system/sysfunc/logger/logger.hpp"

#include <cassert>
#include <cstdlib>

namespace Memory::heap
{
    static bool fail_allocation = false;

    void* kmalloc(size_t size)
    {
        return fail_allocation ? nullptr : std::malloc(size);
    }

    void kfree(void* ptr)
    {
        std::free(ptr);
    }
}

void log(LogLevel, const char*, const char*)
{
}

namespace Timer
{
    static uint64_t ticks = 0;

    uint64_t pit_get_ticks()
    {
        return ticks;
    }

    void sleep(uint64_t milliseconds)
    {
        ticks += milliseconds / 10;
    }
}

static void empty_task(void*)
{
}

int main()
{
    Scheduler::init();
    assert(Scheduler::current_task_id() == 1);
    assert(Scheduler::task_count() == 1);
    assert(Scheduler::create_task(nullptr) == 0);

    Memory::heap::fail_allocation = true;
    assert(Scheduler::create_task(empty_task) == 0);
    Memory::heap::fail_allocation = false;

    const uint32_t first = Scheduler::create_task(empty_task);
    const uint32_t second = Scheduler::create_task(empty_task);
    assert(first != 0 && second != 0 && first != second);
    assert(Scheduler::task_count() == 3);

    Registers kernel_frame{};
    Registers first_frame{};
    Registers second_frame{};

    Registers* first_context = Scheduler::on_interrupt(&kernel_frame);
    assert(first_context != &kernel_frame);
    assert(first_context->rip != 0);
    assert(first_context->cs == 0x28);
    assert((first_context->rflags & 0x2) != 0);
    assert(Scheduler::current_task_id() == first);
    assert(Scheduler::on_interrupt(&first_frame) != &first_frame);
    assert(Scheduler::current_task_id() == second);
    assert(Scheduler::on_interrupt(&second_frame) == &kernel_frame);
    assert(Scheduler::current_task_id() == 1);
    assert(Scheduler::on_interrupt(&kernel_frame) != &kernel_frame);
    assert(Scheduler::current_task_id() == first);

    Scheduler::sleep_ticks(2);
    assert(Scheduler::on_interrupt(&first_frame) != &first_frame);
    assert(Scheduler::current_task_id() == second);
    assert(Scheduler::on_interrupt(&second_frame) == &kernel_frame);
    assert(Scheduler::current_task_id() == 1);
    Timer::ticks++;
    assert(Scheduler::on_interrupt(&kernel_frame) != &kernel_frame);
    assert(Scheduler::current_task_id() == second);
    assert(Scheduler::on_interrupt(&second_frame) == &kernel_frame);
    assert(Scheduler::current_task_id() == 1);
    Timer::ticks++;
    assert(Scheduler::on_interrupt(&kernel_frame) != &kernel_frame);
    assert(Scheduler::current_task_id() == first);

    Timer::ticks = UINT64_MAX - 1;
    Scheduler::sleep_ticks(2);
    assert(Scheduler::on_interrupt(&first_frame) != &first_frame);
    assert(Scheduler::current_task_id() == second);
    assert(Scheduler::on_interrupt(&second_frame) == &kernel_frame);
    Timer::ticks = UINT64_MAX;
    assert(Scheduler::on_interrupt(&kernel_frame) != &kernel_frame);
    assert(Scheduler::current_task_id() == second);
    assert(Scheduler::on_interrupt(&second_frame) == &kernel_frame);
    Timer::ticks = 0;
    assert(Scheduler::on_interrupt(&kernel_frame) != &kernel_frame);
    assert(Scheduler::current_task_id() == first);

    for (uint32_t index = 0; index < Scheduler::MAX_TASKS - 3; ++index)
    {
        assert(Scheduler::create_task(empty_task) != 0);
    }
    assert(Scheduler::task_count() == Scheduler::MAX_TASKS);
    assert(Scheduler::create_task(empty_task) == 0);
}
