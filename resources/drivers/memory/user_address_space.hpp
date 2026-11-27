#pragma once

#include <stdint.h>

namespace Memory::vmm
{
    struct UserAddressSpace
    {
        uint64_t root_phys;
        uint64_t base;
        uint16_t pml4_index;
        bool valid;
    };

    bool create_user_address_space(UserAddressSpace* space);
    // User mappings own their physical pages; unmap/destroy releases them.
    bool map_user_page(UserAddressSpace* space, uint64_t virt, uint64_t phys, uint64_t flags);
    bool unmap_user_page(UserAddressSpace* space, uint64_t virt);
    bool is_user_range(const UserAddressSpace* space, uint64_t virt, uint64_t length,
                       bool write, bool execute = false);
    bool sync_kernel_mappings(UserAddressSpace* space);
    bool activate_address_space(UserAddressSpace* space);
    void activate_kernel_address_space();
    bool destroy_user_address_space(UserAddressSpace* space);
}
