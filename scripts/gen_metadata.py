#!/usr/bin/env python3
"""Generate Terraboot-compatible metadata page for sakura firmware binary.

Prepends a 1KB metadata page to the firmware binary containing magic,
app size, hash, version, variant name, and CRC for boot validation.

Usage:
    python gen_metadata.py input.bin output.bin [--app-version 0x00010203] [--app-variant-name sakura]
"""

import argparse
import struct
import sys

MAGIC = b"TERRABOOT_RaiseTheWorld\x00"  # 24 bytes
DEFAULT_PAGE_SIZE = 1024  # 1KB for STM32F103


def _mix(h: int, m: int) -> int:
    h ^= h >> 23
    h = (h * 0x2127599bf4325c37) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 47
    return h


def fasthash64(data: bytes, seed: int = 0) -> int:
    m = 0x880355f21e6d1965
    h = seed ^ ((len(data) * m) & 0xFFFFFFFFFFFFFFFF)

    pos = 0
    while pos + 8 <= len(data):
        k = struct.unpack_from('<Q', data, pos)[0]
        pos += 8
        h ^= _mix(k, m)
        h = (h * m) & 0xFFFFFFFFFFFFFFFF

    remaining = len(data) - pos
    if remaining > 0:
        v = 0
        for i in range(remaining):
            v |= data[pos + i] << (i * 8)
        h ^= _mix(v, m)
        h = (h * m) & 0xFFFFFFFFFFFFFFFF

    return _mix(h, m)


def fasthash32(data: bytes, seed: int = 0) -> int:
    h = fasthash64(data, seed)
    return ((h - (h >> 32)) & 0xFFFFFFFFFFFFFFFF) & 0xFFFFFFFF


def generate_metadata(app_binary: bytes, page_size: int,
                     app_version: int = 0,
                     app_variant_name: str = "unknown") -> bytes:
    """Generate metadata page.

    Structure (84 bytes):
        char magic[24]          - "TERRABOOT_RaiseTheWorld\\0"
        uint32_t app_size       - Size of app in bytes
        uint64_t app_hash       - fasthash64 of app data
        uint32_t app_version    - 0x00XXYYZZ format
        char app_variant_name[32] - null-terminated
        uint8_t reserved[8]     - 0xFF
        uint32_t meta_crc       - fasthash32 of fields above
    """
    app_size = len(app_binary)
    app_hash = fasthash64(app_binary, 0)

    if len(app_variant_name) > 31:
        app_variant_name = app_variant_name[:31]

    variant_bytes = app_variant_name.encode('ascii', errors='replace').ljust(32, b'\x00')

    meta_without_crc = struct.pack(
        '<24sIQI32s8s',
        MAGIC,
        app_size,
        app_hash,
        app_version,
        variant_bytes,
        b'\xff' * 8
    )

    meta_crc = fasthash32(meta_without_crc, 0)

    metadata = struct.pack(
        '<24sIQI32s8sI',
        MAGIC,
        app_size,
        app_hash,
        app_version,
        variant_bytes,
        b'\xff' * 8,
        meta_crc
    )

    padding = page_size - len(metadata)
    if padding < 0:
        raise ValueError(f"Metadata ({len(metadata)} bytes) exceeds page size ({page_size} bytes)")

    return metadata + (b'\xff' * padding)


def main():
    parser = argparse.ArgumentParser(
        description='Generate Terraboot metadata page for sakura firmware'
    )
    parser.add_argument('input', help='Input firmware binary')
    parser.add_argument('output', help='Output binary with metadata prepended')
    parser.add_argument('--page-size', type=int, default=DEFAULT_PAGE_SIZE)
    parser.add_argument('--metadata-only', action='store_true',
                        help='Output only the metadata page')
    parser.add_argument('--app-version', type=lambda x: int(x, 0), default=0,
                        help='App version in 0x00XXYYZZ format')
    parser.add_argument('--app-variant-name', type=str, default='sakura',
                        help='App variant name (default: sakura)')
    args = parser.parse_args()

    try:
        with open(args.input, 'rb') as f:
            app_binary = f.read()
    except FileNotFoundError:
        print(f"Error: Input file '{args.input}' not found", file=sys.stderr)
        sys.exit(1)

    if len(app_binary) == 0:
        print("Error: Input file is empty", file=sys.stderr)
        sys.exit(1)

    metadata_page = generate_metadata(app_binary, args.page_size,
                                     args.app_version, args.app_variant_name)

    with open(args.output, 'wb') as f:
        f.write(metadata_page)
        if not args.metadata_only:
            f.write(app_binary)

    print(f"Generated {args.output}")
    print(f"  App size: {len(app_binary)} bytes (0x{len(app_binary):x})")
    print(f"  App hash: 0x{fasthash64(app_binary, 0):016x}")
    print(f"  App version: 0x{args.app_version:08x}", end="")
    if args.app_version > 0:
        major = (args.app_version >> 16) & 0xFF
        minor = (args.app_version >> 8) & 0xFF
        patch = args.app_version & 0xFF
        print(f" (v{major}.{minor}.{patch})")
    else:
        print(" (dev)")
    print(f"  Variant: {args.app_variant_name}")
    if args.metadata_only:
        print(f"  Output: metadata only ({args.page_size} bytes)")
    else:
        print(f"  Total size: {len(metadata_page) + len(app_binary)} bytes")


if __name__ == '__main__':
    main()
