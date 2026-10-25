#!/usr/bin/env python3
import struct
import sys
from pathlib import Path


ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
SECTION_HEADER = struct.Struct("<IIQQQQIIQQ")
SYMBOL = struct.Struct("<IBBHQQ")
RELA = struct.Struct("<QQq")
R_X86_64_64 = 1
R_X86_64_32 = 10
R_X86_64_32S = 11
RELOCATION_CAPACITY = 4096


def read_c_string(data, offset):
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError("unterminated ELF string")
    return data[offset:end].decode("ascii")


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

    relocation_section = named_sections.get(".napp_relocs")
    if relocation_section is None:
        raise ValueError("ELF is missing its reserved .napp_relocs section")
    got_section = named_sections.get(".got")
    if got_section is None:
        raise ValueError("ELF is missing its .got section")
    relocation_start = relocation_section[3]
    relocation_capacity = (relocation_section[5] - 4) // 4
    if relocation_capacity < RELOCATION_CAPACITY:
        raise ValueError("ELF relocation section is smaller than its configured capacity")

    image = bytearray(image_path.read_bytes())
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
                raise ValueError(f"relocation target outside NAPP address range: {target:#x}")
            offsets.append(target)

    got_start, got_file_offset, got_size = (
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
        if value > len(image):
            raise ValueError(f"GOT entry points outside NAPP image: {value:#x}")
        offsets.append(got_start + index * 8)

    if len(offsets) > relocation_capacity:
        raise ValueError(
            f"{len(offsets)} relocations exceed table capacity {relocation_capacity}"
        )

    if relocation_start + relocation_section[5] > len(image):
        raise ValueError("relocation table is outside the flat NAPP image")
    table = struct.pack("<I", len(offsets))
    table += struct.pack(f"<{len(offsets)}I", *offsets) if offsets else b""
    start = relocation_start
    image[start : start + len(table)] = table
    image_path.write_bytes(image)
    print(f"Packed {len(offsets)} absolute relocations into {image_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: pack_relocations.py <doom.elf> <doom.napp>")
    try:
        pack_relocations(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, struct.error) as error:
        raise SystemExit(str(error)) from error
