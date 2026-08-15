#include "../driver.hpp"

#include "system/drivers/memory/driver.hpp"

#include "libs/libc/libc.hpp"

static uint64_t ram_pages[RAM_DISK_PAGE_COUNT];

static bool ram_disk_enabled = false;

bool ram_disk_init()
{
    if (ram_disk_enabled)
    {
        return true;
    }

    Uart::puts("[RAM Disk] Initializing...\n");

    for (uint64_t i = 0; i < RAM_DISK_PAGE_COUNT; i++)
    {
        ram_pages[i] = 0;
    }

    Uart::puts("[RAM Disk] Allocating pages...\n");

    for (uint64_t i = 0; i < RAM_DISK_PAGE_COUNT; i++)
    {
        uint64_t phys = pmm_alloc_page();

        if (phys == 0)
        {
            Uart::puts("[RAM Disk] Failed at page: ");

            Uart::putdec(i);

            Uart::puts("\n");


            for (uint64_t j = 0; j < i; j++)
            {
                if (ram_pages[j] != 0)
                {
                    pmm_free_page(ram_pages[j]);

                    ram_pages[j] = 0;
                }
            }

            ram_disk_enabled = false;

            return false;
        }

        ram_pages[i] = phys;
    }

    ram_disk_enabled = true;

    Uart::puts("[RAM Disk] 64 MB allocated.\n");

    Uart::puts("[RAM Disk] Pages: ");

    Uart::putdec(RAM_DISK_PAGE_COUNT);

    Uart::puts("\n");

    return true;
}

void ram_disk_shutdown()
{
    if (!ram_disk_enabled)
    {
        return;
    }

    for (uint64_t i = 0; i < RAM_DISK_PAGE_COUNT; i++)
    {
        if (ram_pages[i] != 0)
        {
            pmm_free_page(ram_pages[i]);

            ram_pages[i] = 0;
        }
    }

    ram_disk_enabled = false;

    Uart::puts("[RAM Disk] Destroyed.\n");
}

bool ram_disk_is_enabled()
{
    return ram_disk_enabled;
}

bool ram_disk_read_sector(uint32_t lba, uint8_t* buffer)
{
    if (!ram_disk_enabled)
    {
        return false;
    }

    if (buffer == nullptr)
    {
        return false;
    }

    if (lba >= RAM_DISK_SECTOR_COUNT)
    {
        return false;
    }

    uint64_t page_index = lba / 8;

    uint64_t sector_index = lba % 8;

    uint64_t phys = ram_pages[page_index];

    if (phys == 0)
    {
        return false;
    }

    uint8_t* source = (uint8_t*)(phys + get_hhdm_offset());

    source += sector_index * 512;

    memcpy(buffer, source, 512);

    return true;
}

bool ram_disk_write_sector(uint32_t lba, uint8_t* buffer)
{
    if (!ram_disk_enabled)
    {
        return false;
    }

    if (buffer == nullptr)
    {
        return false;
    }

    if (lba >= RAM_DISK_SECTOR_COUNT)
    {
        return false;
    }

    uint64_t page_index = lba / 8;
    uint64_t sector_index = lba % 8;
    uint64_t phys = ram_pages[page_index];

    if (phys == 0)
    {
        return false;
    }

    uint8_t* destination = (uint8_t*)(phys + get_hhdm_offset());

    destination += sector_index * 512;

    memcpy(destination, buffer, 512);

    return true;
}

uint64_t ram_disk_size()
{
    return RAM_DISK_SIZE_BYTES;
}

uint32_t ram_disk_sector_count()
{
    return RAM_DISK_SECTOR_COUNT;
}