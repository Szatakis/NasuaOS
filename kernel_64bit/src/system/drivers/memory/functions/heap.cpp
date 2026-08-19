#include "../driver.hpp"


#define HEAP_START 0xFFFF900000000000ULL
#define PAGE_SIZE 4096

struct heap_header
{
    uint64_t size;
    uint64_t pages;
};

static uint64_t heap_current = HEAP_START;

void heap_init()
{
    Uart::puts("[HEAP] Initializing...\n");
    log(INFO,"HEAP","Initializing...");

    heap_current = HEAP_START;

    Uart::puts("[HEAP] Ready\n");
    log(INFO,"HEAP","Ready");
}

void* kmalloc(size_t size)
{
    if(size == 0)
    {
        return nullptr;
    }

    uint64_t total_size = size + sizeof(heap_header);

    uint64_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t start = heap_current;

    for(uint64_t i = 0;i < pages; i++)
    {
        uint64_t phys = pmm_alloc_page();

        if(!phys)
        {
            Uart::puts("[HEAP] OUT OF MEMORY\n");
            log(WARN,"HEAP","OUT OF MEMORY");

            return nullptr;
        }

        // Map heap pages as executable (needed for flat binaries loaded from ClawFS)
        if(!vmm_map_page(heap_current, phys, PAGE_WRITE))
        {
            Uart::puts("[HEAP] MAP FAILED\n");
            log(ERROR,"HEAP","MAP FAILED");

            return nullptr;
        }

        heap_current += PAGE_SIZE;
    }

    heap_header* header = (heap_header*)start;

    header->size = size;

    header->pages = pages;

    return (void*)(start + sizeof(heap_header));
}

void kfree(void* ptr)
{
    if(!ptr)
    {
        return;
    }

    uint64_t address = (uint64_t)ptr;

    heap_header* header = (heap_header*)(address - sizeof(heap_header));

    uint64_t start = address - sizeof(heap_header);

    uint64_t pages = header->pages;

    for(uint64_t i = 0; i < pages; i++)
    {
        uint64_t virt = start + i * PAGE_SIZE;

        uint64_t phys = vmm_translate(virt);

        if(phys)
        {
            vmm_unmap_page(virt);

            pmm_free_page(phys);
        }
    }
}