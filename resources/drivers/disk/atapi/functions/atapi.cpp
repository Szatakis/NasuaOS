#include "../driver.hpp"

#include "libs/asm/asm.hpp"

namespace ATAPI
{
    namespace
    {
        constexpr uint16_t ATA_PRIMARY_IO = 0x1F0;
        constexpr uint16_t ATA_PRIMARY_CONTROL = 0x3F6;
        constexpr uint16_t ATA_SECONDARY_IO = 0x170;
        constexpr uint16_t ATA_SECONDARY_CONTROL = 0x376;
        constexpr uint16_t ATA_DATA = 0;
        constexpr uint16_t ATA_FEATURES = 1;
        constexpr uint16_t ATA_SECTOR_COUNT = 2;
        constexpr uint16_t ATA_LBA_LOW = 3;
        constexpr uint16_t ATA_LBA_MID = 4;
        constexpr uint16_t ATA_LBA_HIGH = 5;
        constexpr uint16_t ATA_DRIVE_HEAD = 6;
        constexpr uint16_t ATA_STATUS_COMMAND = 7;
        constexpr uint8_t ATA_STATUS_ERR = 0x01;
        constexpr uint8_t ATA_STATUS_DRQ = 0x08;
        constexpr uint8_t ATA_STATUS_BSY = 0x80;
        constexpr uint8_t ATA_CMD_IDENTIFY_PACKET = 0xA1;
        constexpr uint8_t ATA_CMD_PACKET = 0xA0;
        constexpr uint8_t ATAPI_CMD_READ_12 = 0xA8;
        constexpr uint32_t ATA_TIMEOUT = 1000000;
        constexpr uint32_t SECTOR_SIZE = 2048;
        constexpr uint8_t MAX_SECTORS_PER_COMMAND = 31;

        struct device
        {
            uint16_t io;
            uint16_t control;
            uint8_t drive_select;
            bool present;
        };

        device optical_drive{};

        void delay_400ns(uint16_t control)
        {
            for (uint32_t i = 0; i < 4; ++i)
            {
                (void)inb(control);
            }
        }

        bool wait_for_status(const device& drive, bool require_drq)
        {
            for (uint32_t attempt = 0; attempt < ATA_TIMEOUT; ++attempt)
            {
                const uint8_t status = inb(drive.io + ATA_STATUS_COMMAND);
                if (status == 0 || status == 0xFF || (status & ATA_STATUS_ERR) != 0)
                {
                    return false;
                }
                if ((status & ATA_STATUS_BSY) == 0 &&
                    (!require_drq || (status & ATA_STATUS_DRQ) != 0))
                {
                    return true;
                }
            }
            return false;
        }

        bool identify_packet_device(device* drive)
        {
            if (drive == nullptr)
            {
                return false;
            }

            outb(drive->io + ATA_DRIVE_HEAD, drive->drive_select);
            delay_400ns(drive->control);
            outb(drive->io + ATA_SECTOR_COUNT, 0);
            outb(drive->io + ATA_LBA_LOW, 0);
            outb(drive->io + ATA_LBA_MID, 0);
            outb(drive->io + ATA_LBA_HIGH, 0);
            outb(drive->io + ATA_STATUS_COMMAND, ATA_CMD_IDENTIFY_PACKET);

            uint8_t status = inb(drive->io + ATA_STATUS_COMMAND);
            if (status == 0 || status == 0xFF)
            {
                return false;
            }
            if (!wait_for_status(*drive, true))
            {
                return false;
            }

            uint16_t identify_data[256];
            for (uint32_t index = 0; index < 256; ++index)
            {
                identify_data[index] = inw(drive->io + ATA_DATA);
            }

            const uint16_t configuration = identify_data[0];
            const uint8_t peripheral_type =
                static_cast<uint8_t>((configuration >> 8) & 0x1F);
            if ((configuration & 0x8000) == 0 || peripheral_type != 5)
            {
                return false;
            }

            drive->present = true;
            return true;
        }

        bool read_command(const device& drive, uint32_t lba, uint8_t count,
                          uint8_t* output)
        {
            if (!drive.present || output == nullptr || count == 0 ||
                count > MAX_SECTORS_PER_COMMAND ||
                lba > UINT32_MAX - static_cast<uint32_t>(count - 1))
            {
                return false;
            }

            const uint16_t transfer_bytes =
                static_cast<uint16_t>(static_cast<uint32_t>(count) * SECTOR_SIZE);

            if (!wait_for_status(drive, false))
            {
                return false;
            }

            outb(drive.io + ATA_DRIVE_HEAD, drive.drive_select);
            delay_400ns(drive.control);
            outb(drive.io + ATA_FEATURES, 0);
            outb(drive.io + ATA_LBA_MID, static_cast<uint8_t>(transfer_bytes));
            outb(drive.io + ATA_LBA_HIGH,
                 static_cast<uint8_t>(transfer_bytes >> 8));
            outb(drive.io + ATA_STATUS_COMMAND, ATA_CMD_PACKET);

            if (!wait_for_status(drive, true))
            {
                return false;
            }

            uint8_t packet[12] = {};
            packet[0] = ATAPI_CMD_READ_12;
            packet[2] = static_cast<uint8_t>(lba >> 24);
            packet[3] = static_cast<uint8_t>(lba >> 16);
            packet[4] = static_cast<uint8_t>(lba >> 8);
            packet[5] = static_cast<uint8_t>(lba);
            packet[6] = 0;
            packet[7] = 0;
            packet[8] = 0;
            packet[9] = count;

            for (uint32_t index = 0; index < sizeof(packet); index += 2)
            {
                const uint16_t word = static_cast<uint16_t>(packet[index]) |
                    (static_cast<uint16_t>(packet[index + 1]) << 8);
                outw(drive.io + ATA_DATA, word);
            }

            const uint32_t expected_bytes =
                static_cast<uint32_t>(count) * SECTOR_SIZE;
            uint32_t received_bytes = 0;
            uint32_t idle_attempts = 0;
            while (received_bytes < expected_bytes)
            {
                bool data_ready = false;
                for (uint32_t attempt = 0; attempt < ATA_TIMEOUT; ++attempt)
                {
                    const uint8_t status = inb(drive.io + ATA_STATUS_COMMAND);
                    if (status == 0 || status == 0xFF ||
                        (status & ATA_STATUS_ERR) != 0)
                    {
                        return false;
                    }
                    if ((status & ATA_STATUS_BSY) != 0)
                    {
                        continue;
                    }
                    if ((status & ATA_STATUS_DRQ) != 0)
                    {
                        data_ready = true;
                        break;
                    }
                    if (++idle_attempts >= ATA_TIMEOUT)
                    {
                        return false;
                    }
                }

                if (!data_ready)
                {
                    return false;
                }

                const uint32_t phase_bytes =
                    static_cast<uint32_t>(inb(drive.io + ATA_LBA_MID)) |
                    (static_cast<uint32_t>(inb(drive.io + ATA_LBA_HIGH)) << 8);
                if (phase_bytes == 0 || phase_bytes > expected_bytes - received_bytes)
                {
                    return false;
                }

                const uint32_t words = (phase_bytes + 1) / 2;
                for (uint32_t index = 0; index < words; ++index)
                {
                    const uint16_t word = inw(drive.io + ATA_DATA);
                    const uint32_t offset = received_bytes + index * 2;
                    if (offset < expected_bytes)
                    {
                        output[offset] = static_cast<uint8_t>(word);
                    }
                    if (index * 2 + 1 < phase_bytes &&
                        offset + 1 < expected_bytes)
                    {
                        output[offset + 1] = static_cast<uint8_t>(word >> 8);
                    }
                }
                received_bytes += phase_bytes;
            }

            return wait_for_status(drive, false);
        }
    }

    bool init()
    {
        device candidates[] =
        {
            {ATA_SECONDARY_IO, ATA_SECONDARY_CONTROL, 0xA0, false},
            {ATA_SECONDARY_IO, ATA_SECONDARY_CONTROL, 0xB0, false},
            {ATA_PRIMARY_IO, ATA_PRIMARY_CONTROL, 0xA0, false},
            {ATA_PRIMARY_IO, ATA_PRIMARY_CONTROL, 0xB0, false}
        };

        for (device& candidate : candidates)
        {
            outb(candidate.control, 0x00);
            delay_400ns(candidate.control);
            if (identify_packet_device(&candidate))
            {
                optical_drive = candidate;
                return true;
            }
        }

        optical_drive = {};
        return false;
    }

    bool read_sectors(uint32_t lba, uint8_t count, void* buffer)
    {
        if (buffer == nullptr || count == 0 ||
            lba > UINT32_MAX - static_cast<uint32_t>(count - 1))
        {
            return false;
        }

        uint8_t* output = static_cast<uint8_t*>(buffer);
        uint32_t remaining = count;
        while (remaining > 0)
        {
            const uint8_t chunk = static_cast<uint8_t>(
                remaining > MAX_SECTORS_PER_COMMAND
                    ? MAX_SECTORS_PER_COMMAND
                    : remaining);
            if (!read_command(optical_drive, lba, chunk, output))
            {
                return false;
            }

            lba += chunk;
            output += static_cast<uint32_t>(chunk) * SECTOR_SIZE;
            remaining -= chunk;
        }

        return true;
    }
}
