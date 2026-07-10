#!/usr/bin/env python3
"""Create boot-state flash images for host flashing tools."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


BOOT_STATE_MAGIC = 0x41545342
BOOT_STATE_VERSION = 1
BOOT_STATE_NO_SLOT = 0xFFFFFFFF
BOOT_SLOT_A = 0
BOOT_SLOT_B = 1
BOOT_SLOT_STATUS_EMPTY = 0
BOOT_SLOT_STATUS_CONFIRMED = 3
BOOT_STATE_WORDS = 20
BOOT_STATE_SIZE = BOOT_STATE_WORDS * 4
BOOT_STATE_CRC_WORD = 11
BOOT_STATE_COPY_B_OFFSET = 0x100


def build_record(active_slot: int, slot_a_status: int, slot_b_status: int) -> bytes:
    words = [
        BOOT_STATE_MAGIC,
        BOOT_STATE_VERSION,
        BOOT_STATE_SIZE,
        0,
        active_slot,
        BOOT_STATE_NO_SLOT,
        0,
        slot_a_status,
        slot_b_status,
        0,
        0,
        0,
        *([0] * 8),
    ]
    record = bytearray(struct.pack("<" + ("I" * BOOT_STATE_WORDS), *words))
    crc = zlib.crc32(record) & 0xFFFFFFFF
    struct.pack_into("<I", record, BOOT_STATE_CRC_WORD * 4, crc)
    return bytes(record)


def image_for_mode(mode: str, image_size: int) -> bytes:
    if image_size < BOOT_STATE_COPY_B_OFFSET + BOOT_STATE_SIZE:
        raise ValueError("boot-state image size is too small for both record copies")

    image = bytearray(b"\xff" * image_size)
    if mode == "empty":
        record = build_record(BOOT_SLOT_A, BOOT_SLOT_STATUS_EMPTY,
                              BOOT_SLOT_STATUS_EMPTY)
    elif mode == "confirmed-a":
        record = build_record(BOOT_SLOT_A, BOOT_SLOT_STATUS_CONFIRMED,
                              BOOT_SLOT_STATUS_EMPTY)
    elif mode == "confirmed-b":
        record = build_record(BOOT_SLOT_B, BOOT_SLOT_STATUS_EMPTY,
                              BOOT_SLOT_STATUS_CONFIRMED)
    else:
        raise ValueError(f"unknown boot-state mode: {mode}")

    image[0:BOOT_STATE_SIZE] = record
    return bytes(image)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create a boot-state flash image")
    parser.add_argument("--mode", choices=["empty", "confirmed-a", "confirmed-b"],
                        required=True)
    parser.add_argument("--size", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image_for_mode(args.mode, args.size))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
