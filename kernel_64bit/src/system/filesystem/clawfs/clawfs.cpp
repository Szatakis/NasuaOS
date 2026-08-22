#include "clawfs.hpp"

#include "drivers/gpu/driver.hpp"
#include "drivers/disk/driver.hpp"
#include "drivers/memory/driver.hpp"

#include "applications/shell/commands.hpp"

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
// First dynamically allocated sector = 108
//

static uint32_t next_free_sector = 108;

uint32_t get_next_free_sector()
{
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
    clawfs_create_file_in("/bin", "kernel_bin");


    clawfs_mkdir("/home", "user");
    clawfs_mkdir("/home", "root");

    clawfs_mkdir("/home/user","desktop");
    clawfs_mkdir("/home/user", "Documents");
    clawfs_mkdir("/home/user", "images");
    clawfs_mkdir("/home/user","videos");

    Gpu::print_info("Format complete.\n");
}

// Format Clear CLAWFS
void clawfs_format_clr()
{
    Gpu::print_info("Formatting CLAWFS...\n");

    // Reset allocation pointer.
    next_free_sector = 108;


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

    return true;
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