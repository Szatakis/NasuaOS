#include "../driver.hpp"

namespace Memory
{
    namespace heap
    {
#ifdef HEAP_TEST
        static constexpr uint64_t HEAP_MAX_PAGES = 16;
        alignas(PAGE_SIZE) static uint8_t heap_test_storage[HEAP_MAX_PAGES * PAGE_SIZE];

        static uint64_t heap_start()
        {
            return reinterpret_cast<uint64_t>(heap_test_storage);
        }
#else
        static constexpr uint64_t HEAP_START = 0xFFFF900000000000ULL;
        static constexpr uint64_t HEAP_MAX_PAGES = 1ULL << 20;

        static uint64_t heap_start()
        {
            return HEAP_START;
        }
#endif

        struct heap_header
        {
            uint64_t magic;
            uint64_t size;
            uint64_t pages;
        };

        static constexpr uint64_t HEAP_MAGIC = 0x4E41535541484541ULL;
        static uint8_t allocation_bitmap[(HEAP_MAX_PAGES + 7) / 8];
        static uint64_t search_hint = 0;
        static bool initialized = false;
#ifndef HEAP_TEST
        static uint8_t heap_lock = 0;
#endif

        static inline uint64_t lock_heap()
        {
#ifdef HEAP_TEST
            return 0;
#else
            uint64_t flags;
            asm volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
            while (__atomic_test_and_set(&heap_lock, __ATOMIC_ACQUIRE))
            {
                asm volatile("pause");
            }
            return flags;
#endif
        }

        static inline void unlock_heap(uint64_t flags)
        {
#ifndef HEAP_TEST
            __atomic_clear(&heap_lock, __ATOMIC_RELEASE);
            if (flags & (1ULL << 9))
            {
                asm volatile("sti" : : : "memory");
            }
#else
            (void)flags;
#endif
        }

        static inline bool page_is_allocated(uint64_t page)
        {
            return (allocation_bitmap[page / 8] & (1U << (page % 8))) != 0;
        }

        static inline void mark_page(uint64_t page)
        {
            allocation_bitmap[page / 8] |= static_cast<uint8_t>(1U << (page % 8));
        }

        static inline void release_page(uint64_t page)
        {
            allocation_bitmap[page / 8] &= static_cast<uint8_t>(~(1U << (page % 8)));
            if (page < search_hint)
            {
                search_hint = page;
            }
        }

        static bool find_free_run(uint64_t pages, uint64_t& start)
        {
            if (pages == 0 || pages > HEAP_MAX_PAGES)
            {
                return false;
            }

            for (uint64_t pass = 0; pass < 2; ++pass)
            {
                const uint64_t first = pass == 0 ? search_hint : 0;
                const uint64_t limit = pass == 0 ? HEAP_MAX_PAGES : search_hint;
                uint64_t candidate = first;

                while (candidate <= limit && pages <= limit - candidate)
                {
                    uint64_t occupied = 0;
                    while (occupied < pages && !page_is_allocated(candidate + occupied))
                    {
                        ++occupied;
                    }

                    if (occupied != pages)
                    {
                        candidate += occupied + 1;
                        continue;
                    }

                    uint64_t mapped = 0;
                    while (mapped < pages &&
                           Memory::vmm::translate(heap_start() +
                               (candidate + mapped) * PAGE_SIZE) == 0)
                    {
                        ++mapped;
                    }

                    if (mapped == pages)
                    {
                        start = candidate;
                        return true;
                    }

                    candidate += mapped + 1;
                }
            }

            return false;
        }

        static void report_failure(const char* uart_message, const char* log_message, LogLevel level)
        {
            Uart::puts(uart_message);
            log(level, "HEAP", log_message);
        }

        void init()
        {
            if (initialized)
            {
                report_failure("[HEAP] Already initialized\n", "Already initialized", WARN);
                return;
            }

            Uart::puts("[HEAP] Initializing...\n");
            log(INFO, "HEAP", "Initializing...");

            for (uint64_t i = 0; i < sizeof(allocation_bitmap); ++i)
            {
                allocation_bitmap[i] = 0;
            }
            search_hint = 0;
            initialized = true;

            Uart::puts("[HEAP] Ready\n");
            log(INFO, "HEAP", "Ready");
        }

        void* kmalloc(size_t size)
        {
            if (size == 0)
            {
                return nullptr;
            }

            if (!initialized)
            {
                report_failure("[HEAP] Not initialized\n", "Allocation before initialization", ERROR);
                return nullptr;
            }

            const uint64_t requested_size = static_cast<uint64_t>(size);
            if (requested_size > ~0ULL - sizeof(heap_header))
            {
                report_failure("[HEAP] Allocation too large\n", "Allocation size overflow", WARN);
                return nullptr;
            }

            const uint64_t total_size = requested_size + sizeof(heap_header);
            const uint64_t pages = total_size / PAGE_SIZE + (total_size % PAGE_SIZE != 0);

            const uint64_t flags = lock_heap();
            uint64_t first_page = 0;
            if (!find_free_run(pages, first_page))
            {
                unlock_heap(flags);
                report_failure("[HEAP] OUT OF VIRTUAL MEMORY\n", "Virtual heap exhausted", WARN);
                return nullptr;
            }

            for (uint64_t i = 0; i < pages; ++i)
            {
                mark_page(first_page + i);
            }
            search_hint = first_page + pages;
            if (search_hint == HEAP_MAX_PAGES)
            {
                search_hint = 0;
            }

            const uint64_t start = heap_start() + first_page * PAGE_SIZE;
            uint64_t mapped_pages = 0;
            bool physical_allocation_failed = false;
            bool mapping_failed = false;
            uint64_t failed_virtual_address = 0;

            for (; mapped_pages < pages; ++mapped_pages)
            {
                const uint64_t physical = Memory::pmm::alloc_page();
                if (physical == 0)
                {
                    physical_allocation_failed = true;
                    break;
                }

                const uint64_t virtual_address = start + mapped_pages * PAGE_SIZE;
                if (!Memory::vmm::map_page(virtual_address, physical, Memory::vmm::PAGE_WRITE))
                {
                    Memory::pmm::free_page(physical);
                    mapping_failed = true;
                    failed_virtual_address = virtual_address;
                    break;
                }
            }

            if (mapped_pages != pages)
            {
                bool rollback_failed = false;
                for (uint64_t i = 0; i < mapped_pages; ++i)
                {
                    const uint64_t virtual_address = start + i * PAGE_SIZE;
                    const uint64_t physical = Memory::vmm::translate(virtual_address);
                    if (physical == 0 || !Memory::vmm::unmap_page(virtual_address))
                    {
                        rollback_failed = true;
                        continue;
                    }

                    Memory::pmm::free_page(physical);
                    release_page(first_page + i);
                }
                for (uint64_t i = mapped_pages; i < pages; ++i)
                {
                    release_page(first_page + i);
                }

                unlock_heap(flags);
                if (!rollback_failed)
                {
                    Uart::puts("[HEAP] Failed request bytes: ");
                    Uart::putdec(requested_size);
                    Uart::puts(", pages: ");
                    Uart::putdec(pages);
                    Uart::puts(", free physical bytes: ");
                    Uart::putdec(Memory::pmm::free_memory());
                    Uart::puts(", mapped pages: ");
                    Uart::putdec(mapped_pages);
                    if (mapping_failed)
                    {
                        Uart::puts(", virtual address: ");
                        Uart::puthex(failed_virtual_address);
                    }
                    Uart::puts("\n");
                }
                report_failure(rollback_failed ? "[HEAP] Allocation rollback incomplete\n"
                                                : physical_allocation_failed
                                                    ? "[HEAP] OUT OF PHYSICAL MEMORY\n"
                                                    : "[HEAP] PAGE MAPPING FAILED\n",
                               rollback_failed ? "Allocation rollback incomplete"
                                               : physical_allocation_failed
                                                   ? "Out of physical memory"
                                                   : mapping_failed
                                                       ? "Unable to map heap page"
                                                       : "Heap allocation failed",
                               rollback_failed ? ERROR : WARN);
                return nullptr;
            }

            heap_header* header = reinterpret_cast<heap_header*>(start);
            header->magic = HEAP_MAGIC;
            header->size = requested_size;
            header->pages = pages;
            void* allocation = reinterpret_cast<void*>(start + sizeof(heap_header));
            unlock_heap(flags);
            return allocation;
        }

        void kfree(void* ptr)
        {
            if (ptr == nullptr)
            {
                return;
            }

            if (!initialized)
            {
                report_failure("[HEAP] Free before initialization\n", "Free before initialization", ERROR);
                return;
            }

            const uint64_t address = reinterpret_cast<uint64_t>(ptr);
            const uint64_t base = heap_start();
            if (address < base + sizeof(heap_header))
            {
                report_failure("[HEAP] Invalid free\n", "Pointer is outside the heap", ERROR);
                return;
            }

            const uint64_t start = address - sizeof(heap_header);
            if (start < base || start >= base + HEAP_MAX_PAGES * PAGE_SIZE ||
                (start - base) % PAGE_SIZE != 0)
            {
                report_failure("[HEAP] Invalid free\n", "Pointer is not an allocation start", ERROR);
                return;
            }

            const uint64_t flags = lock_heap();
            const uint64_t first_page = (start - base) / PAGE_SIZE;
            if (!page_is_allocated(first_page))
            {
                unlock_heap(flags);
                report_failure("[HEAP] Invalid or duplicate free\n", "Pointer is not allocated", ERROR);
                return;
            }

            heap_header* header = reinterpret_cast<heap_header*>(start);
            const uint64_t pages = header->pages;
            if (header->magic != HEAP_MAGIC || pages == 0 ||
                pages > HEAP_MAX_PAGES - first_page ||
                header->size > pages * PAGE_SIZE - sizeof(heap_header))
            {
                unlock_heap(flags);
                report_failure("[HEAP] Invalid allocation header\n", "Allocation metadata is corrupt", ERROR);
                return;
            }

            for (uint64_t i = 0; i < pages; ++i)
            {
                if (!page_is_allocated(first_page + i) ||
                    Memory::vmm::translate(start + i * PAGE_SIZE) == 0)
                {
                    unlock_heap(flags);
                    report_failure("[HEAP] Invalid allocation mapping\n", "Allocation mapping is missing", ERROR);
                    return;
                }
            }

            for (uint64_t i = 0; i < pages; ++i)
            {
                const uint64_t virtual_address = start + i * PAGE_SIZE;
                const uint64_t physical = Memory::vmm::translate(virtual_address);
                if (!Memory::vmm::unmap_page(virtual_address))
                {
                    unlock_heap(flags);
                    report_failure("[HEAP] Unmap failed while freeing\n", "Unable to unmap heap allocation", ERROR);
                    return;
                }

                Memory::pmm::free_page(physical);
                release_page(first_page + i);
            }

            unlock_heap(flags);
        }

#ifdef HEAP_TEST
        uint64_t test_heap_base()
        {
            return heap_start();
        }
#endif
    }
}
