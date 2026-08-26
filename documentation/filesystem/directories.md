# Filesystem - Directories

NasuaOS organizes files across two layers: the **ISO rootfs** (FAT filesystem, read-only) and **ClawFS** (read/write filesystem on the disk partition).

## ISO Rootfs Layout

The ISO rootfs is a FAT16 filesystem image (`rootfs.img`) built from `utilities/rootfs/dir/`. It contains the base system files that are always available.

```
/
├── .clawfs                  # Marker file (empty)
├── bin/
│   ├── .clawfs              # Marker file
│   ├── bootcheck             # NAPP binary
│   ├── calculator            # NAPP binary
│   ├── minesweeper           # NAPP binary
│   ├── snake                 # NAPP binary
│   └── suaedit               # NAPP binary
├── sbin/
│   ├── .clawfs              # Marker file
│   ├── cat                  # NAPP binary
│   ├── cd                   # NAPP binary
│   ├── cp                   # NAPP binary
│   ├── ls                   # NAPP binary
│   ├── mkdir                # NAPP binary
│   ├── mv                   # NAPP binary
│   ├── pwd                  # NAPP binary
│   ├── rm                   # NAPP binary
│   └── tree                 # NAPP binary
└── home/
    └── user/
```

### Directory Structure Details

**`/bin/`** — Contains NAPP applications as flat binary files (one per application):
```
/bin/<app_name>
```
Each file is a flat binary with an NAPP header. The `.clawfs` marker file in the
directory helps the build system track the structure.

**`/sbin/`** — Contains system commands as flat NAPP binaries:
```
/sbin/cat
/sbin/cd
/sbin/cp
/sbin/ls
/sbin/mkdir
/sbin/mv
/sbin/pwd
/sbin/rm
```

**`/home/`** — Default user home directory. The shell starts at `/home` (or `/home/user` after `cd` with no arguments).

## ClawFS Directory Structure

When ClawFS is formatted (via `format`), it creates this default structure:

| Path         | Type      | Purpose                               |
|--------------|-----------|---------------------------------------|
| `/`          | Root      | ClawFS root                           |
| `/bin`       | Directory | NAPP applications (when copied)       |
| `/sbin`      | Directory | System command binaries (when copied) |
| `/home`      | Directory | User home directories                 |
| `/home/user` | Directory | Default user home                     |
| `/mnt`       | Directory | Temporary mount points                |
| `/tmp`       | Directory | Temporary files                       |
| `/dev`       | Directory | Device representations                |

## Path Resolution

The shell uses a current working directory stored as a global string (`current_path`). Path resolution follows these rules:

| Input      | Resolved Path                                  |
|------------|------------------------------------------------|
| `/`        | Root directory                                 |
| `~`        | `/home`                                        |
| `..`       | Parent directory (removes last path component) |
| `.`        | Current directory (no change)                  |
| `foo`      | `<current_path>/foo` (relative)                |
| `/foo/bar` | `/foo/bar` (absolute)                          |

### Examples

Starting from `/home/user`:

```
user@nasua-os:/home/user> cd ..
user@nasua-os:/home> pwd
/home

user@nasua-os:/home> cd ~
user@nasua-os:/home> cd ..
user@nasua-os:/> ls
bin sbin home mnt tmp

user@nasua-os:/> cd /home/user
user@nasua-os:/home/user> pwd
/home/user
```

## Current Working Directory

The current path is maintained as a global variable in the kernel:

```cpp
char current_path[256] = "/home";  // Default starting directory
```

The `cd` command modifies this variable. All file operations (except `cat` which accepts full paths) work relative to this directory.

## Special Paths

| Path         | Description                                  |
|--------------|----------------------------------------------|
| `/`          | Root — used as base for absolute paths       |
| `~`          | Shortcut to `/home` user directory           |
| `/home/user` | Default user home after shell startup        |
| `/sbin`      | System command binaries (flat NAPP binaries) |
| `/bin`       | NAPP applications (flat binary files)        |
| `/tmp`       | Temporary files directory                    |
| `/mnt`       | Mount point directory                        |

## Directory Navigation Commands

### cd

The `cd` command (a `/sbin` command) modifies `current_path`:

```
cd <absolute_or_relative_path>
cd ~           # Go to /home
cd ..          # Go up one level
cd             # No argument — implicit cd ~ (go to /home)
```

### pwd

The `pwd` command (a `/sbin` command) prints `current_path`:

```
pwd
# Output: /home/user/desktop
```

### ls

Lists the contents of a directory:

```
ls             # List current directory
ls /home       # List /home directory
ls /sbin       # List available system commands
```

### mkdir

Creates a new directory:

```
mkdir --dir_name <folder_name>
```

The directory is created in the current working directory (on ClawFS when mounted).

## Root Filesystem (.clawfs markers)

Each directory on the ISO rootfs contains a hidden `.clawfs` marker file. These are empty files that serve as:

1. **Directory existence markers** — The build system uses them to verify directory structure
2. **ClawFS sync indicators** — When copying commands to ClawFS via `format --commands`, these markers help identify directories

These files are not visible to the user in normal `ls` output and are ignored by the shell's path resolution.
