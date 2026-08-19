#include "file_resolver.hpp"

#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/filesystem/fat/fat.hpp"
#include "system/drivers/disk/ata/driver.hpp"
#include "system/drivers/gpu/driver.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/applications/napp/napp.hpp"
#include "libs/libc/libc.hpp"

// External FAT volume from napp
extern fat_volume rootfs_volume;
extern bool rootfs_mounted;

// External storage flag from storage.cpp
extern bool storage_uses_ram;

// Mount state
static bool clawfs_mounted = false;

// Storage detection
bool storage_uses_ata()
{
    return !storage_uses_ram;
}

void file_resolver_init()
{
    clawfs_mounted = false;
}

void file_resolver_mount(bool mount)
{
    clawfs_mounted = mount;
    
    if (mount) {
        print_info("ClawFS overlay mounted.\n");
        Uart::puts("[File Resolver] ClawFS overlay mounted.\n");
    } else {
        print_info("ClawFS overlay unmounted.\n");
        Uart::puts("[File Resolver] ClawFS overlay unmounted.\n");
    }
}

bool file_resolver_is_mounted()
{
    return clawfs_mounted;
}

// Check if file exists in ClawFS
static bool clawfs_file_exists(const char* path)
{
    uint32_t sector = get_sector_by_path(path);
    return sector != 0;
}

// Get ClawFS file entry info
static bool clawfs_get_file_info(const char* path, uint32_t* data_sector, uint32_t* size)
{
    uint32_t sector = get_sector_by_path(path);
    if (sector == 0) {
        return false;
    }
    
    // Extract parent path and filename
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash == nullptr || last_slash == path_copy) {
        return false;
    }
    
    *last_slash = '\0';
    const char* filename = last_slash + 1;
    
    uint32_t parent_sector = get_sector_by_path(path_copy);
    if (parent_sector == 0) {
        return false;
    }
    
    CLAWFSEntry entry;
    if (find_entry_in_dir(parent_sector, filename, &entry) == 0) {
        return false;
    }
    
    if (entry.type != CLAWFS_FILE) {
        return false;
    }
    
    if (data_sector) *data_sector = entry.data_sector;
    if (size) *size = 512; // ClawFS files are single sector for now
    
    return true;
}

// Centralized file resolution
file_resolve_result_t resolve_system_file(const char* path)
{
    file_resolve_result_t result = {FILE_SOURCE_NONE, false, 0, 0};
    
    if (path == nullptr || *path == '\0') {
        return result;
    }
    
    // Priority 1: ClawFS (if mounted)
    if (clawfs_mounted && clawfs_file_exists(path)) {
        uint32_t data_sector;
        if (clawfs_get_file_info(path, &data_sector, &result.size)) {
            result.source = FILE_SOURCE_CLAWFS;
            result.exists = true;
            result.data_sector = data_sector;
            return result;
        }
    }
    
    // Priority 2: rootfs (ISO)
    if (rootfs_mounted) {
        fat_entry_info info;
        if (fat_stat(&rootfs_volume, path, &info) && !info.directory) {
            result.source = FILE_SOURCE_ROOTFS;
            result.exists = true;
            result.size = info.size;
            return result;
        }
    }
    
    return result;
}

bool system_file_exists(const char* path)
{
    file_resolve_result_t result = resolve_system_file(path);
    return result.exists;
}

// Copy file from rootfs to ClawFS
bool copy_file_to_clawfs(const char* src_path, const char* dst_path)
{
    if (!rootfs_mounted || src_path == nullptr || dst_path == nullptr) {
        return false;
    }
    
    // Get source file info from rootfs
    fat_entry_info src_info;
    if (!fat_stat(&rootfs_volume, src_path, &src_info) || src_info.directory) {
        return false;
    }
    
    if (src_info.size == 0) {
        print_error("Empty file in rootfs\n");
        return false;
    }
    
    // Extract parent path and filename for destination
    char dst_path_copy[256];
    strcpy(dst_path_copy, dst_path);
    
    char* last_slash = strrchr(dst_path_copy, '/');
    if (last_slash == nullptr || last_slash == dst_path_copy) {
        return false;
    }
    
    *last_slash = '\0';
    const char* filename = last_slash + 1;
    
    // Check if file already exists in ClawFS
    if (clawfs_file_exists(dst_path)) {
        // Skip copying if already present
        return true;
    }
    
    // Create file in ClawFS
    clawfs_create_file_in(dst_path_copy, filename);
    
    // Get the newly created file's sector
    uint32_t parent_sector = get_sector_by_path(dst_path_copy);
    if (parent_sector == 0) {
        return false;
    }
    
    CLAWFSEntry entry;
    if (find_entry_in_dir(parent_sector, filename, &entry) == 0) {
        return false;
    }
    
    // Read file from rootfs (limited to 512 bytes for ClawFS)
    void* buffer = kmalloc(512);
    if (buffer == nullptr) {
        return false;
    }
    
    memclear(buffer, 512);
    
    uint32_t bytes_to_copy = src_info.size > 512 ? 512 : src_info.size;
    uint32_t read_size = 0;
    
    if (!fat_read_file(&rootfs_volume, src_path, buffer, bytes_to_copy, &read_size) || read_size != bytes_to_copy) {
        kfree(buffer);
        return false;
    }
    
    // Write to ClawFS sector
    storage_write_sector(entry.data_sector, (uint8_t*)buffer);
    
    kfree(buffer);
    
    print_info("Copied: ");
    print(src_path);
    print(" -> ");
    print(dst_path);
    print(" (");
    print_num8(bytes_to_copy);
    print(" bytes)\n");
    
    return true;
}

// Format commands: copy all from rootfs /bin and /sbin to ClawFS
bool format_commands()
{
    if (!rootfs_mounted) {
        print_error("Rootfs not available\n");
        return false;
    }
    
    print_info("Formatting commands - copying from rootfs to ClawFS...\n");
    Uart::puts("[File Resolver] Formatting commands...\n");
    
    // List and copy /bin files
    static fat_entry_info bin_entries[64];
    uint32_t bin_count = fat_list_directory(&rootfs_volume, "/bin", bin_entries, 64);
    
    uint32_t copied = 0;
    uint32_t skipped = 0;
    
    print_info("Processing /bin directory...\n");
    
    for (uint32_t i = 0; i < bin_count; i++) {
        if (bin_entries[i].directory || bin_entries[i].name[0] == '.') {
            continue;
        }
        
        char src_path[256];
        char dst_path[256];
        
        strcpy(src_path, "/bin/");
        strcat(src_path, bin_entries[i].name);
        
        strcpy(dst_path, "/bin/");
        strcat(dst_path, bin_entries[i].name);
        
        print("  Processing: ");
        print(bin_entries[i].name);
        print("...");
        
        if (copy_file_to_clawfs(src_path, dst_path)) {
            copied++;
        } else {
            skipped++;
            print(" [SKIPPED]");
        }
        print("\n");
    }
    
    // List and copy /sbin files
    static fat_entry_info sbin_entries[64];
    uint32_t sbin_count = fat_list_directory(&rootfs_volume, "/sbin", sbin_entries, 64);
    
    print_info("Processing /sbin directory...\n");
    
    for (uint32_t i = 0; i < sbin_count; i++) {
        if (sbin_entries[i].directory || sbin_entries[i].name[0] == '.') {
            continue;
        }
        
        char src_path[256];
        char dst_path[256];
        
        strcpy(src_path, "/sbin/");
        strcat(src_path, sbin_entries[i].name);
        
        strcpy(dst_path, "/sbin/");
        strcat(dst_path, sbin_entries[i].name);
        
        print("  Processing: ");
        print(sbin_entries[i].name);
        print("...");
        
        if (copy_file_to_clawfs(src_path, dst_path)) {
            copied++;
        } else {
            skipped++;
            print(" [SKIPPED]");
        }
        print("\n");
    }
    
    print_info("Format complete. Copied ");
    print_num8(copied);
    print(" files, skipped ");
    print_num8(skipped);
    print(" files.\n");
    
    Uart::puts("[File Resolver] Format complete.\n");
    
    return true;
}
