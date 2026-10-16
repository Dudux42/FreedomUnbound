"""Inventory declared ELF overlays and scan a local ISO for raw MWo3 images.

Writes metadata only; no game images are extracted. A raw signature scan does
not establish coverage of compressed or encrypted archive entries.
"""

import argparse
import collections
import json
import mmap
import re
import struct
from pathlib import Path


SLOTS = {
    0x09A5F300: ("task", 0x09C19100),
    0x09C19100: ("subtask", 0x09D1A280),
    0x09D1A280: ("monster", 0x09D63100),
    0x09D63100: ("stage", 0x09D65500),
    0x09D65500: ("debug", 0x09D65580),
}


def fnv1a64(data):
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    executable = args.elf.read_bytes()
    if executable[:7] != b"\x7fELF\x01\x01\x01":
        raise SystemExit("Expected a little-endian ELF32 executable")
    shoff = struct.unpack_from("<I", executable, 32)[0]
    entry_size, section_count, string_index = struct.unpack_from("<3H", executable, 46)
    sections = [struct.unpack_from("<10I", executable, shoff + i * entry_size)
                for i in range(section_count)]
    strings_header = sections[string_index]
    strings = executable[strings_header[4]:strings_header[4] + strings_header[5]]
    libraries = list(args.libraries.glob("*.dll"))
    declared = []
    for section in sections:
        name = strings[section[0]:].split(b"\0", 1)[0].decode("ascii", "replace")
        base = section[3]
        if base not in SLOTS or not re.fullmatch(r"[A-Za-z0-9_]+\.(?:ovl|elf)", name):
            continue
        prefix = f"ovl{base:08X}_{name[:-4]}_"
        matches = sorted(p.name for p in libraries if p.name.startswith(prefix))
        declared.append(dict(name=name, base=f"0x{base:08X}", category=SLOTS[base][0],
                             compiled_by_name=bool(matches), libraries=matches))
    rows, rejected, signatures = [], [], 0
    with args.iso.open("rb") as source, mmap.mmap(
        source.fileno(), 0, access=mmap.ACCESS_READ
    ) as image:
        offset = 0
        while True:
            offset = image.find(b"MWo3", offset)
            if offset < 0:
                break
            start, offset = offset, offset + 4
            signatures += 1
            header = image[start:start + 64]
            if len(header) != 64:
                rejected.append(start)
                continue
            ident, base, code, data, bss = struct.unpack_from("<5I", header, 4)
            name_bytes = header[32:64].split(b"\0", 1)[0]
            size = 64 + code + data
            slot = SLOTS.get(base)
            if (not slot or not re.fullmatch(rb"[A-Za-z0-9_]+\.ovl", name_bytes)
                    or size < 64 or start + size > len(image)
                    or base + size + bss > slot[1]):
                rejected.append(start)
                continue
            name = name_bytes.decode("ascii")
            fingerprint = fnv1a64(image[start:start + 64 + code])
            library = f"ovl{base:08X}_{name[:-4]}_{fingerprint:016X}.dll"
            rows.append(dict(name=name, category=slot[0], id=ident,
                             base=f"0x{base:08X}", iso_offset=start,
                             code_bytes=code, data_bytes=data, bss_bytes=bss,
                             image_bytes=size, library=library,
                             compiled=(args.libraries / library).is_file()))
    counts = collections.Counter(row["category"] for row in rows)
    missing = collections.Counter(row["category"] for row in rows if not row["compiled"])
    unique = {row["library"] for row in rows}
    report = dict(scan="ELF named overlay sections plus raw ISO MWo3 headers",
                  limitation="ELF names declare overlays; availability is matched by name/base, not image hash. Compressed/encrypted entries and runtime execution are not verified.",
                  declared_count=len(declared),
                  declared_compiled_count=sum(row["compiled_by_name"] for row in declared),
                  declared_categories=dict(collections.Counter(row["category"] for row in declared)),
                  declared_missing_categories=dict(collections.Counter(
                      row["category"] for row in declared if not row["compiled_by_name"])),
                  iso_bytes=args.iso.stat().st_size, signatures=signatures,
                  rejected_offsets=rejected, image_count=len(rows),
                  unique_library_count=len(unique),
                  compiled_count=sum(row["compiled"] for row in rows),
                  categories=dict(counts), missing_categories=dict(missing),
                  libraries_not_found_on_disc=sorted(
                      p.name for p in args.libraries.glob("*.dll") if p.name not in unique),
                  declared=declared, images=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items()
                      if k not in ("images", "declared", "libraries_not_found_on_disc")}, indent=2))


if __name__ == "__main__":
    main()
