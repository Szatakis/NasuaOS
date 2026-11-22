#include "iso9660.hpp"

#include "drivers/disk/atapi/driver.hpp"

namespace Iso9660
{
    namespace
    {
        constexpr uint32_t ISO_SECTOR_SIZE = 2048;
        constexpr uint32_t PRIMARY_VOLUME_DESCRIPTOR_LBA = 16;
        constexpr uint32_t MAX_VOLUME_DESCRIPTORS = 32;
        constexpr uint32_t MAX_COMPONENT_LENGTH = 63;
        constexpr uint8_t ISO_DIRECTORY_FLAG = 0x02;
        constexpr uint8_t ISO_MULTI_EXTENT_FLAG = 0x80;

        struct entry
        {
            uint32_t extent;
            uint32_t size;
            bool directory;
        };

        static bool mounted = false;
        static uint32_t volume_sector_count = 0;
        static entry root_directory{};

        uint16_t read_le16(const uint8_t* bytes)
        {
            return static_cast<uint16_t>(bytes[0]) |
                   (static_cast<uint16_t>(bytes[1]) << 8);
        }

        uint32_t read_le32(const uint8_t* bytes)
        {
            return static_cast<uint32_t>(bytes[0]) |
                   (static_cast<uint32_t>(bytes[1]) << 8) |
                   (static_cast<uint32_t>(bytes[2]) << 16) |
                   (static_cast<uint32_t>(bytes[3]) << 24);
        }

        bool entry_bounds_valid(const entry& item)
        {
            if (item.size == 0 || item.extent >= volume_sector_count)
            {
                return false;
            }

            const uint32_t sectors = item.size / ISO_SECTOR_SIZE +
                (item.size % ISO_SECTOR_SIZE != 0 ? 1 : 0);
            return sectors <= volume_sector_count - item.extent;
        }

        char ascii_upper(char value)
        {
            if (value >= 'a' && value <= 'z')
            {
                return static_cast<char>(value - ('a' - 'A'));
            }
            return value;
        }

        bool identifier_matches(const uint8_t* identifier, uint8_t length,
                                const char* component)
        {
            uint8_t identifier_length = 0;
            while (identifier_length < length &&
                   identifier[identifier_length] != ';')
            {
                ++identifier_length;
            }

            uint32_t component_length = 0;
            while (component[component_length] != '\0')
            {
                if (component_length >= MAX_COMPONENT_LENGTH)
                {
                    return false;
                }
                ++component_length;
            }

            if (component_length != identifier_length)
            {
                return false;
            }

            for (uint32_t index = 0; index < component_length; ++index)
            {
                if (ascii_upper(component[index]) !=
                    ascii_upper(static_cast<char>(identifier[index])))
                {
                    return false;
                }
            }
            return true;
        }

        bool find_child(const entry& directory, const char* component,
                        entry* result)
        {
            if (!directory.directory || result == nullptr ||
                !entry_bounds_valid(directory))
            {
                return false;
            }

            uint8_t sector[ISO_SECTOR_SIZE];
            uint32_t directory_offset = 0;
            while (directory_offset < directory.size)
            {
                const uint32_t sector_index = directory_offset / ISO_SECTOR_SIZE;
                if (!ATAPI::read_sectors(directory.extent + sector_index, 1, sector))
                {
                    return false;
                }

                const uint32_t bytes_in_sector =
                    directory.size - directory_offset < ISO_SECTOR_SIZE
                        ? directory.size - directory_offset
                        : ISO_SECTOR_SIZE;
                uint32_t record_offset = 0;
                while (record_offset < bytes_in_sector)
                {
                    const uint8_t record_length = sector[record_offset];
                    if (record_length == 0)
                    {
                        break;
                    }
                    if (record_length < 34 ||
                        record_length > bytes_in_sector - record_offset)
                    {
                        return false;
                    }

                    const uint8_t identifier_length = sector[record_offset + 32];
                    if (identifier_length == 0 ||
                        static_cast<uint32_t>(33 + identifier_length) > record_length)
                    {
                        return false;
                    }

                    const uint8_t* identifier = sector + record_offset + 33;
                    const bool special_identifier =
                        identifier_length == 1 &&
                        (identifier[0] == 0 || identifier[0] == 1);
                    if (!special_identifier &&
                        identifier_matches(identifier, identifier_length, component))
                    {
                        const uint8_t flags = sector[record_offset + 25];
                        if ((flags & ISO_MULTI_EXTENT_FLAG) != 0)
                        {
                            return false;
                        }

                        const uint32_t extent =
                            read_le32(sector + record_offset + 2);
                        const uint32_t extended_attribute_sectors =
                            sector[record_offset + 1];
                        if (extent > UINT32_MAX - extended_attribute_sectors)
                        {
                            return false;
                        }

                        entry found
                        {
                            extent + extended_attribute_sectors,
                            read_le32(sector + record_offset + 10),
                            (flags & ISO_DIRECTORY_FLAG) != 0
                        };
                        if (!entry_bounds_valid(found))
                        {
                            return false;
                        }
                        *result = found;
                        return true;
                    }

                    record_offset += record_length;
                }

                directory_offset += bytes_in_sector;
            }
            return false;
        }

        bool find_entry(const char* path, entry* result)
        {
            if (!mounted || path == nullptr || result == nullptr)
            {
                return false;
            }

            while (*path == '/')
            {
                ++path;
            }
            if (*path == '\0')
            {
                *result = root_directory;
                return true;
            }

            entry current = root_directory;
            while (*path != '\0')
            {
                char component[MAX_COMPONENT_LENGTH + 1];
                uint32_t component_length = 0;
                while (*path != '\0' && *path != '/')
                {
                    if (component_length >= MAX_COMPONENT_LENGTH)
                    {
                        return false;
                    }
                    component[component_length++] = *path++;
                }
                if (component_length == 0)
                {
                    return false;
                }
                component[component_length] = '\0';

                while (*path == '/')
                {
                    ++path;
                }

                entry child{};
                if (!find_child(current, component, &child))
                {
                    return false;
                }
                if (*path != '\0' && !child.directory)
                {
                    return false;
                }
                current = child;
            }

            *result = current;
            return true;
        }
    }

    bool init()
    {
        mounted = false;
        volume_sector_count = 0;
        root_directory = {};
        if (!ATAPI::init())
        {
            return false;
        }

        uint8_t descriptor[ISO_SECTOR_SIZE];
        for (uint32_t index = 0; index < MAX_VOLUME_DESCRIPTORS; ++index)
        {
            const uint32_t lba = PRIMARY_VOLUME_DESCRIPTOR_LBA + index;
            if (!ATAPI::read_sectors(lba, 1, descriptor) ||
                descriptor[1] != 'C' || descriptor[2] != 'D' ||
                descriptor[3] != '0' || descriptor[4] != '0' ||
                descriptor[5] != '1' || descriptor[6] != 1)
            {
                return false;
            }

            if (descriptor[0] == 255)
            {
                return false;
            }
            if (descriptor[0] != 1)
            {
                continue;
            }

            volume_sector_count = read_le32(descriptor + 80);
            if (read_le16(descriptor + 128) != ISO_SECTOR_SIZE ||
                volume_sector_count == 0)
            {
                return false;
            }

            const uint8_t* root_record = descriptor + 156;
            if (root_record[0] < 34 ||
                (root_record[25] & ISO_DIRECTORY_FLAG) == 0)
            {
                return false;
            }

            const uint32_t root_extent = read_le32(root_record + 2);
            const uint32_t root_extended_attribute_sectors = root_record[1];
            if (root_extent > UINT32_MAX - root_extended_attribute_sectors)
            {
                return false;
            }

            root_directory =
            {
                root_extent + root_extended_attribute_sectors,
                read_le32(root_record + 10),
                true
            };
            if (!entry_bounds_valid(root_directory))
            {
                return false;
            }

            mounted = true;
            return true;
        }
        return false;
    }

    bool file_size(const char* path, uint32_t* size)
    {
        if (size == nullptr)
        {
            return false;
        }
        entry found{};
        if (!find_entry(path, &found) || found.directory)
        {
            return false;
        }
        *size = found.size;
        return true;
    }

    bool read_file_range(const char* path, uint32_t offset,
                         void* buffer, uint32_t length)
    {
        entry found{};
        if (!find_entry(path, &found) || found.directory ||
            offset > found.size || length > found.size - offset ||
            (length != 0 && buffer == nullptr))
        {
            return false;
        }
        if (length == 0)
        {
            return true;
        }

        uint8_t* output = static_cast<uint8_t*>(buffer);
        uint8_t sector[ISO_SECTOR_SIZE];
        while (length > 0)
        {
            const uint32_t sector_index = offset / ISO_SECTOR_SIZE;
            const uint32_t sector_offset = offset % ISO_SECTOR_SIZE;
            const uint32_t lba = found.extent + sector_index;
            if (sector_offset == 0 && length >= ISO_SECTOR_SIZE)
            {
                uint32_t full_sectors = length / ISO_SECTOR_SIZE;
                if (full_sectors > 31)
                {
                    full_sectors = 31;
                }
                if (!ATAPI::read_sectors(lba,
                                         static_cast<uint8_t>(full_sectors),
                                         output))
                {
                    return false;
                }
                const uint32_t bytes_read = full_sectors * ISO_SECTOR_SIZE;
                output += bytes_read;
                offset += bytes_read;
                length -= bytes_read;
                continue;
            }

            if (!ATAPI::read_sectors(lba, 1, sector))
            {
                return false;
            }
            uint32_t bytes_to_copy = ISO_SECTOR_SIZE - sector_offset;
            if (bytes_to_copy > length)
            {
                bytes_to_copy = length;
            }
            for (uint32_t index = 0; index < bytes_to_copy; ++index)
            {
                output[index] = sector[sector_offset + index];
            }
            output += bytes_to_copy;
            offset += bytes_to_copy;
            length -= bytes_to_copy;
        }
        return true;
    }
}
