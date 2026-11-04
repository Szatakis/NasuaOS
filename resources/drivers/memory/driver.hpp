#ifndef MEMORY_H
#define MEMORY_H

#include <limine.h>
#include <stdint.h>
#include <cstdint>
#include <cstddef>

#include "drivers/uart/driver.hpp"
#include "system/sysfunc/logger/logger.hpp"

namespace Memory 
{
    constexpr uint64_t PAGE_SIZE = 4096;

    // BASIC MEMORY
    void* memcpy(void* __restrict dest, const void* __restrict src, size_t n);
    void* memset(void* s, int c, size_t n);
    void* memmove(void* dest, const void* src, size_t n);
    int memcmp(const void* s1, const void* s2, size_t n);

    void init();
    uint64_t used();
    const char* total();
    uint64_t total_bytes();

    // HEAP
    namespace heap 
    {
        void init();
        void* kmalloc(size_t size);
        void kfree(void* ptr);
    }

    // PAGING
    namespace paging 
    {
        void init();
        uint64_t get_cr3();
        uint64_t get_hhdm_offset();
    }

    // PMM
    namespace pmm 
    {
        void init();
        uint64_t alloc_page();
        void free_page(uint64_t addr);
        uint64_t total_memory();
        uint64_t free_memory();
    }

    // VMM
    namespace vmm 
    {
        struct UserAddressSpace
        {
            uint64_t root_phys;
            uint64_t base;
            uint16_t pml4_index;
            bool valid;
        };

        constexpr uint64_t PAGE_PRESENT = 0x001;
        constexpr uint64_t PAGE_WRITE   = 0x002;
        constexpr uint64_t PAGE_USER    = 0x004;
        constexpr uint64_t PAGE_NX      = (1ULL << 63);
        constexpr uint64_t PAGE_EXEC    = 0x000;

        bool map_page(uint64_t virt, uint64_t phys, uint64_t flags);
        bool unmap_page(uint64_t virt);
        uint64_t translate(uint64_t virt);
        bool create_user_address_space(UserAddressSpace* space);
        // User mappings own their physical pages; unmap/destroy releases them.
        bool map_user_page(UserAddressSpace* space, uint64_t virt, uint64_t phys, uint64_t flags);
        bool unmap_user_page(UserAddressSpace* space, uint64_t virt);
        bool activate_address_space(const UserAddressSpace* space);
        void activate_kernel_address_space();
        bool destroy_user_address_space(UserAddressSpace* space);
        void init();
    }
}

#endif