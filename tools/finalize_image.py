#!/usr/bin/env python3

import argparse
import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


MANIFEST_RESERVED_WORDS = 8
MANIFEST_FORMAT = "<IIIIIIII" + ("I" * MANIFEST_RESERVED_WORDS)
MANIFEST_SIZE = struct.calcsize(MANIFEST_FORMAT)
MANIFEST_MAGIC = 0x45424956
MANIFEST_VERSION = 2
MANIFEST_OFFSET = 0x200
MANIFEST_CRC32_OFFSET = 16
MANIFEST_APP_ID_WORD = 0
MANIFEST_BOARD_ID_WORD = 1


def metadata_id(value: str) -> int:
    return zlib.crc32(value.encode("utf-8")) & 0xFFFFFFFF


def finalize_bytes(
    image: bytes,
    version: int,
    app_name: str,
    board_name: str,
) -> tuple[bytes, bytes, dict[str, int | str]]:
    if len(image) < MANIFEST_OFFSET + MANIFEST_SIZE:
        raise ValueError("image does not contain the application manifest")
    if version < 0 or version > 0xFFFFFFFF:
        raise ValueError("version must fit in 32 bits")

    patched = bytearray(image)
    reserved = [0] * MANIFEST_RESERVED_WORDS
    app_id = metadata_id(app_name)
    board_id = metadata_id(board_name)
    reserved[MANIFEST_APP_ID_WORD] = app_id
    reserved[MANIFEST_BOARD_ID_WORD] = board_id

    manifest = struct.pack(
        MANIFEST_FORMAT,
        MANIFEST_MAGIC,
        MANIFEST_VERSION,
        MANIFEST_SIZE,
        len(patched),
        0,
        version,
        board_id,
        0,
        *reserved,
    )
    patched[MANIFEST_OFFSET:MANIFEST_OFFSET + MANIFEST_SIZE] = manifest

    image_crc32 = zlib.crc32(patched) & 0xFFFFFFFF
    manifest = struct.pack(
        MANIFEST_FORMAT,
        MANIFEST_MAGIC,
        MANIFEST_VERSION,
        MANIFEST_SIZE,
        len(patched),
        image_crc32,
        version,
        board_id,
        0,
        *reserved,
    )
    patched[MANIFEST_OFFSET:MANIFEST_OFFSET + MANIFEST_SIZE] = manifest
    metadata = {
        "application_name": app_name,
        "application_id": app_id,
        "board_name": board_name,
        "board_id": board_id,
        "manifest_version": MANIFEST_VERSION,
        "manifest_size": MANIFEST_SIZE,
        "image_size": len(patched),
        "image_crc32": image_crc32,
        "software_version": version,
    }
    return bytes(patched), manifest, metadata


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def finalize_elf(
    elf: Path,
    binary: Path,
    version: int,
    app_name: str,
    board_name: str,
    metadata: Path | None,
    objcopy: str,
) -> None:
    with tempfile.TemporaryDirectory() as directory:
        directory_path = Path(directory)
        raw_binary = directory_path / "image.bin"
        manifest_path = directory_path / "manifest.bin"
        patched_elf = directory_path / "image.elf"

        run([objcopy, "-O", "binary", str(elf), str(raw_binary)])
        patched_binary, manifest, sidecar = finalize_bytes(
            raw_binary.read_bytes(), version, app_name, board_name
        )
        manifest_path.write_bytes(manifest)

        run(
            [
                objcopy,
                "--update-section",
                f".app_manifest={manifest_path}",
                str(elf),
                str(patched_elf),
            ]
        )

        elf.write_bytes(patched_elf.read_bytes())
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(patched_binary)
        metadata_path = metadata if metadata is not None else binary.with_suffix(".json")
        metadata_path.write_text(json.dumps(sidecar, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--bin", required=True, dest="binary", type=Path)
    parser.add_argument("--version", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--app-name", required=True)
    parser.add_argument("--board-name", required=True)
    parser.add_argument("--metadata", type=Path)
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = parser.parse_args()

    try:
        finalize_elf(
            args.elf,
            args.binary,
            args.version,
            args.app_name,
            args.board_name,
            args.metadata,
            args.objcopy,
        )
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"finalize_image.py: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
