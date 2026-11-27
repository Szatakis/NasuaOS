#include "ring3_probe.hpp"

#include "drivers/memory/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "system/scheduler/scheduler.hpp"
#include "system/sysfunc/logger/logger.hpp"

extern "C" const uint8_t ring3_probe_start[];
extern "C" const uint8_t ring3_probe_end[];

namespace Ring3
{
    void launch_probe()
    {
        Memory::vmm::UserAddressSpace address_space{};
        if (!Memory::vmm::create_user_address_space(&address_space))
        {
            log(ERROR, "RING3", "Unable to allocate the privilege-transition test address space");
            return;
        }

        const uint64_t code_phys = Memory::pmm::alloc_page();
        const uint64_t stack_phys = Memory::pmm::alloc_page();
        if (code_phys == 0 || stack_phys == 0)
        {
            if (code_phys != 0)
            {
                Memory::pmm::free_page(code_phys);
            }
            if (stack_phys != 0)
            {
                Memory::pmm::free_page(stack_phys);
            }
            Memory::vmm::destroy_user_address_space(&address_space);
            log(ERROR, "RING3", "Unable to allocate user code and stack pages");
            return;
        }

        constexpr uint64_t USER_CODE_OFFSET = 0x10000;
        constexpr uint64_t USER_STACK_OFFSET = 0x20000;
        constexpr uint64_t PAGE_SIZE = Memory::PAGE_SIZE;
        const uint64_t code_address = address_space.base + USER_CODE_OFFSET;
        const uint64_t stack_address = address_space.base + USER_STACK_OFFSET;
        const uint64_t code_size = static_cast<uint64_t>(ring3_probe_end - ring3_probe_start);

        if (code_size > PAGE_SIZE ||
            !Memory::vmm::map_user_page(&address_space, code_address, code_phys, Memory::vmm::PAGE_USER) ||
            !Memory::vmm::map_user_page(&address_space, stack_address, stack_phys,
                                         Memory::vmm::PAGE_USER | Memory::vmm::PAGE_WRITE |
                                         Memory::vmm::PAGE_NX))
        {
            if (code_size > PAGE_SIZE ||
                !Memory::vmm::is_user_range(&address_space, code_address, 1, false))
            {
                Memory::pmm::free_page(code_phys);
            }
            if (!Memory::vmm::is_user_range(&address_space, stack_address, 1, true))
            {
                Memory::pmm::free_page(stack_phys);
            }
            Memory::vmm::destroy_user_address_space(&address_space);
            log(ERROR, "RING3", "Unable to map user code and stack pages");
            return;
        }

        auto* code_destination = reinterpret_cast<void*>(Memory::paging::get_hhdm_offset() + code_phys);
        Memory::memcpy(code_destination, ring3_probe_start, code_size);

        auto* stack_destination = reinterpret_cast<uint8_t*>(Memory::paging::get_hhdm_offset() + stack_phys);
        for (uint64_t index = 0; index < PAGE_SIZE; ++index)
        {
            stack_destination[index] = 0;
        }

        if (Scheduler::create_user_task(&address_space, code_address, stack_address + PAGE_SIZE) == 0)
        {
            Memory::vmm::destroy_user_address_space(&address_space);
            log(ERROR, "RING3", "Unable to schedule the privilege-transition test");
            return;
        }

        Uart::puts("[RING3] User-mode syscall probe scheduled\n");
    }
}
