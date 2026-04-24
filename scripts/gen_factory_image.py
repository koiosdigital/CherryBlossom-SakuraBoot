#!/usr/bin/env python3
"""
Generate factory firmware image by combining SakuraBoot bootloader with application.

Downloads the latest sakura_boot.bin from GitHub releases,
verifies it fits in the bootloader region, and concatenates:
  bootloader (padded to 8KB) + firmware_with_meta.bin = firmware_factory.bin

Usage:
    python gen_factory_image.py <firmware_with_meta.bin> <output.bin> [--bootloader-size 8192]
"""

import argparse
import json
import sys
import urllib.request
import urllib.error
from pathlib import Path

GITHUB_API_URL = "https://api.github.com/repos/koiosdigital/CherryBlossom-SakuraBoot/releases/latest"
BOOTLOADER_ASSET_NAME = "sakura_boot.bin"
DEFAULT_BOOTLOADER_SIZE = 8192  # 8KB

# Cache directory for downloaded bootloaders
CACHE_DIR = Path(__file__).parent.parent / "build" / ".bootloader_cache"


def get_cached_bootloader(release_tag: str, expected_size: int) -> bytes | None:
    """Try to load bootloader from cache."""
    cache_file = CACHE_DIR / f"{BOOTLOADER_ASSET_NAME}.{release_tag}"
    if cache_file.exists():
        data = cache_file.read_bytes()
        if len(data) == expected_size:
            return data
        cache_file.unlink()
    return None


def cache_bootloader(release_tag: str, data: bytes) -> None:
    """Save bootloader to cache."""
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cache_file = CACHE_DIR / f"{BOOTLOADER_ASSET_NAME}.{release_tag}"
    cache_file.write_bytes(data)


def fetch_latest_release_info():
    """Fetch latest release info from GitHub API."""
    req = urllib.request.Request(
        GITHUB_API_URL,
        headers={"Accept": "application/vnd.github.v3+json", "User-Agent": "sakura-firmware-build"}
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            return json.loads(response.read().decode())
    except urllib.error.URLError as e:
        print(f"Error: Failed to fetch release info from GitHub: {e}", file=sys.stderr)
        sys.exit(1)


def download_bootloader(download_url: str) -> bytes:
    """Download bootloader binary from GitHub."""
    req = urllib.request.Request(
        download_url,
        headers={"User-Agent": "sakura-firmware-build"}
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as response:
            return response.read()
    except urllib.error.URLError as e:
        print(f"Error: Failed to download bootloader: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Generate factory firmware image with bootloader")
    parser.add_argument("firmware_with_meta", type=Path, help="Input firmware with metadata")
    parser.add_argument("output", type=Path, help="Output factory image")
    parser.add_argument("--bootloader-size", type=int, default=DEFAULT_BOOTLOADER_SIZE,
                        help=f"Bootloader region size in bytes (default: {DEFAULT_BOOTLOADER_SIZE})")
    parser.add_argument("--bootloader-bin", type=Path, default=None,
                        help="Use local bootloader binary instead of downloading from GitHub")
    args = parser.parse_args()

    if not args.firmware_with_meta.exists():
        print(f"Error: Input file not found: {args.firmware_with_meta}", file=sys.stderr)
        sys.exit(1)

    if args.bootloader_bin:
        # Use local bootloader binary
        if not args.bootloader_bin.exists():
            print(f"Error: Local bootloader not found: {args.bootloader_bin}", file=sys.stderr)
            sys.exit(1)
        bootloader_data = args.bootloader_bin.read_bytes()
        print(f"Using local bootloader: {args.bootloader_bin} ({len(bootloader_data)} bytes)")
    else:
        # Download from GitHub
        print(f"Fetching latest SakuraBoot release from GitHub...")
        release_info = fetch_latest_release_info()
        release_tag = release_info.get("tag_name", "unknown")
        print(f"Latest release: {release_tag}")

        bootloader_asset = None
        for asset in release_info.get("assets", []):
            if asset["name"] == BOOTLOADER_ASSET_NAME:
                bootloader_asset = asset
                break

        if not bootloader_asset:
            print(f"Error: Asset '{BOOTLOADER_ASSET_NAME}' not found in release {release_tag}", file=sys.stderr)
            sys.exit(1)

        reported_size = bootloader_asset.get("size", 0)
        print(f"Bootloader size (from API): {reported_size} bytes")

        if reported_size >= args.bootloader_size:
            print(f"Error: Bootloader ({reported_size} bytes) exceeds max ({args.bootloader_size} bytes)",
                  file=sys.stderr)
            sys.exit(1)

        bootloader_data = get_cached_bootloader(release_tag, reported_size)
        if bootloader_data:
            print(f"Using cached bootloader for {release_tag}")
        else:
            print(f"Downloading {BOOTLOADER_ASSET_NAME}...")
            download_url = bootloader_asset["browser_download_url"]
            bootloader_data = download_bootloader(download_url)
            cache_bootloader(release_tag, bootloader_data)
            print(f"Cached bootloader for {release_tag}")

    actual_size = len(bootloader_data)
    print(f"Bootloader size (actual): {actual_size} bytes")

    if actual_size >= args.bootloader_size:
        print(f"Error: Bootloader ({actual_size} bytes) exceeds max ({args.bootloader_size} bytes)",
              file=sys.stderr)
        sys.exit(1)

    padded_bootloader = bootloader_data + bytes([0xFF] * (args.bootloader_size - actual_size))
    print(f"Padded bootloader to {len(padded_bootloader)} bytes")

    firmware_data = args.firmware_with_meta.read_bytes()
    print(f"Firmware with metadata size: {len(firmware_data)} bytes")

    factory_image = padded_bootloader + firmware_data
    print(f"Factory image size: {len(factory_image)} bytes")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(factory_image)
    print(f"Written factory image to: {args.output}")


if __name__ == "__main__":
    main()
