#!/usr/bin/env python3
import argparse
import os
import struct
from pathlib import Path


SECTOR_SIZE = 512
CLAWFS_START_LBA = 100
CLAWFS_ROOT_SECTOR = 101
CLAWFS_FILE = 0
CLAWFS_DIRECTORY = 1
CLAWFS_MAX_FILE_SIZE = 32 * 1024 * 1024
DIRECTORY_ENTRY_COUNT = 12
ENTRY_STRUCT = struct.Struct("<28sIII")


def read_sector(image, sector):
    image.seek(sector * SECTOR_SIZE)
    data = image.read(SECTOR_SIZE)
    if len(data) != SECTOR_SIZE:
        raise ValueError(f"ClawFS sector {sector} is outside the disk image")
    return data


def directory_entries(sector_data):
    return [
        ENTRY_STRUCT.unpack_from(sector_data, index * ENTRY_STRUCT.size)
        for index in range(DIRECTORY_ENTRY_COUNT)
    ]


def find_named_entry(entries, name):
    encoded_name = name.encode("ascii")
    for index, entry in enumerate(entries):
        raw_name, entry_type, data_sector, entry_count = entry
        if raw_name.split(b"\0", 1)[0] == encoded_name:
            return index, entry_type, data_sector, entry_count
    return None


def highest_used_sector(image, image_sectors):
    highest = 112
    visited_directories = set()

    def visit(directory_sector, depth):
        nonlocal highest
        if depth >= 32:
            raise ValueError("ClawFS directory tree exceeds the supported depth")
        if directory_sector in visited_directories:
            return
        if directory_sector <= 0 or directory_sector >= image_sectors:
            raise ValueError(f"Invalid ClawFS directory sector {directory_sector}")
        visited_directories.add(directory_sector)

        for raw_name, entry_type, data_sector, entry_count in directory_entries(
            read_sector(image, directory_sector)
        ):
            if raw_name[0] == 0:
                continue
            if data_sector == 0:
                raise ValueError("ClawFS entry has no data sector")

            if entry_type == CLAWFS_DIRECTORY:
                sector_count = 1
            elif entry_type == CLAWFS_FILE:
                if entry_count > CLAWFS_MAX_FILE_SIZE:
                    raise ValueError("ClawFS file exceeds its 32 MiB size limit")
                sector_count = max(1, (entry_count + SECTOR_SIZE - 1) // SECTOR_SIZE)
            else:
                raise ValueError(f"Unsupported ClawFS entry type {entry_type}")

            sector_end = data_sector + sector_count
            if sector_end > image_sectors:
                raise ValueError("ClawFS entry extends beyond the disk image")
            highest = max(highest, sector_end)

            if entry_type == CLAWFS_DIRECTORY:
                visit(data_sector, depth + 1)

    visit(CLAWFS_ROOT_SECTOR, 0)
    return highest


def format_empty_clawfs(image_path, image_sectors):
    with image_path.open("rb") as image:
        while chunk := image.read(1024 * 1024):
            if any(chunk):
                raise ValueError(
                    "--format-empty only works on a completely zeroed disk image"
                )

    if image_sectors <= 135:
        raise ValueError("Disk image is too small for the initial ClawFS directories")

    def make_entry(name, entry_type, sector):
        return ENTRY_STRUCT.pack(name.encode("ascii"), entry_type, sector, 0)

    directories = [
        (101, [(name, CLAWFS_DIRECTORY, sector) for name, sector in (
            ("bin", 102), ("sbin", 103), ("dev", 104), ("mnt", 105),
            ("home", 106), ("etc", 107), ("var", 108), ("lib", 109),
            ("proc", 110), ("tmp", 111),
        )]),
        (102, [("kernel_bin", CLAWFS_FILE, 131)]),
        (106, [("user", CLAWFS_DIRECTORY, 112), ("root", CLAWFS_DIRECTORY, 113)]),
        (112, [(name, CLAWFS_DIRECTORY, sector) for name, sector in (
            ("desktop", 114), ("documents", 115), ("images", 116),
            ("videos", 117), ("downloads", 118), ("uploads", 119),
            ("music", 120), ("projects", 121),
        )]),
        (108, [("log", CLAWFS_DIRECTORY, 122), ("cache", CLAWFS_DIRECTORY, 123)]),
        (109, [("drivers", CLAWFS_DIRECTORY, 124),
               ("modules", CLAWFS_DIRECTORY, 125),
               ("runtime", CLAWFS_DIRECTORY, 126)]),
        (110, [(name, CLAWFS_FILE, sector) for name, sector in (
            ("cpuinfo", 127), ("meminfo", 128), ("uptime", 129),
            ("k_version", 130),
        )]),
    ]

    with image_path.open("r+b") as image:
        header = bytearray(SECTOR_SIZE)
        header[:9] = b"CLAWFS\x01\x00\x00"
        image.seek(CLAWFS_START_LBA * SECTOR_SIZE)
        image.write(header)

        for sector in range(CLAWFS_ROOT_SECTOR, 132):
            image.seek(sector * SECTOR_SIZE)
            image.write(bytes(SECTOR_SIZE))

        for directory_sector, entries in directories:
            data = bytearray(SECTOR_SIZE)
            for index, (name, entry_type, child_sector) in enumerate(entries):
                start = index * ENTRY_STRUCT.size
                data[start : start + ENTRY_STRUCT.size] = make_entry(
                    name, entry_type, child_sector
                )
            image.seek(directory_sector * SECTOR_SIZE)
            image.write(data)

        image.flush()
        os.fsync(image.fileno())


def write_iwad(image_path, wad_path, replace):
    image_sectors = image_path.stat().st_size // SECTOR_SIZE
    if image_path.stat().st_size % SECTOR_SIZE != 0:
        raise ValueError("ClawFS disk image size must be a multiple of 512 bytes")
    if image_sectors <= CLAWFS_ROOT_SECTOR:
        raise ValueError("Disk image is too small to contain ClawFS")

    wad_size = wad_path.stat().st_size
    if wad_size == 0 or wad_size > CLAWFS_MAX_FILE_SIZE:
        raise ValueError("Freedoom IWAD must be between 1 byte and 32 MiB")
    with wad_path.open("rb") as wad:
        wad_header = wad.read(12)
        if len(wad_header) != 12 or wad_header[:4] != b"IWAD":
            raise ValueError("The supplied file does not have an IWAD header")
        lump_count, directory_offset = struct.unpack_from("<II", wad_header, 4)
        if directory_offset + lump_count * 16 > wad_size:
            raise ValueError("The supplied IWAD has an invalid lump directory")

    sector_count = (wad_size + SECTOR_SIZE - 1) // SECTOR_SIZE

    with image_path.open("r+b") as image:
        header = read_sector(image, CLAWFS_START_LBA)
        if header[:6] != b"CLAWFS" or header[6] != 1:
            raise ValueError("The disk image does not contain supported ClawFS")

        root_entries = directory_entries(read_sector(image, CLAWFS_ROOT_SECTOR))
        home = find_named_entry(root_entries, "home")
        if home is None or home[1] != CLAWFS_DIRECTORY:
            raise ValueError("ClawFS root has no /home directory")
        home_sector = home[2]
        home_data = bytearray(read_sector(image, home_sector))
        home_entries = directory_entries(home_data)
        existing = find_named_entry(home_entries, "freedoom2.wad")

        if existing is not None:
            if existing[1] != CLAWFS_FILE:
                raise ValueError("/home/freedoom2.wad exists and is not a file")
            if not replace:
                raise FileExistsError(
                    "/home/freedoom2.wad already exists; use --replace to overwrite it"
                )

        highest = highest_used_sector(image, image_sectors)
        if existing is not None:
            existing_sector_count = max(
                1, (existing[3] + SECTOR_SIZE - 1) // SECTOR_SIZE
            )
            target_sector = (
                existing[2] if sector_count <= existing_sector_count else highest
            )
        else:
            free_slot = next(
                (index for index, entry in enumerate(home_entries) if entry[0][0] == 0),
                None,
            )
            if free_slot is None:
                raise ValueError("/home has no free ClawFS directory entry")
            target_sector = highest
            existing = (free_slot, CLAWFS_FILE, target_sector, 0)

        if target_sector + sector_count > image_sectors:
            raise ValueError("The ClawFS disk image has insufficient free space")

        image.seek(target_sector * SECTOR_SIZE)
        with wad_path.open("rb") as wad:
            remaining = wad_size
            while remaining:
                chunk = wad.read(min(1024 * 1024, remaining))
                if not chunk:
                    raise OSError("Freedoom IWAD changed while it was being copied")
                image.write(chunk)
                remaining -= len(chunk)
        padding = sector_count * SECTOR_SIZE - wad_size
        if padding:
            image.write(bytes(padding))
        image.flush()
        os.fsync(image.fileno())

        entry_index = existing[0]
        encoded_name = b"freedoom2.wad"
        entry_bytes = ENTRY_STRUCT.pack(
            encoded_name, CLAWFS_FILE, target_sector, wad_size
        )
        start = entry_index * ENTRY_STRUCT.size
        home_data[start : start + ENTRY_STRUCT.size] = entry_bytes
        image.seek(home_sector * SECTOR_SIZE)
        image.write(home_data)
        image.flush()
        os.fsync(image.fileno())

    print(f"Installed {wad_path} as /home/freedoom2.wad ({wad_size} bytes)")


def main():
    parser = argparse.ArgumentParser(
        description="Install Freedoom 2 IWAD data into a ClawFS disk image."
    )
    parser.add_argument("disk_image", type=Path, help="raw NasuaOS ClawFS disk image")
    parser.add_argument(
        "--wad",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "data" / "freedoom2.wad",
        help="IWAD file to install (defaults to the local data/freedoom2.wad)",
    )
    parser.add_argument(
        "--replace",
        action="store_true",
        help="replace an existing /home/freedoom2.wad",
    )
    parser.add_argument(
        "--format-empty",
        action="store_true",
        help="initialize ClawFS only if the disk image is entirely zeroed",
    )
    args = parser.parse_args()
    try:
        if not args.wad.is_file():
            parser.error(
                f"{args.wad} is missing; download Freedoom 0.13.0 and extract "
                "freedoom2.wad to utilities/applications/doom/data/freedoom2.wad"
            )
        if args.format_empty:
            wad_size = args.wad.stat().st_size
            with args.wad.open("rb") as wad:
                wad_header = wad.read(12)
            if (
                wad_size == 0
                or wad_size > CLAWFS_MAX_FILE_SIZE
                or len(wad_header) != 12
                or wad_header[:4] != b"IWAD"
            ):
                parser.error("The supplied Freedoom IWAD is invalid or too large")
            lump_count, directory_offset = struct.unpack_from("<II", wad_header, 4)
            if directory_offset + lump_count * 16 > wad_size:
                parser.error("The supplied Freedoom IWAD has an invalid lump directory")
            image_sectors = args.disk_image.stat().st_size // SECTOR_SIZE
            required_sectors = 132 + (wad_size + SECTOR_SIZE - 1) // SECTOR_SIZE
            if image_sectors < required_sectors:
                parser.error("Disk image is too small for ClawFS and the IWAD")
            format_empty_clawfs(args.disk_image, image_sectors)
        write_iwad(args.disk_image, args.wad, args.replace)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
