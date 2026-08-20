#include "../driver.hpp"

#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/drivers/disk/ram_disk/driver.hpp"
#include "system/drivers/gpu/driver.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/drivers/memory/driver.hpp"

#include "libs/libc/libc.hpp"

static uint64_t total_storage_bytes = 0;
static uint64_t used_storage_bytes = 0;

static bool storage_uses_ram = false;

namespace Disk
{
    bool read_sector(uint32_t lba, uint8_t* buffer)
    {
        if (storage_uses_ram)
        {
            return RAM_Disk::read_sector(lba, buffer);
        }

        return ATA_disk::read_sector(lba, buffer);
    }

    bool write_sector(uint32_t lba, uint8_t* buffer)
    {
        if (storage_uses_ram)
        {
            return RAM_Disk::write_sector(lba, buffer);
        }

        return ATA_disk::write_sector(lba, buffer);
    }

    void init()
    {
        total_storage_bytes = 0;
        used_storage_bytes = 0;
        storage_uses_ram = false;

        uint32_t sectors = 0;

        if (ATA_disk::identify(&sectors))
        {
            total_storage_bytes = (uint64_t)sectors * 512;

            Uart::puts("[Storage] Disk detected.\n");
        }
        else
        {
            Uart::puts("[Storage] No disk detected.\n");
            Uart::puts("[Storage] Creating 64 MB RAM disk...\n");

            if (!RAM_Disk::init())
            {
                Uart::puts("[Storage] Failed to create RAM disk!\n");
                return;
            }

            storage_uses_ram = true;
            total_storage_bytes = RAM_Disk::size();

            Uart::puts("[Storage] RAM disk created.\n");
        }

        uint8_t header_buffer[512];

        memclear(header_buffer, sizeof(header_buffer));

        if (!Disk::read_sector(CLAWFS_START_LBA, header_buffer))
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

        uint32_t entries_sectors =
            (header->entryCount + 15) / 16;

        if (entries_sectors == 0)
        {
            entries_sectors = 1;
        }

        uint32_t data_sectors = header->entryCount;

        used_storage_bytes =
            (uint64_t)(1 + entries_sectors + data_sectors) * 512;
    }

    bool storage_is_ram()
    {
        return storage_uses_ram;
    }

    uint64_t total()
    {
        return total_storage_bytes;
    }

    uint64_t used()
    {
        return used_storage_bytes;
    }
}