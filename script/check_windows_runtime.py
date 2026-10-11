#!/usr/bin/env python3
"""Stage matching VC runtime DLLs and reject invalid native packages."""
import argparse
from pathlib import Path
import shutil
import struct


def inspect(data):
    if data[:2] != b"MZ":
        raise ValueError("Not a PE image")
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine, count = struct.unpack_from("<HH", data, pe + 4)
    size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic not in (0x10b, 0x20b):
        raise ValueError("Unsupported PE optional header")
    directories = opt + (112 if magic == 0x20b else 96)
    image_base = struct.unpack_from("<Q" if magic == 0x20b else "<I", data, opt + (24 if magic == 0x20b else 28))[0]
    sections = []
    for index in range(count):
        virtual_size, address, raw_size, raw = struct.unpack_from("<IIII", data, opt + size + index * 40 + 8)
        sections.append((address, max(virtual_size, raw_size), raw))

    def offset(rva):
        for address, length, raw in sections:
            if address <= rva < address + length:
                return raw + rva - address
        raise ValueError("PE RVA outside sections")

    def name(rva):
        start = offset(rva)
        return data[start:data.index(0, start)].decode("ascii").lower()

    imports = []
    for directory, width, name_position in [(1, 20, 12), (13, 32, 4)]:
        rva = struct.unpack_from("<I", data, directories + directory * 8)[0]
        if not rva:
            continue
        cursor = offset(rva)
        while any(data[cursor:cursor + width]):
            name_rva = struct.unpack_from("<I", data, cursor + name_position)[0]
            if directory == 13 and not struct.unpack_from("<I", data, cursor)[0] & 1:
                name_rva -= image_base
            imports.append(name(name_rva))
            cursor += width
    managed = struct.unpack_from("<I", data, directories + 14 * 8)[0] != 0
    return machine, managed, imports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--arch", choices=["x64", "arm64"], required=True)
    parser.add_argument("--compiler-runtime-source", type=Path)
    args = parser.parse_args()
    expected = {"x64": 0x8664, "arm64": 0xaa64}[args.arch]
    if args.compiler_runtime_source:
        copied = 0
        for source in sorted(args.compiler_runtime_source.glob("*.dll")):
            machine, managed, _ = inspect(source.read_bytes())
            if machine == expected and not managed:
                shutil.copy2(source, args.directory / source.name)
                copied += 1
        if not copied:
            raise SystemExit(f"No {args.arch} VC runtime DLLs found in {args.compiler_runtime_source}")
        print(f"Staged {copied} {args.arch} VC runtime DLLs")
    bundled = {path.name.lower() for path in args.directory.iterdir() if path.is_file()}
    for name in ["YachtApp.exe", "yacht_ffi.dll", "yacht.exe", "yacht-update.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll"]:
        if not (args.directory / name).is_file():
            raise SystemExit("Missing packaged binary: " + name)
    checked = 0
    for path in sorted(args.directory.rglob("*")):
        if path.suffix.lower() not in (".dll", ".exe"):
            continue
        if "avalonia" in path.name.lower():
            raise SystemExit(f"Retired UI binary remains in package: {path.name}")
        machine, managed, imports = inspect(path.read_bytes())
        if managed:
            raise SystemExit(f"Managed .NET binary remains in native package: {path.name}")
        if machine != expected:
            raise SystemExit(f"Wrong native architecture: {path.name}")
        external = [name for name in imports if name != "msvcrt.dll" and name.startswith(("vcruntime", "msvcp", "msvcr", "concrt")) and name not in bundled]
        if external:
            raise SystemExit(f"Missing packaged VC runtime dependency for {path.name}: {', '.join(external)}")
        checked += 1
    print(f"Windows {args.arch}: {checked} PE images checked; native architecture and bundled VC runtime dependencies verified")


if __name__ == "__main__":
    main()
