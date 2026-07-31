#include "../driver.hpp" 

static char ram_buffer[32];

static uint64_t total_ram = 0; 
static uint64_t used_ram = 0;  

extern "C" {

    void *memcpy(void *__restrict dest, const void *__restrict src, std::size_t n) 
    {
        std::uint8_t *__restrict pdest = static_cast<std::uint8_t *__restrict>(dest);
        const std::uint8_t *__restrict psrc = static_cast<const std::uint8_t *__restrict>(src);

        for (std::size_t i = 0; i < n; i++) 
        {
            pdest[i] = psrc[i];
        }

        return dest;
    }

    extern "C" {
        __attribute__((used, section(".limine_requests"))) 
        volatile limine_memmap_request memmap_request = {     
            .id = LIMINE_MEMMAP_REQUEST_ID, 
            .revision = 0,     
            .response = nullptr 
        };  
    }

    void *memset(void *s, int c, std::size_t n) 
    {
        std::uint8_t *p = static_cast<std::uint8_t *>(s);

        for (std::size_t i = 0; i < n; i++) 
        {
            p[i] = static_cast<uint8_t>(c);
        }

        return s;
    }

    void *memmove(void *dest, const void *src, std::size_t n) 
    {
        std::uint8_t *pdest = static_cast<std::uint8_t *>(dest);
        const std::uint8_t *psrc = static_cast<const std::uint8_t *>(src);

        if (reinterpret_cast<std::uintptr_t>(src) > reinterpret_cast<std::uintptr_t>(dest)) 
        {
            for (std::size_t i = 0; i < n; i++) 
            {
                pdest[i] = psrc[i];
            }
        } 
        else if (reinterpret_cast<std::uintptr_t>(src) < reinterpret_cast<std::uintptr_t>(dest)) 
        {
            for (std::size_t i = n; i > 0; i--) 
            {
                pdest[i-1] = psrc[i-1];
            }
        }

        return dest;
    }

    int memcmp(const void *s1, const void *s2, std::size_t n) 
    {
        const std::uint8_t *p1 = static_cast<const std::uint8_t *>(s1);
        const std::uint8_t *p2 = static_cast<const std::uint8_t *>(s2);

        for (std::size_t i = 0; i < n; i++) 
        {
            if (p1[i] != p2[i]) 
            {
                return p1[i] < p2[i] ? -1 : 1;
            }
        }

        return 0;
    }
}


void memory_init()
{
    total_ram = 0;

    if (!memmap_request.response)
    {
        return;
    }

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        limine_memmap_entry* entry = memmap_request.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            total_ram += entry->length;
        }
    }

    used_ram = 0;
}


const char* memory_total()
{
    uint64_t mb = total_ram / 1024 / 1024;
    char temp[32];
    int i = 0;

    if (mb == 0)
    {
        ram_buffer[0] = '0';
        ram_buffer[1] = '\0';
        return ram_buffer;
    }

    while (mb > 0)
    {
        temp[i++] = '0' + (mb % 10);
        mb /= 10;
    }

    int j = 0;

    while (i > 0)
    {
        ram_buffer[j++] = temp[--i];
    }

    ram_buffer[j] = '\0';

    return ram_buffer;
}


uint64_t memory_total_bytes()
{
    return total_ram;
}

uint64_t memory_used() 
{     
    return used_ram; 
}