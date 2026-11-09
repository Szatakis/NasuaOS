#include <cassert>
#include <cstdint>
#include <cstring>

#include "drivers/memory/driver.hpp"

extern "C"
{
    volatile limine_memmap_request memmap_request = {};
}

namespace Memory::heap
{
    uint64_t test_heap_base();
}

namespace
{
    struct Mapping
    {
        uint64_t virtual_address;
        uint64_t physical_address;
        bool active;
    };

    constexpr uint64_t TEST_USABLE_BYTES = 32 * Memory::PAGE_SIZE;
    Mapping mappings[32] = {};
    limine_memmap_entry reserved_entry = {0, 0x100000, LIMINE_MEMMAP_RESERVED};
    limine_memmap_entry usable_entry = {0x100000, TEST_USABLE_BYTES, LIMINE_MEMMAP_USABLE};
    limine_memmap_entry* entries[] = {&reserved_entry, &usable_entry};
    limine_memmap_response response = {0, 2, entries};
    uint64_t map_call_count = 0;
    uint64_t fail_map_call = 0;

    uint64_t active_mapping_count()
    {
        uint64_t count = 0;
        for (const Mapping& mapping : mappings)
        {
            count += mapping.active;
        }
        return count;
    }

    void reset_mappings()
    {
        for (Mapping& mapping : mappings)
        {
            mapping = {};
        }
        map_call_count = 0;
        fail_map_call = 0;
    }

    void seed_mapping(uint64_t virtual_address, uint64_t physical_address)
    {
        for (Mapping& mapping : mappings)
        {
            if (!mapping.active)
            {
                mapping = {virtual_address, physical_address, true};
                return;
            }
        }
        assert(false);
    }
}

void Uart::puts(const char*) {}
void Uart::putdec(uint64_t) {}
void Uart::puthex(uint64_t) {}
void Uart::putc(char) {}
void Uart::init() {}

void log(LogLevel, const char*, const char*) {}

namespace Memory::vmm
{
    bool map_page(uint64_t virt, uint64_t phys, uint64_t flags)
    {
        ++map_call_count;
        if (fail_map_call != 0 && map_call_count == fail_map_call)
        {
            return false;
        }

        assert(virt >= Memory::heap::test_heap_base());
        assert(virt < Memory::heap::test_heap_base() + 16 * Memory::PAGE_SIZE);
        assert((virt & (Memory::PAGE_SIZE - 1)) == 0);
        assert(flags == PAGE_WRITE);

        for (Mapping& mapping : mappings)
        {
            if (mapping.active && mapping.virtual_address == virt)
            {
                return false;
            }
            if (!mapping.active)
            {
                mapping = {virt, phys, true};
                return true;
            }
        }
        return false;
    }

    bool unmap_page(uint64_t virt)
    {
        for (Mapping& mapping : mappings)
        {
            if (mapping.active && mapping.virtual_address == virt)
            {
                mapping.active = false;
                return true;
            }
        }
        return false;
    }

    uint64_t translate(uint64_t virt)
    {
        const uint64_t page = virt & ~(Memory::PAGE_SIZE - 1);
        for (const Mapping& mapping : mappings)
        {
            if (mapping.active && mapping.virtual_address == page)
            {
                return mapping.physical_address;
            }
        }
        return 0;
    }
}

int main()
{
    memmap_request.response = nullptr;
    Memory::pmm::init();
    assert(Memory::pmm::alloc_page() == 0);
    assert(Memory::pmm::free_memory() == 0);

    memmap_request.response = &response;
    Memory::pmm::init();
    const uint64_t initial_free_memory = TEST_USABLE_BYTES;
    assert(Memory::pmm::free_memory() == initial_free_memory);

    const uint64_t first_page = Memory::pmm::alloc_page();
    assert(first_page == 0x100000);
    assert(Memory::pmm::free_memory() == initial_free_memory - Memory::PAGE_SIZE);
    Memory::pmm::free_page(first_page + 1);
    assert(Memory::pmm::free_memory() == initial_free_memory - Memory::PAGE_SIZE);
    Memory::pmm::free_page(first_page);
    Memory::pmm::free_page(first_page);
    assert(Memory::pmm::free_memory() == initial_free_memory);

    reset_mappings();
    Memory::heap::init();
    assert(Memory::heap::kmalloc(0) == nullptr);
    assert(Memory::heap::kmalloc(static_cast<size_t>(~0ULL)) == nullptr);
    assert(Memory::heap::kmalloc(16 * Memory::PAGE_SIZE) == nullptr);

    seed_mapping(Memory::heap::test_heap_base() + Memory::PAGE_SIZE, 0x200000);
    void* after_existing_mapping = Memory::heap::kmalloc(5000);
    assert(after_existing_mapping ==
           reinterpret_cast<void*>(Memory::heap::test_heap_base() +
                                   2 * Memory::PAGE_SIZE + 24));
    Memory::heap::kfree(after_existing_mapping);
    assert(active_mapping_count() == 1);
    assert(Memory::vmm::unmap_page(Memory::heap::test_heap_base() + Memory::PAGE_SIZE));
    assert(active_mapping_count() == 0);

    void* first = Memory::heap::kmalloc(64);
    void* second = Memory::heap::kmalloc(5000);
    void* third = Memory::heap::kmalloc(32);
    assert(first != nullptr && second != nullptr && third != nullptr);
    assert(active_mapping_count() == 4);
    std::memset(second, 0xA5, 5000);
    assert(static_cast<uint8_t*>(second)[4999] == 0xA5);

    Memory::heap::kfree(second);
    assert(active_mapping_count() == 2);
    void* reused = Memory::heap::kmalloc(5000);
    assert(reused == second);
    Memory::heap::kfree(reused);
    Memory::heap::kfree(third);
    Memory::heap::kfree(first);
    assert(active_mapping_count() == 0);

    void* duplicate = Memory::heap::kmalloc(1);
    assert(duplicate != nullptr);
    Memory::heap::kfree(duplicate);
    Memory::heap::kfree(duplicate);
    Memory::heap::kfree(reinterpret_cast<void*>(1));
    assert(active_mapping_count() == 0);

    const uint64_t before_map_failure = Memory::pmm::free_memory();
    fail_map_call = map_call_count + 2;
    assert(Memory::heap::kmalloc(3 * Memory::PAGE_SIZE) == nullptr);
    assert(active_mapping_count() == 0);
    assert(Memory::pmm::free_memory() == before_map_failure);
    fail_map_call = 0;

    uint64_t held_pages[30] = {};
    for (uint64_t& page : held_pages)
    {
        page = Memory::pmm::alloc_page();
        assert(page != 0);
    }
    assert(Memory::pmm::free_memory() == 2 * Memory::PAGE_SIZE);
    assert(Memory::heap::kmalloc(2 * Memory::PAGE_SIZE) == nullptr);
    assert(active_mapping_count() == 0);
    assert(Memory::pmm::free_memory() == 2 * Memory::PAGE_SIZE);
    for (uint64_t page : held_pages)
    {
        Memory::pmm::free_page(page);
    }
    assert(Memory::pmm::free_memory() == initial_free_memory);

    void* allocations[16] = {};
    for (void*& allocation : allocations)
    {
        allocation = Memory::heap::kmalloc(1);
        assert(allocation != nullptr);
    }
    assert(Memory::heap::kmalloc(1) == nullptr);
    for (void* allocation : allocations)
    {
        Memory::heap::kfree(allocation);
    }
    assert(active_mapping_count() == 0);
    assert(Memory::pmm::free_memory() == initial_free_memory);
    return 0;
}
