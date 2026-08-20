#include "../driver.hpp"


extern volatile limine_memmap_request memmap_request;

static uint8_t* bitmap = nullptr;
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t free_pages = 0;
static uint64_t bitmap_size = 0;


// BITMAP
static inline void set_bit(uint64_t bit)
{
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void clear_bit(uint64_t bit)
{
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool test_bit(uint64_t bit)
{
    return bitmap[bit / 8] & (1 << (bit % 8));
}


// ALIGN
static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}


// FIND HIGHEST MEMORY
static uint64_t get_highest_address()
{
    uint64_t highest = 0;

    for(uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        limine_memmap_entry* entry = memmap_request.response->entries[i];

        uint64_t end = entry->base + entry->length;

        if(end > highest)
        {
            highest = end;
        }
    }

    return highest;
}


// PMM INIT
void pmm_init()
{
    (void)get_highest_address();

    Uart::puts("[PMM] Initializing...\n");
    log(INFO,"PMM","Initializing...");

    if(!memmap_request.response)
    {
        Uart::puts("[PMM] ERROR: no memory map\n");
        log(ERROR,"PMM","No memory map");

        return;
    }

    uint64_t highest = 0;

    for(uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        limine_memmap_entry* entry = memmap_request.response->entries[i];

        if(entry->type != LIMINE_MEMMAP_USABLE)
        {
            continue;
        }

        uint64_t end = entry->base + entry->length;

        if(end > highest)
        {
            highest = end;
        }
    }

    total_pages = highest / PAGE_SIZE;
    bitmap_size = (total_pages + 7) / 8;

    static uint8_t bitmap_storage[1024 * 1024];

    bitmap = bitmap_storage;

    if(bitmap_size > sizeof(bitmap_storage))
    {
        Uart::puts("[PMM] Bitmap too small\n");
        log(WARN,"PMM","Bitmap too small");

        return;
    }

    for(uint64_t i = 0; i < bitmap_size; i++)
    {
        bitmap[i] = 0xFF;
    }

    used_pages = total_pages;
    free_pages = 0;

    for(uint64_t i = 0; i < 256; i++)
    {
        set_bit(i);
    }

    for(uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        limine_memmap_entry* entry = memmap_request.response->entries[i];

        if(entry->type != LIMINE_MEMMAP_USABLE)
        {
            continue;
        }

        uint64_t start = align_up(entry->base, PAGE_SIZE);
        uint64_t end = entry->base + entry->length;

        for(uint64_t addr = start; addr + PAGE_SIZE <= end; addr += PAGE_SIZE)
        {
            uint64_t page = addr / PAGE_SIZE;

            clear_bit(page);

            used_pages--;
            free_pages++;
        }
    }

    /*
        Lock first 1BN for
        (BIOS, VGA, mapy etc.)
    */

    for(uint64_t addr = 0; addr < 0x100000; addr += PAGE_SIZE)
    {
        uint64_t page = addr / PAGE_SIZE;

        if(!test_bit(page))
        {
            set_bit(page);

            used_pages--;
            free_pages--;
        }
    }


    Uart::puts("[PMM] Pages: ");
    Uart::putdec(total_pages);
    Uart::puts("\n");

    Uart::puts("[PMM] Free pages: ");
    Uart::putdec(free_pages);
    Uart::puts("\n");

    Uart::puts("[PMM] Ready\n");
    log(INFO,"PMM","Ready");
}


// ALLOC PAGE
uint64_t pmm_alloc_page()
{
    for(uint64_t i = 0; i < total_pages; i++)
    {
        if(!test_bit(i))
        {
            set_bit(i);

            used_pages++;
            free_pages--;

            return i * PAGE_SIZE;
        }
    }

    return 0;
}


// FREE PAGE
void pmm_free_page(uint64_t addr)
{
    if(addr == 0)
    {
        return;
    }

    uint64_t page = addr / PAGE_SIZE;

    if(test_bit(page))
    {
        clear_bit(page);

        used_pages--;
        free_pages++;
    }
}

uint64_t pmm_get_total_memory()
{
    return total_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory()
{
    return free_pages * PAGE_SIZE;
}