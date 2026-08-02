#include "../driver.hpp"

#include "libs/asm/asm.h"

#define ATA_PORT_DATA       0x1F0
#define ATA_PORT_ERROR      0x1F1
#define ATA_PORT_SECT_COUNT 0x1F2
#define ATA_PORT_LBA_LOW    0x1F3
#define ATA_PORT_LBA_MID    0x1F4
#define ATA_PORT_LBA_HIGH   0x1F5
#define ATA_PORT_DRV_HEAD   0x1F6
#define ATA_PORT_COMMAND    0x1F7

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_TIMEOUT 1000000

static bool ata_wait_not_busy()
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = inb(ATA_PORT_COMMAND);

        if (!(status & ATA_SR_BSY))
            return true;
    }

    return false;
}

static bool ata_wait_drq()
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = inb(ATA_PORT_COMMAND);

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
            return true;
        }
    }

    return false;
}

void disk_read_sector(uint32_t lba, uint8_t* buffer)
{
    if (!ata_wait_not_busy())
        return;

    outb(ATA_PORT_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));

    outb(ATA_PORT_SECT_COUNT, 1);

    outb(ATA_PORT_LBA_LOW,  (uint8_t)lba);
    outb(ATA_PORT_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_PORT_COMMAND, ATA_CMD_READ_PIO);

    if (!ata_wait_drq())
    {
        return;
    }

    uint16_t* ptr = (uint16_t*)buffer;

    for (int i = 0; i < 256; i++)
    {
        ptr[i] = inw(ATA_PORT_DATA);
    }
}

void disk_write_sector(uint32_t lba, uint8_t* buffer)
{
    if (!ata_wait_not_busy())
    {
        return;
    }

    outb(ATA_PORT_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));

    outb(ATA_PORT_SECT_COUNT, 1);

    outb(ATA_PORT_LBA_LOW,  (uint8_t)lba);
    outb(ATA_PORT_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_PORT_COMMAND, ATA_CMD_WRITE_PIO);

    if (!ata_wait_drq())
    {
        return;
    }

    uint16_t* ptr = (uint16_t*)buffer;

    for (int i = 0; i < 256; i++)
    {
        outw(ATA_PORT_DATA, ptr[i]);
    }

    outb(ATA_PORT_COMMAND, ATA_CMD_CACHE_FLUSH);

    ata_wait_not_busy();
}