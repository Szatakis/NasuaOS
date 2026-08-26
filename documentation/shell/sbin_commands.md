# Shell /sbin Commands

The `/sbin` commands are **flat-binary NAPP applications** that are dynamically loaded from the root filesystem. Unlike built-in shell commands, these are not compiled into the kernel — they are loaded at runtime.

## Loading Mechanism

When the shell receives a command it does not recognize as a built-in, it attempts to load it from `/sbin/`:

1. The shell constructs the path `/sbin/<command_name>`
2. It checks if the file exists using `system_file_exists()` (the file resolver)
3. If found, the binary is loaded via `napp_run_path()` and executed
4. Arguments are passed as `argv[]` with `argv[0]` set to the command name

The file resolver determines the source:

| Overlay State            | Source                              | Fallback         |
|--------------------------|-------------------------------------|------------------|
| ClawFS mounted (`mount`) | ClawFS (`/sbin/` on disk partition) | ISO rootfs (FAT) |
| ClawFS unmounted         | ISO rootfs (FAT)                    | None             |

## Available /sbin Commands

| Command | Description              | Flags                        |
|---------|--------------------------|------------------------------|
| `cat`   | Display file contents    | `--file`, `--file_n`         |
| `cd`    | Change directory         | `[path]` (no flags)          |
| `cp`    | Copy file                | `--source`, `--destination`  |
| `ls`    | List directory           | `[path]` (no flags)          |
| `mkdir` | Create directory         | `--dir_name`                 |
| `mv`    | Move/rename file         | `--source`, `--destination`  |
| `pwd`   | Print working directory  | No flags                     |
| `rm`    | Remove file or directory | `--name`, `--type`           |
| `tree`  | Display directory tree   | `[path]` (no flags)          |

## Help System

The `help` command documents `/sbin` commands starting from **page 11**. The number of `/sbin` pages depends on:

- How many commands exist in `/sbin` (up to 13 commands per page)
- Whether ClawFS is mounted (affects which `/sbin` directory is queried)

To view `/sbin` commands:

```
user@nasua-pc:/home> help --page 11
System commands from /sbin (Page 11/<total>):
 -cat
 -cd
 -cp
 -ls
 -mkdir
 -mv
 -pwd
 -rm
 -tree
```

If no commands are found in `/sbin` (e.g., rootfs not mounted or ClawFS not formatted), the help system displays:

```
 (no commands found in /sbin — rootfs may not be mounted)
```

## Command Details

### cat

Display the contents of one or more files.

```
cat --file <filename> [--file_n <filename>] ...
```

| Flag       | Required              | Description                 |
|------------|-----------------------|-----------------------------|
| `--file`   | Yes (first file)      | First file to display       |
| `--file_n` | No (subsequent files) | Additional files to display |

File paths are resolved relative to the current working directory. Both absolute and relative paths are supported.

```
user@nasua-pc:/home> cd /home/user/desktop
user@nasua-pc:/home/user/desktop> cat --file notes.txt
Contents of notes.txt...
```

### cd

Change the current working directory.

```
cd [directory_path]
cd ~          # Return to /home
cd ..         # Go up one level
```

- `cd` with no arguments changes to `/home`
- `cd ~` also changes to `/home`
- `cd ..` removes the last path component
- Paths starting with `/` are treated as absolute
- Other paths are resolved relative to the current directory

### cp

Copy a file from source to destination.

```
cp --source <source> --destination <destination>
```

If the destination is an existing directory, the file is copied into that directory with its original name.

### ls

List files and directories in a path.

```
ls [path]
ls /home/user
```

With no arguments, lists the current working directory. Paths are resolved via ClawFS.

### mkdir

Create a new directory in the current path.

```
mkdir --dir_name <folder_name>
```

### mv

Move or rename a file.

```
mv --source <source> --destination <destination>
```

Like `cp`, if the destination is a directory the file is moved into it. The source is then deleted.

### pwd

Print the current working directory.

```
pwd
```

### rm

Remove a file or directory.

```
rm --name <name> --type <file|dir>
```

| Flag     | Required | Description                                 |
|----------|----------|---------------------------------------------|
| `--name` | Yes      | Name of the item (relative to current path) |
| `--type` | Yes      | `file` or `dir`                             |

Directories must be empty before they can be removed. If `--type` is not specified, the command attempts to auto-detect the entry type.

### tree

Display a directory tree structure recursively.

```
tree [directory_path]
tree
tree /home/user
```

With no arguments, displays the tree starting from the current working directory. If a path is given, it is resolved relative to the current directory (or treated as absolute if it starts with `/`).

Hidden entries (names starting with `.`) are not displayed. Only directories are traversed recursively; files are listed but not entered. Each entry is prefixed with `<DIR>` for directories or `<FILE>` for files, matching the `ls` output format.

## Copy-on-Write and Persistence

When ClawFS is mounted:

1. Commands are loaded from ClawFS (if present there)
2. If a command is not in ClawFS, the system falls back to the ISO rootfs
3. Files deleted from ClawFS are tracked as **tombstones** — they are hidden even if present in the ISO rootfs
4. The `format --commands` command copies all `/bin` and `/sbin` from ISO to ClawFS

See [Filesystem - Mounting](../filesystem/mounting.md) and [Filesystem - Mounting Details](../filesystem/mounting.md) for more information on the overlay behavior.
