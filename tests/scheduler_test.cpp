#include "system/scheduler/scheduler.hpp"
#include "system/interrupts/functions/gdt.hpp"
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

namespace Process
{
    static uint32_t finished_pid = 0;
    static int finished_exit_code = 0;

    void finish(uint32_t pid, int exit_code)
    {
        finished_pid = pid;
        finished_exit_code = exit_code;
    }
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

namespace Gdt
{
    void set_kernel_stack(uint64_t)
    {
    }
}

namespace Memory::vmm
{
    bool is_user_range(const UserAddressSpace* space, uint64_t address, uint64_t length,
                       bool, bool)
    {
        return space != nullptr && space->valid && length != 0 &&
            address >= space->base && address < space->base + 0x40000;
    }

    bool activate_address_space(UserAddressSpace* space)
    {
        return space != nullptr && space->valid;
    }

    bool sync_kernel_mappings(UserAddressSpace* space)
    {
        return space != nullptr && space->valid;
    }

    void activate_kernel_address_space()
    {
    }

    bool destroy_user_address_space(UserAddressSpace* space)
    {
        if (space == nullptr || !space->valid)
        {
            return false;
        }
        *space = {};
        return true;
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

    Memory::vmm::UserAddressSpace user_space{1, 0x8000000000ULL, 1, true};
    const uint32_t user_task = Scheduler::create_user_task(
        &user_space, 0x8000001000ULL, 0x8000003000ULL, 42);
    assert(user_task != 0);
    assert(!user_space.valid);

    Registers kernel_frame{};
    Registers* user_context = Scheduler::on_interrupt(&kernel_frame);
    assert(user_context != &kernel_frame);
    assert(user_context->cs == Gdt::USER_CODE_SELECTOR);
    assert(user_context->ss == Gdt::USER_DATA_SELECTOR);
    assert(user_context->rip == 0x8000001000ULL);
    assert(user_context->rsp == 0x8000002FF8ULL);
    assert(Scheduler::current_task_id() == user_task);
    assert(Scheduler::current_user_address_space() != nullptr);

    Scheduler::terminate_current_user_task(7);
    assert(Process::finished_pid == 42);
    assert(Process::finished_exit_code == 7);
    Registers user_frame{};
    assert(Scheduler::on_interrupt(&user_frame) == &kernel_frame);
    assert(Scheduler::current_task_id() == 1);
    assert(Scheduler::on_interrupt(&kernel_frame) == &kernel_frame);
    assert(Scheduler::task_count() == 1);
    assert(Scheduler::current_user_address_space() == nullptr);

    Memory::heap::fail_allocation = true;
    assert(Scheduler::create_task(empty_task) == 0);
    Memory::heap::fail_allocation = false;

    const uint32_t first = Scheduler::create_task(empty_task);
    const uint32_t second = Scheduler::create_task(empty_task);
    assert(first != 0 && second != 0 && first != second);
    assert(Scheduler::task_count() == 3);

    Registers first_frame{};
    Registers second_frame{};

    Registers* first_context = Scheduler::on_interrupt(&kernel_frame);
    assert(first_context != &kernel_frame);
    assert(first_context->rip != 0);
    assert(first_context->cs == Gdt::KERNEL_CODE_SELECTOR);
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
