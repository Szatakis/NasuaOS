#!/usr/bin/env python3
"""Appends a NAPP relocation table to a flat .napp image.

The generic app.mk link emits R_X86_64_64 relocations for every absolute
pointer stored inside the image (callback structs, GOT entries, ...). Those
pointers are image-relative offsets at link time; the kernel loader rebases
them once the load address is known.

The table is appended to the end of the image as:

    uint32 count
    uint32 offsets[count]   # image offset of each 64-bit pointer slot

Its location is recorded in the napp_header reloc_offset/reloc_count fields,
which the kernel reads before jumping to the entry point.
"""
import struct
import sys
from pathlib import Path

ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
SECTION_HEADER = struct.Struct("<IIQQQQIIQQ")
RELA = struct.Struct("<QQq")
R_X86_64_64 = 1
R_X86_64_32 = 10
R_X86_64_32S = 11
NAPP_MAGIC = 0x5050414E
MAX_RELOCATIONS = 0x100000

# Byte offsets of napp_header::reloc_offset / napp_header::reloc_count inside
# the packed header produced by utilities/applications/include/napp.h. Keep
# in sync with that struct: they follow category[NAPP_CATEGORY_LENGTH].
HEADER_RELOC_OFFSET = 386
HEADER_RELOC_COUNT = 390
HEADER_MIN_SIZE = HEADER_RELOC_COUNT + 4


def read_c_string(data, offset):
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError("unterminated ELF string")
    return data[offset:end].decode("ascii")


def collect_offsets(elf, named_sections, image_size):
    offsets = []
    for section_name, section in named_sections.items():
        if not section_name.startswith(".rela."):
            continue
        offset, size, entry_size = section[4], section[5], section[9]
        if entry_size < RELA.size:
            raise ValueError(f"invalid relocation entry size in {section_name}")
        for position in range(offset, offset + size, entry_size):
            target, info, _ = RELA.unpack_from(elf, position)
            relocation_type = info & 0xFFFFFFFF
            if relocation_type in (R_X86_64_32, R_X86_64_32S):
                raise ValueError(
                    f"unsupported absolute 32-bit relocation in {section_name} "
                    f"at {target:#x}"
                )
            if relocation_type != R_X86_64_64:
                continue
            if target > 0xFFFFFFFF:
                raise ValueError(
                    f"relocation target outside NAPP address range: {target:#x}"
                )
            offsets.append(target)

    got_section = named_sections.get(".got")
    if got_section is not None:
        got_vma, got_file_offset, got_size = (
            got_section[3],
            got_section[4],
            got_section[5],
        )
        if got_size % 8 != 0:
            raise ValueError("ELF .got section is not a sequence of 64-bit entries")
        for index in range(got_size // 8):
            value = struct.unpack_from("<Q", elf, got_file_offset + index * 8)[0]
            if value == 0:
                continue
            if value > image_size:
                raise ValueError(f"GOT entry points outside NAPP image: {value:#x}")
            offsets.append(got_vma + index * 8)

    return sorted(set(offsets))


def pack_relocations(elf_path, image_path):
    elf = elf_path.read_bytes()
    header = ELF_HEADER.unpack_from(elf)
    section_offset = header[6]
    section_entry_size = header[11]
    section_count = header[12]
    string_table_index = header[13]

    sections = [
        SECTION_HEADER.unpack_from(elf, section_offset + index * section_entry_size)
        for index in range(section_count)
    ]
    string_table = sections[string_table_index]
    section_names = elf[
        string_table[4] : string_table[4] + string_table[5]
    ]
    named_sections = {
        read_c_string(section_names, section[0]): section
        for section in sections
    }

    image = bytearray(image_path.read_bytes())
    if len(image) < HEADER_MIN_SIZE:
        raise ValueError("NAPP image is too small to hold a header")
    magic, = struct.unpack_from("<I", image, 0)
    if magic != NAPP_MAGIC:
        raise ValueError("NAPP image is missing its header magic")
    header_size, = struct.unpack_from("<I", image, 8)
    if header_size < HEADER_MIN_SIZE:
        raise ValueError("NAPP header predates the relocation fields; rebuild it")

    offsets = collect_offsets(elf, named_sections, len(image))

    if offsets:
        # Keep the table 8-byte aligned for the kernel's uint64 accesses.
        padding = (-len(image)) % 8
        image.extend(b"\0" * padding)
        table_offset = len(image)
        image.extend(struct.pack("<I", len(offsets)))
        image.extend(struct.pack(f"<{len(offsets)}I", *offsets))
        struct.pack_into("<I", image, HEADER_RELOC_OFFSET, table_offset)
        struct.pack_into("<I", image, HEADER_RELOC_COUNT, len(offsets))

    image_path.write_bytes(image)
    print(f"Packed {len(offsets)} absolute relocations into {image_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: pack_relocations.py <app.elf> <app.napp>")
    try:
        pack_relocations(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, struct.error) as error:
        raise SystemExit(str(error)) from error
