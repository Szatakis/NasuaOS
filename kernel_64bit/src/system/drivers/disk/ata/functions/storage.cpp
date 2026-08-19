#include "../driver.hpp"

#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/drivers/disk/ram_disk/driver.hpp"
#include "system/drivers/gpu/driver.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/drivers/memory/driver.hpp"

#include "libs/libc/libc.hpp"

static uint64_t total_storage_bytes = 0;
static uint64_t used_storage_bytes = 0;

bool storage_uses_ram = false;

bool storage_read_sector(uint32_t lba, uint8_t* buffer)
{
    if (storage_uses_ram)
    {
        return ram_disk_read_sector(lba, buffer);
    }

    disk_read_sector(lba, buffer);

    return true;
}

bool storage_write_sector(uint32_t lba, uint8_t* buffer)
{
    if (storage_uses_ram)
    {
        return ram_disk_write_sector(lba, buffer);
    }

    disk_write_sector(lba, buffer);

    return true;
}

void storage_init()
{
    total_storage_bytes = 0;
    used_storage_bytes = 0;
    storage_uses_ram = false;

    uint32_t sectors = 0;

    if (ata_identify(&sectors))
    {
        total_storage_bytes = (uint64_t)sectors * 512;

        storage_uses_ram = false;

        Uart::puts("[Storage] Disk detected.\n");

        
        Uart::puts("[Storage] Disk detected.\n");
    }
    else
    {
        Uart::puts("[Storage] No disk detected.\n");

        Uart::puts("[Storage] Creating 64 MB RAM disk...\n");

        if (!ram_disk_init())
        {
            Uart::puts("[Storage] Failed to create RAM disk!\n");

            return;
        }

        storage_uses_ram = true;

        total_storage_bytes = ram_disk_size();

        Uart::puts("[Storage] RAM disk created.\n");
    }

    uint8_t header_buffer[512];

    memclear(header_buffer, sizeof(header_buffer));

    if (!storage_read_sector(CLAWFS_START_LBA, header_buffer))
    {
        used_storage_bytes = 0;
        return;
    }

    CLAWFSHeader* header = (CLAWFSHeader*)header_buffer;

    if (memcmp(header->signature, "CLAWFS", 6) != 0)
    {
        used_storage_bytes = 0;
        return;
    }

    uint32_t entries_sectors = (header->entryCount + 15) / 16;

    if (entries_sectors == 0)
    {
        entries_sectors = 1;
    }

    uint32_t data_sectors = header->entryCount;

    used_storage_bytes = (uint64_t)(1 + entries_sectors + data_sectors) * 512;
}

bool storage_is_ram()
{
    return storage_uses_ram;
}

uint64_t storage_total()
{
    return total_storage_bytes;
}

uint64_t storage_used()
{
    return used_storage_bytes;
}
