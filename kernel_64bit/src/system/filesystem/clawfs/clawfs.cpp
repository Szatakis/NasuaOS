#include "clawfs.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/disk/driver.hpp"
#include "drivers/memory/driver.hpp"

#include "applications/shell/shell.hpp"

#include "libs/libc/libc.hpp"

// Helpers
char* next_path_token(char* str, const char* delim)
{
    static char* backup_str = nullptr;

    if (str != nullptr)
    {
        backup_str = str;
    }

    if (backup_str == nullptr)
    {
        return nullptr;
    }

    char* start = backup_str;

    while (*backup_str == *delim)
    {
        backup_str++;
        start++;
    }

    if (*backup_str == '\0')
    {
        return nullptr;
    }

    while (*backup_str != '\0')
    {
        if (*backup_str == *delim)
        {
            *backup_str = '\0';
            backup_str++;

            return start;
        }

        backup_str++;
    }

    return start;
}

// Sector allocation
//
// 100 = CLAWFS header
// 101 = root
// 102 = bin
// 103 = sbin
// 104 = dev
// 105 = mnt
// 106 = home
// 107 = etc
// 108 = var
// 109 = lib
// 110 = tmp
//
// First dynamically allocated sector = 112
//

static uint32_t next_free_sector = 112;
static bool allocator_initialized = false;

static void update_allocator_from_directory(uint32_t dir_sector, uint32_t depth)
{
    if (depth >= 32 || dir_sector == 0)
    {
        return;
    }

    uint8_t buffer[512];
    if (!Disk::read_sector(dir_sector, buffer))
    {
        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;
    for (uint32_t i = 0; i < 12; ++i)
    {
        if (entries[i].name[0] == '\0' || entries[i].data_sector == 0)
        {
            continue;
        }

        uint32_t sector_count = 1;
        if (entries[i].type == CLAWFS_FILE &&
            entries[i].entry_count > 512 &&
            entries[i].entry_count <= CLAWFS_MAX_FILE_SIZE)
        {
            sector_count = (entries[i].entry_count + 511) / 512;
        }

        const uint32_t sector_end = entries[i].data_sector + sector_count;
        if (sector_end > next_free_sector)
        {
            next_free_sector = sector_end;
        }

        if (entries[i].type == CLAWFS_DIRECTORY)
        {
            update_allocator_from_directory(entries[i].data_sector, depth + 1);
        }
    }
}

static bool initialize_allocator()
{
    if (allocator_initialized)
    {
        return true;
    }
    if (!clawfs_exists())
    {
        return false;
    }

    next_free_sector = 112;
    update_allocator_from_directory(CLAWFS_ROOT_SECTOR, 0);
    allocator_initialized = true;
    return true;
}

uint32_t get_next_free_sector()
{
    initialize_allocator();
    return next_free_sector++;
}


// Find entry in directory
uint32_t find_entry_in_dir(uint32_t dir_sector, const char* name, CLAWFSEntry* out_entry)
{
    uint8_t buffer[512];

    if (!Disk::read_sector(dir_sector, buffer))
    {
        return 0;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++)
    {
        if (entries[i].name[0] == '\0')
        {
            continue;
        }

        if (strcmp(entries[i].name, name) == 0)
        {
            if (out_entry != nullptr)
            {
                *out_entry = entries[i];
            }

            return entries[i].data_sector;
        }
    }

    return 0;
}


// Get sector by path
uint32_t get_sector_by_path(const char* path)
{
    if (path == nullptr)
    {
        return 0;
    }

    // Make sure CLAWFS is actually formatted.
    if (!clawfs_exists())
    {
        return 0;
    }

    if (strcmp(path, "/") == 0)
    {
        return CLAWFS_ROOT_SECTOR;
    }

    uint32_t current_dir = CLAWFS_ROOT_SECTOR;

    char path_copy[128];

    memclear(path_copy, sizeof(path_copy));
    strcpy(path_copy, path);

    char* token = next_path_token(path_copy, "/");

    CLAWFSEntry entry;

    while (token != nullptr)
    {
        uint32_t next_sector = find_entry_in_dir(current_dir, token, &entry);

        if (next_sector == 0)
        {
            return 0;
        }

        current_dir = entry.data_sector;

        token = next_path_token(nullptr, "/");
    }

    return current_dir;
}


// Setup directory/file entry
void setup_entry(CLAWFSEntry* e, const char* name, uint32_t type, uint32_t sector)
{
    if (e == nullptr)
    {
        return;
    }

    memclear(e, sizeof(CLAWFSEntry));

    strcpy(e->name, name);

    e->type = type;
    e->data_sector = sector;
    e->entry_count = 0;

    // Clear the sector belonging to the entry.
    uint8_t zero_buf[512];

    memclear(zero_buf, sizeof(zero_buf));

    Disk::write_sector(sector, zero_buf);
}


// Format CLAWFS
void clawfs_format()
{
    Gpu::print_info("Formatting CLAWFS...\n");

    // Reset allocation pointer.
    next_free_sector = 112;
    allocator_initialized = false;


    // Header
    uint8_t buffer[512];

    memclear(buffer, sizeof(buffer));

    CLAWFSHeader* header = (CLAWFSHeader*)buffer;

    Memory::memcpy(header->signature, "CLAWFS", 6);

    header->version = CLAWFS_VERSION;
    header->entryCount = 0;

    Disk::write_sector(CLAWFS_START_LBA, buffer);


    // Root directory
    uint8_t root_buffer[512];

    memclear(root_buffer, sizeof(root_buffer));

    CLAWFSEntry* entries = (CLAWFSEntry*)root_buffer;

    setup_entry(&entries[0], "bin", CLAWFS_DIRECTORY, 102);
    setup_entry(&entries[1], "sbin", CLAWFS_DIRECTORY, 103);
    setup_entry(&entries[2], "dev", CLAWFS_DIRECTORY, 104);
    setup_entry(&entries[3], "mnt", CLAWFS_DIRECTORY, 105);
    setup_entry(&entries[4], "home", CLAWFS_DIRECTORY,106);
    setup_entry(&entries[5], "etc", CLAWFS_DIRECTORY, 107);
    setup_entry(&entries[6], "var", CLAWFS_DIRECTORY, 108);
    setup_entry(&entries[7], "lib", CLAWFS_DIRECTORY, 109);
    setup_entry(&entries[8], "proc", CLAWFS_DIRECTORY, 110);
    setup_entry(&entries[9], "tmp", CLAWFS_DIRECTORY, 111);

    Disk::write_sector(CLAWFS_ROOT_SECTOR, root_buffer);


    // Default filesystem structure
    // Home directory
    clawfs_mkdir("/home", "user");
    clawfs_mkdir("/home", "root");

    clawfs_mkdir("/home/user", "desktop");
    clawfs_mkdir("/home/user", "documents");
    clawfs_mkdir("/home/user", "images");
    clawfs_mkdir("/home/user", "videos");
    clawfs_mkdir("/home/user", "downloads");
    clawfs_mkdir("/home/user", "uploads");
    clawfs_mkdir("/home/user", "music");
    clawfs_mkdir("/home/user", "projects");

    // Var directory
    clawfs_mkdir("/var", "log");
    clawfs_mkdir("/var", "cache");

    // Lib directory
    clawfs_mkdir("/lib", "drivers");
    clawfs_mkdir("/lib", "modules");
    clawfs_mkdir("/lib", "runtime");

    // Proc directory
    clawfs_create_file_in("/proc", "cpuinfo");
    clawfs_create_file_in("/proc", "meminfo");
    clawfs_create_file_in("/proc", "uptime");
    clawfs_create_file_in("/proc", "k_version");

    // Bin directory
    clawfs_create_file_in("/bin", "kernel_bin");


    Gpu::print_info("Format complete.\n");
}


// Format Clear CLAWFS
void clawfs_format_clr()
{
    Gpu::print_info("Formatting CLAWFS...\n");

    // Reset allocation pointer.
    next_free_sector = 102;


    // Header
    uint8_t buffer[512];

    memclear(buffer, sizeof(buffer));

    CLAWFSHeader* header = (CLAWFSHeader*)buffer;

    Memory::memcpy(header->signature, "CLAWFS", 6);

    header->version = CLAWFS_VERSION;
    header->entryCount = 0;

    Disk::write_sector(CLAWFS_START_LBA, buffer);


    // Root directory
    uint8_t root_buffer[512];
    memclear(root_buffer, sizeof(root_buffer));

    Disk::write_sector(CLAWFS_ROOT_SECTOR, root_buffer);

    Gpu::print_info("Format complete.\n");
}


// Format Distclear Disk
void clawfs_format_dclr()
{
    Gpu::print_info("Clearing CLAWFS...\n");

    uint8_t empty_sector[512];
    memclear(empty_sector, sizeof(empty_sector));

    uint32_t end_sector = next_free_sector;

    for (uint32_t sector = CLAWFS_START_LBA; sector < end_sector; sector++)
    {
        Disk::write_sector(sector, empty_sector);
    }

    next_free_sector = CLAWFS_START_LBA;

    Gpu::print_info("CLAWFS completely cleared.\n");
}


// Make directory
void clawfs_mkdir(const char* parent_path, const char* dir_name)
{
    if (parent_path == nullptr || dir_name == nullptr)
    {
        Gpu::print_error("Invalid path or directory name!\n");

        return;
    }

    uint32_t parent_sector = get_sector_by_path(parent_path);

    if (parent_sector == 0)
    {
        Gpu::print_error("Parent not found!\n");

        return;
    }

    uint8_t buffer[512];

    if (!Disk::read_sector(parent_sector, buffer))
    {
        Gpu::print_error("Failed to read parent directory!\n");

        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++)
    {
        if (entries[i].name[0] == '\0')
        {
            uint32_t sector = get_next_free_sector();

            setup_entry(&entries[i], dir_name, CLAWFS_DIRECTORY, sector);

            Disk::write_sector(parent_sector, buffer);

            Gpu::print_info("Dir created.\n");

            return;
        }
    }

    Gpu::print_error("Directory is full!\n");
}


// Create file inside directory
void clawfs_create_file_in(const char* path, const char* name)
{
    if (path == nullptr || name == nullptr)
    {
        Gpu::print_error("Invalid path or file name!\n");

        return;
    }

    if (name[0] == '\0' || strlen(name) >= 28)
    {
        Gpu::print_error("Invalid file name!\n");
        return;
    }

    uint32_t target_sector = get_sector_by_path(path);

    if (target_sector == 0)
    {
        Gpu::print_error("Path not found!\n");

        return;
    }

    uint8_t buffer[512];

    if (!Disk::read_sector(target_sector, buffer))
    {
        Gpu::print_error("Failed to read directory!\n");

        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++)
    {
        if (entries[i].name[0] == '\0')
        {
            memclear(&entries[i], sizeof(CLAWFSEntry));

            strcpy(entries[i].name, name);

            entries[i].type = CLAWFS_FILE;

            entries[i].data_sector = get_next_free_sector();

            entries[i].entry_count = 0;

            // Clear file data sector.
            uint8_t zero_buffer[512];

            memclear(zero_buffer, sizeof(zero_buffer));

            Disk::write_sector(entries[i].data_sector, zero_buffer);

            Disk::write_sector(target_sector, buffer);

            Gpu::print_info("File created.\n");

            return;
        }
    }

    Gpu::print_error("Directory is full!\n");
}


// List directory
void clawfs_dir(const char* path)
{
    if (path == nullptr)
    {
        Gpu::print_error("Invalid path!\n");

        return;
    }

    uint32_t target = get_sector_by_path(path);

    if (target == 0)
    {
        Gpu::print_error("Dir not found!\n");

        return;
    }

    uint8_t buffer[512];

    if (!Disk::read_sector(target, buffer))
    {
        Gpu::print_error("Failed to read directory!\n");

        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++)
    {
        if (entries[i].name[0] != '\0')
        {
            if (entries[i].type == CLAWFS_DIRECTORY)
            {
                Gpu::print("<DIR>  ");
            }
            else if(entries[i].type == CLAWFS_FILE)
            {
                Gpu::print("<FILE> ");
            }
            else
            {
                Gpu::print("       ");
            }

            Gpu::print(entries[i].name);
            Gpu::print("\n");
        }
    }
}


// Check if CLAWFS exists
bool clawfs_exists()
{
    uint8_t buffer[512];

    memclear(buffer, sizeof(buffer));

    if (!Disk::read_sector(CLAWFS_START_LBA, buffer))
    {
        return false;
    }

    CLAWFSHeader* header = (CLAWFSHeader*)buffer;

    if (Memory::memcmp(header->signature, "CLAWFS", 6) != 0)
    {
        return false;
    }

    if (header->version != CLAWFS_VERSION)
    {
        return false;
    }

    if (!allocator_initialized)
    {
        next_free_sector = 112;
        update_allocator_from_directory(CLAWFS_ROOT_SECTOR, 0);
        allocator_initialized = true;
    }
    return true;
}

static bool split_file_path(const char* path, char* parent_path, char* file_name)
{
    if (path == nullptr || parent_path == nullptr || file_name == nullptr || path[0] == '\0')
    {
        return false;
    }

    uint32_t length = 0;
    while (path[length] != '\0' && length < 255)
    {
        ++length;
    }
    if (path[length] != '\0' || path[length - 1] == '/')
    {
        return false;
    }

    uint32_t separator = length;
    while (separator > 0 && path[separator - 1] != '/')
    {
        --separator;
    }

    const uint32_t name_length = length - separator;
    if (name_length == 0 || name_length >= 28)
    {
        return false;
    }
    for (uint32_t i = 0; i < name_length; ++i)
    {
        file_name[i] = path[separator + i];
    }
    file_name[name_length] = '\0';

    if (separator == 0)
    {
        strcpy(parent_path, "/");
        return true;
    }

    uint32_t parent_length = separator == 1 ? 1 : separator - 1;
    if (parent_length >= 128)
    {
        return false;
    }
    for (uint32_t i = 0; i < parent_length; ++i)
    {
        parent_path[i] = path[i];
    }
    parent_path[parent_length] = '\0';
    return true;
}

static bool get_file_entry(const char* path, char* parent_path, uint32_t* parent_sector,
                           CLAWFSEntry* entry, uint8_t* directory_buffer)
{
    char file_name[28];
    if (!split_file_path(path, parent_path, file_name))
    {
        return false;
    }

    *parent_sector = get_sector_by_path(parent_path);
    if (*parent_sector == 0 || !Disk::read_sector(*parent_sector, directory_buffer))
    {
        return false;
    }

    if (find_entry_in_dir(*parent_sector, file_name, entry) == 0 || entry->type != CLAWFS_FILE)
    {
        return false;
    }
    return true;
}

bool clawfs_read_file(const char* path, void* buffer, uint32_t capacity, uint32_t* length)
{
    if (length == nullptr)
    {
        return false;
    }

    char parent_path[128];
    uint32_t parent_sector = 0;
    uint8_t directory_buffer[512];
    CLAWFSEntry entry;
    if (!get_file_entry(path, parent_path, &parent_sector, &entry, directory_buffer))
    {
        return false;
    }

    uint32_t file_length = entry.entry_count;
    if (file_length > CLAWFS_MAX_FILE_SIZE)
    {
        return false;
    }

    if (entry.entry_count == 0)
    {
        uint8_t legacy_buffer[512];
        if (!Disk::read_sector(entry.data_sector, legacy_buffer))
        {
            return false;
        }
        while (file_length < sizeof(legacy_buffer) && legacy_buffer[file_length] != '\0')
        {
            ++file_length;
        }
    }

    *length = file_length;
    if (capacity < file_length || (file_length != 0 && buffer == nullptr))
    {
        return false;
    }

    uint8_t sector_buffer[512];
    uint32_t copied = 0;
    while (copied < file_length)
    {
        if (!Disk::read_sector(entry.data_sector + copied / 512, sector_buffer))
        {
            return false;
        }

        uint32_t chunk_size = file_length - copied;
        if (chunk_size > sizeof(sector_buffer))
        {
            chunk_size = sizeof(sector_buffer);
        }
        Memory::memcpy((uint8_t*)buffer + copied, sector_buffer, chunk_size);
        copied += chunk_size;
    }
    return true;
}

bool clawfs_get_file_size(const char* path, uint32_t* size)
{
    if (size == nullptr)
    {
        return false;
    }

    char parent_path[128];
    uint32_t parent_sector = 0;
    uint8_t directory_buffer[512];
    CLAWFSEntry entry;
    if (!get_file_entry(path, parent_path, &parent_sector, &entry, directory_buffer) ||
        entry.entry_count == 0 || entry.entry_count > CLAWFS_MAX_FILE_SIZE)
    {
        return false;
    }

    *size = entry.entry_count;
    return true;
}

bool clawfs_read_file_range(const char* path, uint32_t offset, void* buffer, uint32_t length)
{
    char parent_path[128];
    uint32_t parent_sector = 0;
    uint8_t directory_buffer[512];
    CLAWFSEntry entry;
    if (!get_file_entry(path, parent_path, &parent_sector, &entry, directory_buffer) ||
        entry.entry_count == 0 || entry.entry_count > CLAWFS_MAX_FILE_SIZE ||
        offset > entry.entry_count || length > entry.entry_count - offset ||
        (length != 0 && buffer == nullptr))
    {
        return false;
    }

    uint8_t sector_buffer[512];
    uint32_t copied = 0;
    while (copied < length)
    {
        const uint32_t file_offset = offset + copied;
        const uint32_t sector_offset = file_offset % sizeof(sector_buffer);
        if (!Disk::read_sector(entry.data_sector + file_offset / sizeof(sector_buffer), sector_buffer))
        {
            return false;
        }

        uint32_t chunk_size = sizeof(sector_buffer) - sector_offset;
        if (chunk_size > length - copied)
        {
            chunk_size = length - copied;
        }
        Memory::memcpy((uint8_t*)buffer + copied, sector_buffer + sector_offset, chunk_size);
        copied += chunk_size;
    }

    return true;
}

bool clawfs_write_file(const char* path, const void* buffer, uint32_t length)
{
    if ((buffer == nullptr && length != 0) || length > CLAWFS_MAX_FILE_SIZE)
    {
        return false;
    }

    char parent_path[128];
    char file_name[28];
    if (!split_file_path(path, parent_path, file_name))
    {
        return false;
    }

    uint32_t parent_sector = get_sector_by_path(parent_path);
    if (parent_sector == 0)
    {
        return false;
    }

    uint8_t directory_buffer[512];
    if (!Disk::read_sector(parent_sector, directory_buffer))
    {
        return false;
    }

    CLAWFSEntry entry;
    uint32_t file_sector = find_entry_in_dir(parent_sector, file_name, &entry);
    if (file_sector == 0)
    {
        clawfs_create_file_in(parent_path, file_name);
        if (!Disk::read_sector(parent_sector, directory_buffer))
        {
            return false;
        }
        file_sector = find_entry_in_dir(parent_sector, file_name, &entry);
    }
    if (file_sector == 0 || entry.type != CLAWFS_FILE)
    {
        return false;
    }
    if (entry.entry_count > CLAWFS_MAX_FILE_SIZE)
    {
        return false;
    }

    const uint32_t existing_sector_count = entry.entry_count == 0
        ? 1
        : (entry.entry_count + 511) / 512;
    const uint32_t required_sector_count = length == 0 ? 1 : (length + 511) / 512;
    uint32_t target_sector = entry.data_sector;
    if (required_sector_count > existing_sector_count)
    {
        if (!initialize_allocator() || next_free_sector > UINT32_MAX - required_sector_count)
        {
            return false;
        }
        target_sector = next_free_sector;
        next_free_sector += required_sector_count;
    }

    uint8_t sector_buffer[512];
    uint32_t written = 0;
    for (uint32_t sector_index = 0; sector_index < required_sector_count; ++sector_index)
    {
        memclear(sector_buffer, sizeof(sector_buffer));
        uint32_t chunk_size = length - written;
        if (chunk_size > sizeof(sector_buffer))
        {
            chunk_size = sizeof(sector_buffer);
        }
        if (chunk_size != 0)
        {
            Memory::memcpy(sector_buffer, (const uint8_t*)buffer + written, chunk_size);
        }
        if (!Disk::write_sector(target_sector + sector_index, sector_buffer))
        {
            return false;
        }
        written += chunk_size;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)directory_buffer;
    for (uint32_t i = 0; i < 12; ++i)
    {
        if (strcmp(entries[i].name, file_name) == 0 && entries[i].type == CLAWFS_FILE)
        {
            entries[i].data_sector = target_sector;
            entries[i].entry_count = length;
            return Disk::write_sector(parent_sector, directory_buffer);
        }
    }
    return false;
}


// Remove file/directory
void clawfs_rm(const char* parent_path, const char* name, uint32_t type)
{
    if (parent_path == nullptr || name == nullptr)
    {
        Gpu::print_error("Invalid path or name!\n");

        return;
    }

    uint32_t parent_sector = get_sector_by_path(parent_path);

    if (parent_sector == 0)
    {
        Gpu::print_error("Parent directory not found!\n");

        return;
    }

    uint8_t buffer[512];

    if (!Disk::read_sector(parent_sector, buffer))
    {
        Gpu::print_error("Failed to read parent directory!\n");

        return;
    }

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++)
    {
        if (entries[i].name[0] == '\0')
        {
            continue;
        }

        if (strcmp(entries[i].name, name) != 0)
        {
            continue;
        }

        if (entries[i].type != type)
        {
            continue;
        }

        // Directory must be empty
        if (type == CLAWFS_DIRECTORY)
        {
            uint8_t dir_buffer[512];

            if (!Disk::read_sector(entries[i].data_sector, dir_buffer))
            {
                Gpu::print_error("Failed to read directory!\n");

                return;
            }

            CLAWFSEntry* sub_entries = (CLAWFSEntry*)dir_buffer;

            for (int j = 0; j < 12; j++)
            {
                if (sub_entries[j].name[0] != '\0')
                {
                    Gpu::print_error("Directory is not empty!\n");

                    return;
                }
            }
        }

        // Delete entry
        memclear(&entries[i], sizeof(CLAWFSEntry));

        if (!Disk::write_sector(parent_sector, buffer))
        {
            Gpu::print_error("Failed to update directory!\n");

            return;
        }

        Gpu::print_info("Removed successfully.\n");

        return;
    }

    if (type == CLAWFS_FILE)
    {
        Gpu::print_error("File not found!\n");
    }
    else if (type == CLAWFS_DIRECTORY)
    {
        Gpu::print_error("Directory not found!\n");
    }
}