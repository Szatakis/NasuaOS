#include "../driver.hpp"

namespace Memory {

static char ram_buffer[32];
static uint64_t total_ram = 0;

extern "C" 
{
    __attribute__((used, section(".limine_requests")))
    volatile limine_memmap_request memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

    __attribute__((used, section(".limine_requests")))
    volatile limine_hhdm_request hhdm_request = {
        .id = LIMINE_HHDM_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };
}

void* memcpy(void* __restrict dest, const void* __restrict src, size_t n)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    for(size_t i=0;i<n;i++) 
    {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, size_t n)
{
    uint8_t* p = (uint8_t*)s;
    for(size_t i=0;i<n;i++) 
    {
        p[i] = (uint8_t)c;
    }

    return s;
}

void* memmove(void* dest, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if(s > d) 
    {
        for(size_t i=0;i<n;i++)
        {
            d[i] = s[i];
        }
    } 
    else 
    {
        for(size_t i=n;i>0;i--) 
        {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;

    for(size_t i=0;i<n;i++) 
    {
        if(a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

void init()
{
    total_ram = 0;

    if(!memmap_request.response) 
    {
        return;
    }

    for(uint64_t i=0;i<memmap_request.response->entry_count;i++) 
    {
        auto* entry = memmap_request.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
        {
            total_ram += entry->length;
        }
    }
}

const char* total()
{
    uint64_t mb = total_ram / 1024 / 1024;
    char temp[32];
    int i=0;

    if(mb == 0) 
    {
        ram_buffer[0] = '0';
        ram_buffer[1] = '\0';
        return ram_buffer;
    }

    while(mb > 0) 
    {
        temp[i++] = '0' + (mb % 10);
        mb /= 10;
    }

    int j=0;
    while(i > 0) ram_buffer[j++] = temp[--i];
    ram_buffer[j] = '\0';

    return ram_buffer;
}

uint64_t total_bytes()
{
    return total_ram;
}

uint64_t used()
{
    uint64_t free_bytes = pmm::free_memory();
    if(free_bytes >= total_ram) 
    {
        return 0;
    }
    return total_ram - free_bytes;
}

}