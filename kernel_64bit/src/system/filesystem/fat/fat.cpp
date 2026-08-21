#include "fat.hpp"

#include "system/drivers/memory/driver.hpp"

#define FAT_ENTRY_SIZE 32

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_LONG_NAME 0x0F

struct fat_directory
{
    bool fixed_root;
    uint32_t cluster;
};

typedef bool (*fat_directory_callback)(const fat_entry_info* entry, void* user_data);

static char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c - 'A' + 'a');
    }

    return c;
}

static bool name_equals(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (to_lower(*a) != to_lower(*b))
        {
            return false;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static uint16_t read16(const uint8_t* data)
{
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t read32(const uint8_t* data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static const uint8_t* sector_pointer(fat_volume* volume, uint32_t sector)
{
    uint64_t offset = (uint64_t)sector * volume->bytes_per_sector;

    if (offset + volume->bytes_per_sector > volume->image_size)
    {
        return nullptr;
    }

    return volume->image + offset;
}

static uint32_t cluster_first_sector(fat_volume* volume, uint32_t cluster)
{
    return volume->data_sector + (cluster - 2) * volume->sectors_per_cluster;
}

static uint32_t next_cluster(fat_volume* volume, uint32_t cluster)
{
    const uint8_t* fat = volume->image + (uint64_t)volume->fat_sector * volume->bytes_per_sector;

    uint64_t fat_size = (uint64_t)volume->sectors_per_fat * volume->bytes_per_sector;

    if (volume->type == FAT_TYPE_32)
    {
        uint64_t offset = (uint64_t)cluster * 4;

        if (offset + 4 > fat_size)
        {
            return 0x0FFFFFFF;
        }

        return read32(fat + offset) & 0x0FFFFFFF;
    }

    if (volume->type == FAT_TYPE_16)
    {
        uint64_t offset = (uint64_t)cluster * 2;

        if (offset + 2 > fat_size)
        {
            return 0xFFFF;
        }

        return read16(fat + offset);
    }

    uint64_t offset = (uint64_t)cluster + ((uint64_t)cluster / 2);

    if (offset + 2 > fat_size)
    {
        return 0xFFF;
    }

    uint16_t value = read16(fat + offset);

    if (cluster & 1)
    {
        return (uint32_t)(value >> 4);
    }

    return (uint32_t)(value & 0x0FFF);
}

static bool is_last_cluster(fat_volume* volume, uint32_t cluster)
{
    if (cluster < 2)
    {
        return true;
    }

    if (volume->type == FAT_TYPE_32)
    {
        return cluster >= 0x0FFFFFF8;
    }

    if (volume->type == FAT_TYPE_16)
    {
        return cluster >= 0xFFF8;
    }

    return cluster >= 0x0FF8;
}

static void short_name_to_string(const uint8_t* entry, char* out)
{
    int length = 0;

    for (int i = 0; i < 8; i++)
    {
        if (entry[i] == ' ')
        {
            break;
        }

        out[length++] = to_lower((char)entry[i]);
    }

    if (entry[8] != ' ')
    {
        out[length++] = '.';

        for (int i = 8; i < 11; i++)
        {
            if (entry[i] == ' ')
            {
                break;
            }

            out[length++] = to_lower((char)entry[i]);
        }
    }

    out[length] = '\0';
}

static void long_name_chunk(const uint8_t* entry, uint32_t order, char* name)
{
    static const int offsets[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };

    uint32_t base = (order - 1) * 13;

    for (int i = 0; i < 13; i++)
    {
        uint32_t index = base + (uint32_t)i;

        if (index >= FAT_MAX_NAME - 1)
        {
            break;
        }

        uint16_t value = read16(entry + offsets[i]);

        if (value == 0x0000 || value == 0xFFFF)
        {
            if (name[index] == '\0')
            {
                break;
            }

            continue;
        }

        name[index] = (value < 128) ? (char)value : '?';
    }
}

static void iterate_directory(fat_volume* volume, fat_directory directory, fat_directory_callback callback, void* user_data)
{
    char long_name[FAT_MAX_NAME];
    bool has_long_name = false;

    for (uint32_t i = 0; i < FAT_MAX_NAME; i++)
    {
        long_name[i] = '\0';
    }

    uint32_t cluster = directory.cluster;
    uint32_t sector = directory.fixed_root ? volume->root_sector : 0;
    uint32_t sectors_left = directory.fixed_root ? volume->root_sectors : 0;

    while (true)
    {
        if (!directory.fixed_root)
        {
            if (cluster < 2 || is_last_cluster(volume, cluster))
            {
                return;
            }

            sector = cluster_first_sector(volume, cluster);
            sectors_left = volume->sectors_per_cluster;
        }

        for (uint32_t s = 0; s < sectors_left; s++)
        {
            const uint8_t* data = sector_pointer(volume, sector + s);

            if (data == nullptr)
            {
                return;
            }

            for (uint32_t offset = 0; offset < volume->bytes_per_sector; offset += FAT_ENTRY_SIZE)
            {
                const uint8_t* entry = data + offset;

                if (entry[0] == 0x00)
                {
                    return;
                }

                if (entry[0] == 0xE5)
                {
                    has_long_name = false;

                    continue;
                }

                uint8_t attributes = entry[11];

                if ((attributes & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME)
                {
                    uint32_t order = entry[0] & 0x1F;

                    if (entry[0] & 0x40)
                    {
                        for (uint32_t i = 0; i < FAT_MAX_NAME; i++)
                        {
                            long_name[i] = '\0';
                        }
                    }

                    if (order >= 1)
                    {
                        long_name_chunk(entry, order, long_name);

                        has_long_name = true;
                    }

                    continue;
                }

                if (attributes & FAT_ATTR_VOLUME_ID)
                {
                    has_long_name = false;

                    continue;
                }

                fat_entry_info info;

                if (has_long_name)
                {
                    for (uint32_t i = 0; i < FAT_MAX_NAME; i++)
                    {
                        info.name[i] = long_name[i];
                    }

                    info.name[FAT_MAX_NAME - 1] = '\0';
                }
                else
                {
                    short_name_to_string(entry, info.name);
                }

                info.directory = (attributes & FAT_ATTR_DIRECTORY) != 0;
                info.size = read32(entry + 28);
                info.first_cluster = ((uint32_t)read16(entry + 20) << 16) | (uint32_t)read16(entry + 26);

                has_long_name = false;

                if (!callback(&info, user_data))
                {
                    return;
                }
            }
        }

        if (directory.fixed_root)
        {
            return;
        }

        cluster = next_cluster(volume, cluster);
    }
}

struct fat_search
{
    const char* name;
    uint32_t name_length;

    fat_entry_info result;

    bool found;
};

static bool search_callback(const fat_entry_info* entry, void* user_data)
{
    fat_search* search = (fat_search*)user_data;

    char wanted[FAT_MAX_NAME];

    uint32_t length = search->name_length;

    if (length >= FAT_MAX_NAME)
    {
        length = FAT_MAX_NAME - 1;
    }

    for (uint32_t i = 0; i < length; i++)
    {
        wanted[i] = search->name[i];
    }

    wanted[length] = '\0';

    if (!name_equals(entry->name, wanted))
    {
        return true;
    }

    search->result = *entry;
    search->found = true;

    return false;
}

static fat_directory root_directory(fat_volume* volume)
{
    fat_directory directory;

    directory.fixed_root = (volume->type != FAT_TYPE_32);
    directory.cluster = volume->root_cluster;

    return directory;
}

// Resolves an absolute path. Returns false when a component does not exist.
static bool resolve_path(fat_volume* volume, const char* path, fat_entry_info* info, fat_directory* directory)
{
    fat_directory current = root_directory(volume);

    fat_entry_info current_info;

    current_info.name[0] = '/';
    current_info.name[1] = '\0';
    current_info.directory = true;
    current_info.size = 0;
    current_info.first_cluster = volume->root_cluster;

    const char* cursor = path;

    while (*cursor == '/')
    {
        cursor++;
    }

    while (*cursor != '\0')
    {
        const char* component = cursor;
        uint32_t length = 0;

        while (cursor[length] != '\0' && cursor[length] != '/')
        {
            length++;
        }

        cursor += length;

        while (*cursor == '/')
        {
            cursor++;
        }

        if (length == 0)
        {
            continue;
        }

        fat_search search;

        search.name = component;
        search.name_length = length;
        search.found = false;

        iterate_directory(volume, current, search_callback, &search);

        if (!search.found)
        {
            return false;
        }

        current_info = search.result;

        if (*cursor != '\0' && !current_info.directory)
        {
            return false;
        }

        if (current_info.directory)
        {
            current.fixed_root = false;
            current.cluster = current_info.first_cluster;
        }
    }

    if (info != nullptr)
    {
        *info = current_info;
    }

    if (directory != nullptr)
    {
        *directory = current;
    }

    return true;
}

bool fat_mount(fat_volume* volume, const void* image, uint64_t image_size)
{
    if (volume == nullptr || image == nullptr || image_size < 512)
    {
        return false;
    }

    const uint8_t* boot = (const uint8_t*)image;

    volume->mounted = false;
    volume->image = boot;
    volume->image_size = image_size;

    volume->bytes_per_sector = read16(boot + 11);
    volume->sectors_per_cluster = boot[13];
    volume->reserved_sectors = read16(boot + 14);
    volume->fat_count = boot[16];
    volume->root_entry_count = read16(boot + 17);

    if (volume->bytes_per_sector == 0 || volume->sectors_per_cluster == 0 || volume->fat_count == 0)
    {
        return false;
    }

    volume->total_sectors = read16(boot + 19);

    if (volume->total_sectors == 0)
    {
        volume->total_sectors = read32(boot + 32);
    }

    volume->sectors_per_fat = read16(boot + 22);

    if (volume->sectors_per_fat == 0)
    {
        volume->sectors_per_fat = read32(boot + 36);

        volume->root_cluster = read32(boot + 44);
    }
    else
    {
        volume->root_cluster = 0;
    }

    if (volume->sectors_per_fat == 0 || volume->total_sectors == 0)
    {
        return false;
    }

    volume->fat_sector = volume->reserved_sectors;
    volume->root_sectors = ((volume->root_entry_count * 32) + (volume->bytes_per_sector - 1)) / volume->bytes_per_sector;
    volume->root_sector = volume->fat_sector + (volume->fat_count * volume->sectors_per_fat);
    volume->data_sector = volume->root_sector + volume->root_sectors;

    if (volume->data_sector >= volume->total_sectors)
    {
        return false;
    }

    volume->cluster_count = (volume->total_sectors - volume->data_sector) / volume->sectors_per_cluster;

    if (volume->cluster_count < 4085)
    {
        volume->type = FAT_TYPE_12;
    }
    else if (volume->cluster_count < 65525)
    {
        volume->type = FAT_TYPE_16;
    }
    else
    {
        volume->type = FAT_TYPE_32;
    }

    if (volume->type == FAT_TYPE_32 && volume->root_cluster < 2)
    {
        volume->root_cluster = 2;
    }

    volume->mounted = true;

    return true;
}

bool fat_stat(fat_volume* volume, const char* path, fat_entry_info* info)
{
    if (volume == nullptr || !volume->mounted || path == nullptr)
    {
        return false;
    }

    return resolve_path(volume, path, info, nullptr);
}

bool fat_read_file(fat_volume* volume, const char* path, void* buffer, uint32_t buffer_size, uint32_t* read_size)
{
    if (volume == nullptr || !volume->mounted || path == nullptr || buffer == nullptr)
    {
        return false;
    }

    fat_entry_info info;

    if (!resolve_path(volume, path, &info, nullptr))
    {
        return false;
    }

    if (info.directory)
    {
        return false;
    }

    uint32_t remaining = info.size;

    if (remaining > buffer_size)
    {
        return false;
    }

    uint8_t* output = (uint8_t*)buffer;

    uint32_t cluster = info.first_cluster;
    uint32_t copied = 0;

    uint32_t cluster_size = volume->sectors_per_cluster * volume->bytes_per_sector;

    while (remaining > 0)
    {
        if (cluster < 2 || is_last_cluster(volume, cluster))
        {
            return false;
        }

        const uint8_t* data = sector_pointer(volume, cluster_first_sector(volume, cluster));

        if (data == nullptr)
        {
            return false;
        }

        uint32_t chunk = (remaining < cluster_size) ? remaining : cluster_size;

        Memory::memcpy(output + copied, data, chunk);

        copied += chunk;
        remaining -= chunk;

        cluster = next_cluster(volume, cluster);
    }

    if (read_size != nullptr)
    {
        *read_size = copied;
    }

    return true;
}

struct fat_listing
{
    fat_entry_info* entries;

    uint32_t max_entries;
    uint32_t count;
};

static bool listing_callback(const fat_entry_info* entry, void* user_data)
{
    fat_listing* listing = (fat_listing*)user_data;

    if (name_equals(entry->name, ".") || name_equals(entry->name, ".."))
    {
        return true;
    }

    listing->entries[listing->count] = *entry;
    listing->count++;

    return listing->count < listing->max_entries;
}

uint32_t fat_list_directory(fat_volume* volume, const char* path, fat_entry_info* entries, uint32_t max_entries)
{
    if (volume == nullptr || !volume->mounted || entries == nullptr || max_entries == 0)
    {
        return 0;
    }

    fat_entry_info info;
    fat_directory directory;

    if (!resolve_path(volume, path, &info, &directory))
    {
        return 0;
    }

    if (!info.directory)
    {
        return 0;
    }

    fat_listing listing;

    listing.entries = entries;
    listing.max_entries = max_entries;
    listing.count = 0;

    iterate_directory(volume, directory, listing_callback, &listing);

    return listing.count;
}
