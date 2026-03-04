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
CRC-32/ISO-HDLC: poly=0xEDB88320, init=0xFFFFFFFF, final XOR=0xFFFFFFFF.
This matches zlib.crc32(data) & 0xFFFFFFFF in Python and
esp_rom_crc32_le(UINT32_MAX, ...) ^ UINT32_MAX in the bootloader C code.

Default partition size: 1 MiB (matches partitions.csv factory entry).
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


DEFAULT_PARTITION_SIZE = 0x100000
ERASE_VALUE = 0xFF
DESCRIPTOR_SIZE = 8  # image_size (4 bytes LE) + crc32 (4 bytes LE)


def parse_int(text: str) -> int:
    return int(text, 0)


def compute_crc32(data: bytes) -> int:
    """CRC-32/ISO-HDLC — matches zlib.crc32() and esp_rom_crc32_le(^0xFFFF) ^ 0xFFFF."""
    return zlib.crc32(data) & 0xFFFFFFFF


def build_partition_image(app_image: bytes, partition_size: int) -> bytes:
    """
    Assemble the full partition image:
      [app_image][0xFF padding][image_size LE32][crc32 LE32]
    """
    max_payload = partition_size - DESCRIPTOR_SIZE
    if len(app_image) > max_payload:
        raise ValueError(
            f"App image ({len(app_image)} B) exceeds partition payload area ({max_payload} B)"
        )

    crc = compute_crc32(app_image)
    image_size = len(app_image)
    padding = bytes([ERASE_VALUE]) * (max_payload - image_size)
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
        default=DEFAULT_PARTITION_SIZE,
        help="Factory partition size in bytes (default: 0x100000 = 1 MiB)",
    )
    parser.add_argument(
        "--out-image",
        type=Path,
        default=Path("build/factory_crc_partition.bin"),
        help="Output full partition image (default: build/factory_crc_partition.bin)",
    )

    args = parser.parse_args()

    app_image = args.input.read_bytes()
    crc = compute_crc32(app_image)
    partition = build_partition_image(app_image, args.partition_size)

    args.out_image.parent.mkdir(parents=True, exist_ok=True)
    args.out_image.write_bytes(partition)

    print(f"Input app image    : {args.input}")
    print(f"Input image size   : {len(app_image)} bytes (0x{len(app_image):X})")
    print(f"CRC-32 (ISO-HDLC)  : 0x{crc:08X}")
    print(f"Partition size     : 0x{args.partition_size:08X} ({args.partition_size} bytes)")
    print(f"Descriptor offset  : 0x{(args.partition_size - DESCRIPTOR_SIZE):08X}")
    print(f"Output partition   : {args.out_image} ({len(partition)} bytes)")


if __name__ == "__main__":
    main()
