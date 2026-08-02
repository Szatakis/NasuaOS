#include "../driver.hpp"

#include "system/filesystem/clawfs.hpp"

#include "libs/asm/asm.h"

static uint64_t total_storage_bytes = 0;
static uint64_t used_storage_bytes = 0;

#define ATA_PORT_DATA       0x1F0
#define ATA_PORT_SECT_COUNT 0x1F2
#define ATA_PORT_LBA_LOW    0x1F3
#define ATA_PORT_LBA_MID    0x1F4
#define ATA_PORT_LBA_HIGH   0x1F5
#define ATA_PORT_DRV_HEAD   0x1F6
#define ATA_PORT_COMMAND    0x1F7

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01
#define ATA_SR_DF   0x20

#define ATA_CMD_IDENTIFY 0xEC

#define ATA_TIMEOUT 1000000

static bool ata_identify(uint32_t* sectors)
{
    *sectors = 0;

    asm volatile ("outb %0, %1" : : "a"((uint8_t)0xA0), "Nd"((uint16_t)ATA_PORT_DRV_HEAD));
    asm volatile ("outb %0, %1" : : "a"((uint8_t)0), "Nd"((uint16_t)ATA_PORT_SECT_COUNT));
    asm volatile ("outb %0, %1" : : "a"((uint8_t)0), "Nd"((uint16_t)ATA_PORT_LBA_LOW));
    asm volatile ("outb %0, %1" : : "a"((uint8_t)0), "Nd"((uint16_t)ATA_PORT_LBA_MID));
    asm volatile ("outb %0, %1" : : "a"((uint8_t)0), "Nd"((uint16_t)ATA_PORT_LBA_HIGH));

    asm volatile ("outb %0, %1" : : "a"((uint8_t)ATA_CMD_IDENTIFY), "Nd"((uint16_t)ATA_PORT_COMMAND));

    uint8_t status;

    for (uint32_t t = 0; t < ATA_TIMEOUT; t++)
    {
        asm volatile ("inb %1, %0"
            : "=a"(status)
            : "Nd"((uint16_t)ATA_PORT_COMMAND));

        if (status == 0)
        {
            return false;
        }

        if (status & ATA_SR_ERR)
        {
            return false;
        }

        if (status & ATA_SR_DF)
        {
            return false;
        }

        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
        {
            break;
        }

        if (t == ATA_TIMEOUT - 1)
        {
            return false;
        }
    }

    uint16_t identify[256];

    for (int i = 0; i < 256; i++)
    {
        asm volatile ("inw %1, %0"
            : "=a"(identify[i])
            : "Nd"((uint16_t)ATA_PORT_DATA));
    }

    *sectors = ((uint32_t)identify[61] << 16) | identify[60];

    return true;
}

void storage_init()
{
    total_storage_bytes = 0;
    used_storage_bytes = 0;

    uint32_t sectors;

    if (ata_identify(&sectors))
    {
        total_storage_bytes = (uint64_t)sectors * 512;
    }
    else
    {
        total_storage_bytes = 64ULL * 1024 * 1024;
    }

    uint8_t header_buffer[512] = {0};

    disk_read_sector(CLAWFS_START_LBA, header_buffer);

    CLAWFSHeader* header = (CLAWFSHeader*)header_buffer;

    if (header->signature[0] != 'C' ||
        header->signature[1] != 'L' ||
        header->signature[2] != 'A' ||
        header->signature[3] != 'W' ||
        header->signature[4] != 'F' ||
        header->signature[5] != 'S')
    {
        used_storage_bytes = 0;
        return;
    }

    uint32_t entries_sectors = (header->entryCount + 15) / 16;

    if (entries_sectors == 0)
        entries_sectors = 1;

    uint32_t data_sectors = header->entryCount;

    used_storage_bytes =
        (uint64_t)(1 + entries_sectors + data_sectors) * 512;
}

uint64_t storage_total()
{
    return total_storage_bytes;
}

uint64_t storage_used()
{
    return used_storage_bytes;
}