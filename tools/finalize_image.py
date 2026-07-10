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
MANIFEST_VECTOR_WORDS_WORD = 2
MANIFEST_GOT_OFFSET_WORD = 3
MANIFEST_GOT_SIZE_WORD = 4
MANIFEST_DATA_LOAD_OFFSET_WORD = 5


def metadata_id(value: str) -> int:
    return zlib.crc32(value.encode("utf-8")) & 0xFFFFFFFF


def finalize_bytes(
    image: bytes,
    version: int,
    app_name: str,
    board_name: str,
    relocation: dict[str, int] | None = None,
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
    if relocation is not None:
        reserved[MANIFEST_VECTOR_WORDS_WORD] = relocation["vector_words"]
        reserved[MANIFEST_GOT_OFFSET_WORD] = relocation["got_offset"]
        reserved[MANIFEST_GOT_SIZE_WORD] = relocation["got_size"]
        reserved[MANIFEST_DATA_LOAD_OFFSET_WORD] = relocation["data_load_offset"]

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
        "vector_words": reserved[MANIFEST_VECTOR_WORDS_WORD],
        "got_offset": reserved[MANIFEST_GOT_OFFSET_WORD],
        "got_size": reserved[MANIFEST_GOT_SIZE_WORD],
        "data_load_offset": reserved[MANIFEST_DATA_LOAD_OFFSET_WORD],
    }
    return bytes(patched), manifest, metadata


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def read_relocation_metadata(elf: Path, nm: str) -> dict[str, int]:
    result = subprocess.run(
        [nm, "-n", str(elf)], check=True, capture_output=True, text=True
    )
    required = {
        "__vector_start", "__vector_end", "__got_start", "__got_end",
        "__data_load_start",
    }
    symbols: dict[str, int] = {}
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] in required:
            symbols[parts[2]] = int(parts[0], 16)
    missing = required - symbols.keys()
    if missing:
        raise ValueError(f"ELF missing relocation symbols: {', '.join(sorted(missing))}")

    image_origin = symbols["__vector_start"]
    vector_size = symbols["__vector_end"] - image_origin
    got_offset = symbols["__got_start"] - image_origin
    got_size = symbols["__got_end"] - symbols["__got_start"]
    data_load_offset = symbols["__data_load_start"] - image_origin
    if vector_size <= 0 or (vector_size & 3) or got_offset < 0 or got_size < 0:
        raise ValueError("invalid relocation symbol layout")
    if data_load_offset < 0:
        raise ValueError("data load address precedes image origin")
    return {
        "vector_words": vector_size // 4,
        "got_offset": got_offset,
        "got_size": got_size,
        "data_load_offset": data_load_offset,
    }


def finalize_elf(
    elf: Path,
    binary: Path,
    version: int,
    app_name: str,
    board_name: str,
    metadata: Path | None,
    objcopy: str,
    nm: str,
) -> None:
    with tempfile.TemporaryDirectory() as directory:
        directory_path = Path(directory)
        raw_binary = directory_path / "image.bin"
        manifest_path = directory_path / "manifest.bin"
        patched_elf = directory_path / "image.elf"

        run([objcopy, "-O", "binary", str(elf), str(raw_binary)])
        relocation = read_relocation_metadata(elf, nm)
        patched_binary, manifest, sidecar = finalize_bytes(
            raw_binary.read_bytes(), version, app_name, board_name, relocation
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
    parser.add_argument("--nm", default="arm-none-eabi-nm")
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
            args.nm,
        )
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"finalize_image.py: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
