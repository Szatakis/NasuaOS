# Filesystem - Mounting

NasuaOS uses a **file resolver overlay** system that determines which filesystem commands and applications are loaded from. This is controlled by the `mount` and `unmount` commands.

## File Resolver Overview

The file resolver (`kernel_64bit/src/system/filesystem/file_resolver/`) sits between the NAPP loader and the underlying storage drivers. It decides where to look for `/bin` and `/sbin` files.

```
        Shell / NAPP Loader
                ↓
        File Resolver
        (resolve_system_file)
                ↓
    ┌───────────┴────────────┐
    │                        │
  Mounted?              Not Mounted?
    ↓                        ↓
  ClawFS overlay         ISO rootfs (FAT)
  (partition)            (rootfs.img)
    ↓                        ↓
  Disk driver              FAT driver
```

## Overlay Logic

### When Mounted (`mount` was executed)

```
File resolution order:
  1. Check ClawFS for /sbin/<command>
     → If found, load from ClawFS
  2. If not found in ClawFS, check ISO rootfs FAT
     → If found, load from ISO
```

### When Unmounted (default after boot)

```
File resolution order:
  1. Check ISO rootfs FAT for /sbin/<command>
     → If found, load from ISO
  2. If not found in ISO, check ClawFS
     → If found, load from ClawFS
```

### Tombstones

When ClawFS is mounted and a file is deleted from ClawFS, a **tombstone** entry is written at sector 1024. Tombstones ensure that even if a file exists in the ISO rootfs fallback, it remains hidden when ClawFS is mounted.

The tombstone table supports up to 60 entries (sectors 1024–1054).

```cpp
struct tombstone_entry
{
    char name[32];       // File or directory name
    uint32_t tombstone_id;// Sequential ID
};
```

## Mount Command

```
mount
```

Activates the ClawFS overlay. After running this command:

- `/sbin` commands load from ClawFS (if present there), falling back to ISO
- `/bin` NAPP applications load from ClawFS (if present there), falling back to ISO
- File creation/modification operations target ClawFS

### After Mounting

```
user@nasua-os:/home> mount
ClawFS overlay mounted.
Commands will load from ClawFS (/dev/sda1).
```

## Unmount Command

```
unmount
```

Deactivates the ClawFS overlay. After running this command:

- All commands and applications load from the ISO rootfs (FAT) only
- Write operations to ClawFS are blocked (files may show as not found)

### After Unmounting

```
user@nasua-os:/home> unmount
ClawFS overlay unmounted.
Commands will load from ISO rootfs.
```

## Format with Commands

The `format --commands` command copies all `/bin` and `/sbin` from the ISO rootfs (FAT) into ClawFS, enabling them to persist across reboots.

```
user@nasua-os:/home> format --commands
Setting up persistent command storage...
Commands copied successfully to ClawFS.
Run 'mount' to enable the overlay.
```

This is typically run once, followed by `mount`:

```
user@nasua-os:/home> format
Filesystem formatted successfully.
user@nasua-os:/home> format --commands
Commands copied successfully to ClawFS.
user@nasua-os:/home> mount
ClawFS overlay mounted.
```

## Checking Overlay Status

Use the `info` command to check the current overlay state:

```
user@nasua-os:/home> info
ClawFS status
  Overlay:         Mounted (commands from ClawFS)
```

or:

```
user@nasua-os:/home> info
ClawFS status
  Overlay:         Unmounted (commands from ISO)
```

## Boot Behavior

By default, after a clean boot:
1. The ISO rootfs (FAT) is loaded as a Limine boot module
2. ClawFS is **not** automatically mounted unless `AutoMount=true` in `defaults.txt`
3. Commands load from the ISO rootfs (`/sbin` on the FAT image)

If ClawFS was previously formatted and mounted, the `file_resolver` state is persisted as part of the kernel's global state — but since it's a new kernel load each boot, the overlay starts as **unmounted** by default (or mounted if `AutoMount=true`).

## File Resolution Function

```cpp
bool resolve_system_file(const char* path, char* out_path, bool use_clawfs);
```

This internal function resolves a system file path. When `use_clawfs` is true, it routes the query to the ClawFS filesystem driver; otherwise it falls back to the FAT ISO rootfs driver.

## Storage Detection

The file resolver determines storage type via:

```cpp
bool storage_uses_ata();
```

- If ATA is available, ClawFS is read from `/dev/sda` (the ATA disk)
- If no ATA disk is present, ClawFS cannot be mounted — commands always load from ISO rootfs

## Recovery

If the file resolver is in an inconsistent state (e.g., ClawFS is mounted but corrupted):

1. Run `unmount` to disable the overlay
2. Run `format --clear` to wipe ClawFS
3. Run `format` to create a fresh ClawFS
4. Run `format --commands` to copy commands
5. Run `mount` to re-enable the overlay
