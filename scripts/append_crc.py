#!/usr/bin/env python3
"""
Build CRC artifacts for the ESP32-C3 factory app partition.

Why this exists
---------------
The bootloader CRC check validates only the actual app image bytes using an
8-byte descriptor stored at the end of the partition:

    [ app image: image_size bytes ][ 0xFF padding ][ image_size: 4 B LE ][ crc32: 4 B LE ]

Why image_size + descriptor, not full-partition CRC
----------------------------------------------------
Hashing the full 1 MiB partition (app + 0xFF padding) is fragile:
- esptool may update SHA fields inside the app binary after writing to flash
- erased flash may contain non-0xFF bytes in edge cases
- any difference in a single padding byte silently invalidates the check

Only the actual app image bytes are hashed.
The descriptor tells the bootloader exactly how many bytes to read.

CRC algorithm
-------------
CRC-32/ISO-HDLC with the same API contract used by the bootloader code:
- Python: zlib.crc32(data) & 0xFFFFFFFF
- Bootloader: esp_rom_crc32_le(0, ...) with incremental updates

Both produce the same 32-bit value stored in the partition descriptor.

Default partition size: 1 MiB (matches partitions.csv factory entry).
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


DEFAULT_PARTITION_SIZE = 0x100000
DEFAULT_PARTITIONS_CSV = Path(__file__).resolve().parents[1] / "partitions.csv"
ERASE_VALUE = 0xFF
DESCRIPTOR_SIZE = 8  # image_size (4 bytes LE) + crc32 (4 bytes LE)


def parse_int(text: str) -> int:
    return int(text, 0)


def parse_partition_size_token(text: str) -> int:
    token = text.strip()
    if not token:
        raise ValueError("Empty partition size token")

    upper = token.upper()
    if upper.endswith("K"):
        base = int(upper[:-1], 0)
        return base * 1024
    if upper.endswith("M"):
        base = int(upper[:-1], 0)
        return base * 1024 * 1024

    return int(token, 0)


def resolve_factory_partition_size(partitions_csv: Path | None) -> int:
    if partitions_csv is None:
        return DEFAULT_PARTITION_SIZE

    if not partitions_csv.exists():
        raise FileNotFoundError(f"Partition table not found: {partitions_csv}")

    for raw_line in partitions_csv.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        fields = [field.strip() for field in stripped.split(",")]
        if len(fields) < 5:
            continue

        name, p_type, subtype, _, size = fields[:5]
        if name == "factory" and p_type == "app" and subtype == "factory":
            return parse_partition_size_token(size)

    raise ValueError(f"Could not resolve factory partition size from {partitions_csv}")


def compute_crc32(data: bytes) -> int:
    """CRC-32/ISO-HDLC compatible with bootloader-side esp_rom_crc32_le(0, ...)."""
    return zlib.crc32(data) & 0xFFFFFFFF


def build_partition_image(app_image: bytes, partition_size: int, corrupt_crc: bool = False) -> bytes:
    """
    Assemble the full partition image:
      [app_image][0xFF padding][image_size LE32][crc32 LE32]
    
    If app_image is already partition-sized, assume it's a full partition and handle CRC corruption in-place.
    """
    app_size = len(app_image)
    
    # If input is already partition-sized, assume it's a full partition image and rewrite descriptor in-place.
    if app_size == partition_size:
        partition = bytearray(app_image)
        image_size = int.from_bytes(partition[partition_size - DESCRIPTOR_SIZE:partition_size - 4], "little")

        # Fallback for legacy/uninitialized descriptor: detect payload by trimming 0xFF tail before descriptor region.
        if image_size == 0 or image_size > (partition_size - DESCRIPTOR_SIZE):
            payload = bytes(partition[:partition_size - DESCRIPTOR_SIZE])
            image_size = len(payload.rstrip(bytes([ERASE_VALUE])))

        payload = bytes(partition[:image_size])
        crc = compute_crc32(payload)
        if corrupt_crc:
            crc ^= 0xFFFFFFFF

        partition[partition_size - DESCRIPTOR_SIZE:partition_size] = struct.pack("<II", image_size, crc)
        return bytes(partition)
    elif app_size > partition_size - DESCRIPTOR_SIZE:
        raise ValueError(
            f"App image ({app_size} B) exceeds partition payload area ({partition_size - DESCRIPTOR_SIZE} B)"
        )
    
    # Build partition from app image
    crc = compute_crc32(app_image)
    if corrupt_crc:
        crc ^= 0xFFFFFFFF
    image_size = app_size
    padding = bytes([ERASE_VALUE]) * (partition_size - DESCRIPTOR_SIZE - image_size)
    descriptor = struct.pack("<II", image_size, crc)
    return app_image + padding + descriptor


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate CRC-valid factory partition image for ESP32-C3."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input app image (e.g. build/waveshare_esp32c3_zero_bootloader.bin)",
    )
    parser.add_argument(
        "--partition-size",
        type=parse_int,
        default=None,
        help="Factory partition size in bytes (overrides partitions.csv autodetection)",
    )
    parser.add_argument(
        "--partitions-csv",
        type=Path,
        default=DEFAULT_PARTITIONS_CSV,
        help="Partition table CSV used to autodetect factory partition size",
    )
    parser.add_argument(
        "--out-image",
        type=Path,
        default=None,
        help="Output full partition image (default: input file when --bad-crc, else build/factory_crc_partition.bin)",
    )
    parser.add_argument(
        "--bad-crc",
        action="store_true",
        help="Corrupt the CRC for testing failure path (flips all bits)",
    )

    args = parser.parse_args()

    if args.partition_size is None:
        args.partition_size = resolve_factory_partition_size(args.partitions_csv)

    # Default output: always build/factory_crc_partition.bin.
    # Never overwrite the app binary — that confuses ninja's dependency tracking.
    if args.out_image is None:
        args.out_image = Path("build/factory_crc_partition.bin")

    app_image = args.input.read_bytes()
    crc = compute_crc32(app_image)
    partition = build_partition_image(app_image, args.partition_size, args.bad_crc)

    args.out_image.parent.mkdir(parents=True, exist_ok=True)
    args.out_image.write_bytes(partition)

    crc_display = crc
    if args.bad_crc:
        crc_display ^= 0xFFFFFFFF
        print(f"WARNING: CRC corrupted for testing (actual CRC: 0x{crc:08X})")

    print(f"Input app image    : {args.input}")
    print(f"Input image size   : {len(app_image)} bytes (0x{len(app_image):X})")
    print(f"CRC-32 (ISO-HDLC)  : 0x{crc_display:08X}")
    print(f"Partition size     : 0x{args.partition_size:08X} ({args.partition_size} bytes)")
    print(f"Descriptor offset  : 0x{(args.partition_size - DESCRIPTOR_SIZE):08X}")
    print(f"Output partition   : {args.out_image} ({len(partition)} bytes)")


if __name__ == "__main__":
    main()
