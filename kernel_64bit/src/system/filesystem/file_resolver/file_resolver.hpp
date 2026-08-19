#ifndef FILE_RESOLVER_HPP
#define FILE_RESOLVER_HPP

#include <cstdint>

// File source enumeration
enum file_source_t
{
    FILE_SOURCE_NONE = 0,
    FILE_SOURCE_CLAWFS = 1,
    FILE_SOURCE_ROOTFS = 2
};

// File resolution result
struct file_resolve_result_t
{
    file_source_t source;
    bool exists;
    uint32_t data_sector;
    uint32_t size;
};

// Deletion tombstone entry
struct deletion_tombstone_t
{
    char path[256];
    bool deleted;
};

// Initialize the file resolver
void file_resolver_init();

// Mount/unmount ClawFS overlay
void file_resolver_mount(bool mount);

// Check if ClawFS overlay is mounted
bool file_resolver_is_mounted();

// Resolve a system file path to its source and location
file_resolve_result_t resolve_system_file(const char* path);

// Check if a system file exists (uses resolver)
bool system_file_exists(const char* path);

// Check if storage uses ATA (vs RAM disk)
bool storage_uses_ata();

// Copy file from rootfs to ClawFS
bool copy_file_to_clawfs(const char* src_path, const char* dst_path);

// Format commands: copy all from rootfs /bin and /sbin to ClawFS
bool format_commands();

// Add file to deletion tombstone list (when deleted from ClawFS while mounted)
void file_resolver_mark_deleted(const char* path);

// Remove file from deletion tombstone list (when restored or unmounted)
void file_resolver_undelete(const char* path);

// Clear all deletion tombstones (when unmounting)
void file_resolver_clear_deletions();

// Check if file is marked as deleted
bool file_resolver_is_deleted(const char* path);

#endif // FILE_RESOLVER_HPP
