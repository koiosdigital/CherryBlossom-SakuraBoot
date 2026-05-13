#!/usr/bin/env python3
"""Embed sakura_boot.bin as a C source for the application.

Produces a single .c file containing:
  - sakura_boot_image[]            — raw bootloader bytes, padded to even length
  - sakura_boot_image_size         — number of valid bytes in the array
  - sakura_boot_region_size        — size of the flash region the bootloader
                                     occupies (bytes beyond image_size are
                                     erased 0xFF in the live region)
  - sakura_boot_image_hash         — fasthash64 over the full region image,
                                     i.e. image bytes followed by 0xFF padding
                                     up to sakura_boot_region_size. This is
                                     exactly what hashing the live 0x08000000
                                     window of length region_size yields after
                                     a successful flash.

Source for the bootloader is either --bootloader-bin (local path, preferred
for dev builds) or the latest GitHub release (same logic as
gen_factory_image.py — they should always agree on which build is shipped).

Usage:
    python gen_embedded_bootloader.py output.c \\
        [--bootloader-bin path/to/sakura_boot.bin] \\
        [--region-size 8192]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import urllib.error
import urllib.request
from pathlib import Path

GITHUB_API_URL = "https://api.github.com/repos/koiosdigital/CherryBlossom-SakuraBoot/releases/latest"
BOOTLOADER_ASSET_NAME = "sakura_boot.bin"
DEFAULT_REGION_SIZE = 8192
CACHE_DIR = Path(__file__).parent.parent / "build" / ".bootloader_cache"


# ── fasthash64 (byte-for-byte identical to sakura fasthash.c) ────────────

def _mix(h: int) -> int:
    h ^= h >> 23
    h = (h * 0x2127599BF4325C37) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 47
    return h


def fasthash64(data: bytes, seed: int = 0) -> int:
    m = 0x880355F21E6D1965
    h = seed ^ ((len(data) * m) & 0xFFFFFFFFFFFFFFFF)

    pos = 0
    while pos + 8 <= len(data):
        k = struct.unpack_from("<Q", data, pos)[0]
        pos += 8
        h ^= _mix(k)
        h = (h * m) & 0xFFFFFFFFFFFFFFFF

    remaining = len(data) - pos
    if remaining > 0:
        v = 0
        for i in range(remaining):
            v |= data[pos + i] << (i * 8)
        h ^= _mix(v)
        h = (h * m) & 0xFFFFFFFFFFFFFFFF

    return _mix(h)


# ── Bootloader sourcing (local path or GitHub) ───────────────────────────

def _get_cached(tag: str, expected_size: int) -> bytes | None:
    f = CACHE_DIR / f"{BOOTLOADER_ASSET_NAME}.{tag}"
    if f.exists():
        data = f.read_bytes()
        if len(data) == expected_size:
            return data
        f.unlink()
    return None


def _cache(tag: str, data: bytes) -> None:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    (CACHE_DIR / f"{BOOTLOADER_ASSET_NAME}.{tag}").write_bytes(data)


def _fetch_release_info() -> dict:
    req = urllib.request.Request(
        GITHUB_API_URL,
        headers={
            "Accept": "application/vnd.github.v3+json",
            "User-Agent": "sakura-firmware-build",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read().decode())
    except urllib.error.URLError as e:
        print(f"Error: failed to fetch release info: {e}", file=sys.stderr)
        sys.exit(1)


def _download(url: str) -> bytes:
    req = urllib.request.Request(
        url, headers={"User-Agent": "sakura-firmware-build"}
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.read()
    except urllib.error.URLError as e:
        print(f"Error: bootloader download failed: {e}", file=sys.stderr)
        sys.exit(1)


def resolve_bootloader(local: Path | None, region_size: int) -> tuple[bytes, str]:
    """Return (raw_bytes, provenance_string)."""
    if local is not None:
        if not local.exists():
            print(f"Error: local bootloader not found: {local}", file=sys.stderr)
            sys.exit(1)
        data = local.read_bytes()
        return data, f"local:{local}"

    info = _fetch_release_info()
    tag = info.get("tag_name", "unknown")
    asset = next(
        (a for a in info.get("assets", []) if a["name"] == BOOTLOADER_ASSET_NAME),
        None,
    )
    if asset is None:
        print(
            f"Error: asset '{BOOTLOADER_ASSET_NAME}' missing from release {tag}",
            file=sys.stderr,
        )
        sys.exit(1)
    reported = asset.get("size", 0)
    if reported >= region_size:
        print(
            f"Error: bootloader ({reported} B) does not fit in {region_size} B region",
            file=sys.stderr,
        )
        sys.exit(1)
    cached = _get_cached(tag, reported)
    if cached is not None:
        return cached, f"github:{tag} (cached)"
    data = _download(asset["browser_download_url"])
    _cache(tag, data)
    return data, f"github:{tag}"


# ── C source emission ────────────────────────────────────────────────────

C_TEMPLATE = """\
/* AUTO-GENERATED by scripts/gen_embedded_bootloader.py — do not edit. */

#include "bootloader_update.h"

#include <stdint.h>

/* Provenance: {provenance} */
/* Raw bootloader size: {raw_size} bytes */
/* Padded array size:   {array_size} bytes (rounded up to half-word) */
/* Region size:         {region_size} bytes (image + 0xFF tail) */
/* Region hash:         0x{hash:016x} */

const uint32_t sakura_boot_image_size  = {array_size}u;
const uint32_t sakura_boot_region_size = {region_size}u;
const uint64_t sakura_boot_image_hash  = 0x{hash:016x}ULL;

/* Aligned so the update routine can read half-words off the array directly
 * if it wants to skip the byte-pair shuffle on a hot path. */
const uint8_t sakura_boot_image[{array_size}] __attribute__((aligned(4))) = {{
{body}
}};
"""


def emit_c(out: Path, image: bytes, region_size: int, provenance: str) -> None:
    raw_size = len(image)
    if raw_size & 1:
        image = image + b"\xff"  # half-word align
    array_size = len(image)

    # Hash matches what the live region looks like after a successful flash:
    # the embedded bytes followed by 0xFF up to region_size.
    region_image = image + b"\xff" * (region_size - array_size)
    region_hash = fasthash64(region_image, 0)

    lines: list[str] = []
    per_line = 16
    for i in range(0, array_size, per_line):
        chunk = image[i : i + per_line]
        bytes_str = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"    {bytes_str},")
    body = "\n".join(lines)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        C_TEMPLATE.format(
            provenance=provenance,
            raw_size=raw_size,
            array_size=array_size,
            region_size=region_size,
            hash=region_hash,
            body=body,
        )
    )
    print(f"Wrote {out}")
    print(f"  Provenance:  {provenance}")
    print(f"  Raw size:    {raw_size} B")
    print(f"  Padded size: {array_size} B")
    print(f"  Region:      {region_size} B")
    print(f"  Region hash: 0x{region_hash:016x}")


# ── CLI ──────────────────────────────────────────────────────────────────

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("output", type=Path, help="Output .c file path")
    p.add_argument(
        "--bootloader-bin",
        type=Path,
        default=None,
        help="Use local bootloader binary instead of downloading from GitHub",
    )
    p.add_argument(
        "--region-size",
        type=int,
        default=DEFAULT_REGION_SIZE,
        help=f"Bootloader region size in bytes (default: {DEFAULT_REGION_SIZE})",
    )
    args = p.parse_args()

    image, provenance = resolve_bootloader(args.bootloader_bin, args.region_size)
    if len(image) >= args.region_size:
        print(
            f"Error: bootloader ({len(image)} B) does not fit in "
            f"{args.region_size} B region",
            file=sys.stderr,
        )
        sys.exit(1)

    emit_c(args.output, image, args.region_size, provenance)


if __name__ == "__main__":
    main()
