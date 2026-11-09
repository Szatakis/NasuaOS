#include "../driver.hpp"

extern "C"
{
    extern volatile limine_memmap_request memmap_request;
}

namespace Memory
{
    namespace pmm
    {
        static uint8_t* bitmap = nullptr;
        static uint64_t total_pages = 0;
        static uint64_t used_pages = 0;
        static uint64_t free_pages = 0;
        static uint64_t bitmap_size = 0;
        static uint8_t bitmap_storage[1024 * 1024];

        static inline void set_bit(uint64_t bit)
        {
            bitmap[bit / 8] |= static_cast<uint8_t>(1U << (bit % 8));
        }

        static inline void clear_bit(uint64_t bit)
        {
            bitmap[bit / 8] &= static_cast<uint8_t>(~(1U << (bit % 8)));
        }

        static inline bool test_bit(uint64_t bit)
        {
            return (bitmap[bit / 8] & static_cast<uint8_t>(1U << (bit % 8))) != 0;
        }

        static bool align_up(uint64_t value, uint64_t alignment, uint64_t& aligned)
        {
            const uint64_t remainder = value % alignment;
            const uint64_t increment = remainder == 0 ? 0 : alignment - remainder;
            if (value > ~0ULL - increment)
            {
                return false;
            }

            aligned = value + increment;
            return true;
        }

        void init()
        {
            Uart::puts("[PMM] Initializing...\n");
            log(INFO, "PMM", "Initializing...");

            bitmap = nullptr;
            total_pages = 0;
            used_pages = 0;
            free_pages = 0;
            bitmap_size = 0;

            if (::memmap_request.response == nullptr)
            {
                Uart::puts("[PMM] ERROR: no memory map\n");
                log(ERROR, "PMM", "No memory map");
                return;
            }

            uint64_t highest = 0;
            for (uint64_t i = 0; i < ::memmap_request.response->entry_count; ++i)
            {
                limine_memmap_entry* entry = ::memmap_request.response->entries[i];
                if (entry == nullptr || entry->type != LIMINE_MEMMAP_USABLE)
                {
                    continue;
                }

                if (entry->length > ~0ULL - entry->base)
                {
                    Uart::puts("[PMM] Invalid memory map range\n");
                    log(ERROR, "PMM", "Usable memory range overflows");
                    continue;
                }

                const uint64_t end = entry->base + entry->length;
                if (end > highest)
                {
                    highest = end;
                }
            }

            total_pages = highest / PAGE_SIZE + (highest % PAGE_SIZE != 0);
            bitmap_size = total_pages / 8 + (total_pages % 8 != 0);
            if (total_pages == 0)
            {
                Uart::puts("[PMM] ERROR: no usable memory\n");
                log(ERROR, "PMM", "No usable memory");
                return;
            }

            if (bitmap_size > sizeof(bitmap_storage))
            {
                total_pages = 0;
                bitmap_size = 0;
                Uart::puts("[PMM] Bitmap too small\n");
                log(ERROR, "PMM", "Bitmap too small");
                return;
            }

            bitmap = bitmap_storage;
            for (uint64_t i = 0; i < bitmap_size; ++i)
            {
                bitmap[i] = 0xFF;
            }
            used_pages = total_pages;

            for (uint64_t i = 0; i < ::memmap_request.response->entry_count; ++i)
            {
                limine_memmap_entry* entry = ::memmap_request.response->entries[i];
                if (entry == nullptr || entry->type != LIMINE_MEMMAP_USABLE ||
                    entry->length > ~0ULL - entry->base)
                {
                    continue;
                }

                uint64_t start = 0;
                if (!align_up(entry->base, PAGE_SIZE, start))
                {
                    continue;
                }
                const uint64_t end = entry->base + entry->length;

                for (uint64_t address = start; address <= end && PAGE_SIZE <= end - address;
                     address += PAGE_SIZE)
                {
                    const uint64_t page = address / PAGE_SIZE;
                    if (page < total_pages && test_bit(page))
                    {
                        clear_bit(page);
                        --used_pages;
                        ++free_pages;
                    }
                }
            }

            const uint64_t reserved_pages = 0x100000 / PAGE_SIZE;
            for (uint64_t page = 0; page < reserved_pages && page < total_pages; ++page)
            {
                if (!test_bit(page))
                {
                    set_bit(page);
                    --free_pages;
                    ++used_pages;
                }
            }

            Uart::puts("[PMM] Pages: ");
            Uart::putdec(total_pages);
            Uart::puts("\n");
            Uart::puts("[PMM] Free pages: ");
            Uart::putdec(free_pages);
            Uart::puts("\n");
            Uart::puts("[PMM] Ready\n");
            log(INFO, "PMM", "Ready");
        }

        uint64_t alloc_page()
        {
            if (bitmap == nullptr)
            {
                return 0;
            }

            for (uint64_t i = 0; i < total_pages; ++i)
            {
                if (!test_bit(i))
                {
                    set_bit(i);
                    ++used_pages;
                    --free_pages;
                    return i * PAGE_SIZE;
                }
            }

            return 0;
        }

        void free_page(uint64_t addr)
        {
            if (bitmap == nullptr || addr == 0 || (addr & (PAGE_SIZE - 1)) != 0)
            {
                return;
            }

            const uint64_t page = addr / PAGE_SIZE;
            if (page < total_pages && test_bit(page))
            {
                clear_bit(page);
                --used_pages;
                ++free_pages;
            }
        }

        uint64_t total_memory()
        {
            return total_pages * PAGE_SIZE;
        }

        uint64_t free_memory()
        {
            return free_pages * PAGE_SIZE;
        }
    }
}
