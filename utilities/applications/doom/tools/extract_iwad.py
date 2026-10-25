#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path, PurePosixPath
from zipfile import BadZipFile, ZipFile


IWAD_NAME = "freedoom2.wad"
IWAD_SHA256 = "a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b"
MAX_IWAD_SIZE = 32 * 1024 * 1024
OUTPUT_PATH = Path(__file__).resolve().parents[1] / "data" / IWAD_NAME


def main():
    parser = argparse.ArgumentParser(
        description="Extract and verify the Freedoom 2 IWAD from its release archive."
    )
    parser.add_argument("archive", type=Path, help="Freedoom release ZIP archive")
    args = parser.parse_args()

    try:
        with ZipFile(args.archive) as archive:
            entries = [
                entry
                for entry in archive.infolist()
                if PurePosixPath(entry.filename).name == IWAD_NAME
                and not entry.is_dir()
            ]
            if len(entries) != 1:
                raise ValueError(f"Expected exactly one {IWAD_NAME} in the archive")
            entry = entries[0]
            if entry.file_size == 0 or entry.file_size > MAX_IWAD_SIZE:
                raise ValueError(f"{IWAD_NAME} has an invalid size")
            wad = archive.read(entry)
    except (OSError, BadZipFile, ValueError) as error:
        parser.error(str(error))

    if wad[:4] != b"IWAD":
        parser.error(f"{IWAD_NAME} does not have a valid IWAD header")
    if hashlib.sha256(wad).hexdigest() != IWAD_SHA256:
        parser.error(f"{IWAD_NAME} does not match the expected SHA-256 checksum")

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_bytes(wad)
    print(f"Installed verified {IWAD_NAME} ({len(wad)} bytes)")


if __name__ == "__main__":
    main()
