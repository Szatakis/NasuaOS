#pragma once
#include <stdint.h>
#include <stdbool.h>

// File source types
typedef enum {
    FILE_SOURCE_NONE = 0,
    FILE_SOURCE_CLAWFS,      // From ClawFS (ATA or RAM disk)
    FILE_SOURCE_ROOTFS       // From ISO rootfs.img (FAT)
} file_source_t;

// File resolution result
typedef struct {
    file_source_t source;
    bool exists;
    uint32_t size;           // File size in bytes
    uint32_t data_sector;    // For ClawFS files
} file_resolve_result_t;

// Mount state management
void file_resolver_init();
void file_resolver_mount(bool mount);
bool file_resolver_is_mounted();

// Centralized file resolution
// Returns the active source for a system file path (e.g., "/bin/ls", "/sbin/bootcheck")
// Priority: ClawFS (if mounted) > rootfs
file_resolve_result_t resolve_system_file(const char* path);

// Check if file exists in any source
bool system_file_exists(const char* path);

// Copy file from rootfs to ClawFS (for format commands)
bool copy_file_to_clawfs(const char* src_path, const char* dst_path);

// Format commands: copy all from rootfs /bin and /sbin to ClawFS
bool format_commands();

// Get current storage type (ATA or RAM disk)
bool storage_uses_ata();
