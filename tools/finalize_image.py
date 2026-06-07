#!/usr/bin/env python3

import argparse
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


MANIFEST_FORMAT = "<IIII"
MANIFEST_SIZE = struct.calcsize(MANIFEST_FORMAT)
MANIFEST_MAGIC = 0x45424956
MANIFEST_OFFSET = 0x200


def finalize_bytes(image: bytes, version: int) -> tuple[bytes, bytes]:
    if len(image) < MANIFEST_OFFSET + MANIFEST_SIZE:
        raise ValueError("image does not contain the application manifest")
    if version < 0 or version > 0xFFFFFFFF:
        raise ValueError("version must fit in 32 bits")

    patched = bytearray(image)
    manifest = struct.pack(
        MANIFEST_FORMAT,
        MANIFEST_MAGIC,
        len(patched),
        0,
        version,
    )
    patched[MANIFEST_OFFSET:MANIFEST_OFFSET + MANIFEST_SIZE] = manifest

    image_crc32 = zlib.crc32(patched) & 0xFFFFFFFF
    manifest = struct.pack(
        MANIFEST_FORMAT,
        MANIFEST_MAGIC,
        len(patched),
        image_crc32,
        version,
    )
    patched[MANIFEST_OFFSET:MANIFEST_OFFSET + MANIFEST_SIZE] = manifest
    return bytes(patched), manifest


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def finalize_elf(
    elf: Path,
    binary: Path,
    version: int,
    objcopy: str,
) -> None:
    with tempfile.TemporaryDirectory() as directory:
        directory_path = Path(directory)
        raw_binary = directory_path / "image.bin"
        manifest_path = directory_path / "manifest.bin"
        patched_elf = directory_path / "image.elf"

        run([objcopy, "-O", "binary", str(elf), str(raw_binary)])
        patched_binary, manifest = finalize_bytes(raw_binary.read_bytes(), version)
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--bin", required=True, dest="binary", type=Path)
    parser.add_argument("--version", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = parser.parse_args()

    try:
        finalize_elf(args.elf, args.binary, args.version, args.objcopy)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"finalize_image.py: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
