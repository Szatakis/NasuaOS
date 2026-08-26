# Filesystem - ClawFS

ClawFS is NasuaOS's native filesystem. It is a flat, sector-based filesystem designed for simplicity and reliability on raw block devices.

## Overview

| Property           | Value                                   |
|--------------------|-----------------------------------------|
| Start LBA          | 100                                     |
| Root sector        | 101                                     |
| Directory sectors  | 102–109                                 |
| Tombstone area     | Sector 1024 (entries up to sector 1054) |
| Entries per sector | 12                                      |
| Sector size        | 512 bytes                               |
| Max filename       | 28 characters                           |

> **Note**: Some of the above constants are based on the 32-bit ClawFS implementation. The 64-bit version (`kernel_64bit/src/system/filesystem/clawfs/`) may differ slightly. Where specific values are not confirmed from the 64-bit source, they are marked as approximate.

## On-Disk Layout

```
 LBA 0     : Boot sector (reserved)
 LBA 1–99  : Reserved for bootloader and partition table
 LBA 100   : ClawFS header ("CLAWFS" magic)
 LBA 101   : Root directory sector
 LBA 102–109 : Directory table sectors (8 sectors for directory metadata)
 LBA 110   : First data sector (file contents)
 ...
 LBA 1024+ : Tombstone table (deleted file markers)
```

## Entry Structure

Each directory entry is 24 bytes:

| Offset | Size | Field                                                             |
|--------|------|-------------------------------------------------------------------|
| 0x00   | 4    | Magic value (0x53464157 = "WAFS" little-endian? — **unverified**) |
| 0x04   | 28   | Name (null-terminated string)                                     |
| 0x20   | 4    | Starting LBA                                                      |
| 0x24   | 4    | Size (for files) or entry count (for directories)                 |
| 0x28   | 4    | Type (CLAWFS_FILE=0, CLAWFS_DIRECTORY=1)                          |

With 12 entries per 512-byte sector, 12 × 24 = 288 bytes are needed, leaving 224 bytes unused per sector (or entries may be tightly packed — **unverified**).

## Magic Values

| Type                       | Value                                 |
|----------------------------|---------------------------------------|
| File                       | `CLAWFS_FILE = 0`                     |
| Directory                  | `CLAWFS_DIRECTORY = 1`                |
| Deleted (tombstone marker) | `CLAWFS_DELETED = 0xFF` (approximate) |

## Core Operations

### Initialization

```cpp
void clawfs_init(uint8_t* drive_buffer);
```

Initializes the ClawFS driver with a pointer to the drive's data buffer.

### Formatting

```cpp
clawfs_format();
clawfs_format_clr();
```

| Function              | Description                                                                           |
|-----------------------|---------------------------------------------------------------------------------------|
| `clawfs_format()`     | Create a new ClawFS filesystem — writes header, root directory, and directory sectors |
| `clawfs_format_clr()` | Completely wipe ClawFS — clears header and all sectors                                |

The `format` shell command calls `clawfs_format()`:

```
user@nasua-os:/home> format
Filesystem formatted successfully.
```

### Directory Operations

```cpp
int clawfs_mkdir(const char* path);
int clawfs_dir(const char* path);
void list_dir(const char* path);
```

- `clawfs_mkdir(path)` — Create a directory at `path`
- `clawfs_dir(path)` — List directory contents (prints file listing)
- `list_dir(path)` — Internal directory listing helper

### File Operations

```cpp
int clawfs_create_file(const char* name);
int clawfs_create_file_in(const char* dir_path, const char* name);
int clawfs_rm(const char* path);
int clawfs_rm_dir(const char* path);
```

- `clawfs_create_file(name)` — Create an empty file in the current directory
- `clawfs_create_file_in(dir, name)` — Create a file in a specific directory
- `clawfs_rm(path)` — Remove a file
- `clawfs_rm_dir(path)` — Remove an empty directory

### File Writing

```cpp
int clawfs_write_to_file(const char* path, const char* content);
```

Writes content to an existing file, overwriting its previous contents.

### Path Resolution

```cpp
int get_sector_by_path(const char* path, int* out_type);
```

Resolves a full path (e.g., `/home/user/file.txt`) to its sector number and type. Supports:
- Absolute paths (starting with `/`)
- Relative paths (resolved against the current working directory)
- `..` to traverse parent directories
- `.` for the current directory

## Default Directory Structure

After formatting, ClawFS creates these root-level directories:

| Directory    | Purpose                 |
|--------------|-------------------------|
| `/bin`       | NAPP applications       |
| `/sbin`      | System command binaries |
| `/home`      | User home directories   |
| `/home/user` | Default user home       |
| `/mnt`       | Mount points            |
| `/tmp`       | Temporary files         |

## .clawfs Marker

Each directory on the ISO rootfs contains a hidden `.clawfs` marker file (empty). These are used by the build system to track directory structure when creating the rootfs image.

## File Size Limits

- Maximum file size: ~65,535 bytes (4 data sectors ≈ 2KB, or more with multi-cluster allocation — **unverified**)
- Maximum filename length: 28 characters
- Maximum path depth: 32 levels (approximate)

## Limitations

- No file permissions model
- No hard or soft links
- No symbolic links
- Files are allocated contiguously (no fragmentation handling)
- No journaling or crash recovery
