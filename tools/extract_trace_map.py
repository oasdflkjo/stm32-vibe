#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


FORMAT_RE = re.compile(
    r"%(?:[-+ #0]*)(?:\d+)?(?:\.\d+)?(?:l)?([diuoxXc%])"
)
SYMBOL_RE = re.compile(
    r"^([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+\w\s+"
    r"(trace_format_([0-4])_\d+(?:\.\d+)?)$"
)


def parse_format(format_string: str, location: str) -> list[str]:
    arg_types = []
    index = 0

    while index < len(format_string):
        if format_string[index] != "%":
            index += 1
            continue

        match = FORMAT_RE.match(format_string, index)
        if match is None:
            raise ValueError(
                f"{location}: unsupported format near {format_string[index:index + 12]!r}; "
                "supported conversions are %d, %i, %u, %o, %x, %X, %c and %%"
            )

        conversion = match.group(1)
        if conversion in ("d", "i"):
            arg_types.append("i32")
        elif conversion in ("u", "o", "x", "X"):
            arg_types.append("u32")
        elif conversion == "c":
            arg_types.append("char")

        index = match.end()

    if len(arg_types) > 4:
        raise ValueError(f"{location}: at most four trace arguments are supported")

    return arg_types


def run_tool(command: list[str]) -> str:
    result = subprocess.run(
        command,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout


def read_symbols(nm: str, elf: Path) -> list[tuple[int, int, int, str]]:
    output = run_tool([nm, "-S", "--defined-only", str(elf)])
    symbols = []

    for line in output.splitlines():
        match = SYMBOL_RE.match(line.strip())
        if match is None:
            continue
        symbols.append(
            (
                int(match.group(1), 16),
                int(match.group(2), 16),
                int(match.group(4)),
                match.group(3),
            )
        )

    return sorted(symbols)


def read_trace_section(objcopy: str, elf: Path) -> bytes:
    with tempfile.TemporaryDirectory() as directory:
        output_path = Path(directory) / "trace_fmt.bin"
        subprocess.run(
            [
                objcopy,
                "--dump-section",
                f".trace_fmt={output_path}",
                str(elf),
            ],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        return output_path.read_bytes()


def build_map(elf: Path, nm: str, objcopy: str) -> dict:
    symbols = read_symbols(nm, elf)
    section = read_trace_section(objcopy, elf) if symbols else b""
    events = {}

    for event_id, size, argument_count, symbol_name in symbols:
        if event_id > 0xFFFF:
            raise ValueError(f"{symbol_name}: trace ID exceeds 16 bits")
        if event_id + size > len(section):
            raise ValueError(f"{symbol_name}: symbol lies outside .trace_fmt")

        raw_format = section[event_id:event_id + size]
        if not raw_format.endswith(b"\0"):
            raise ValueError(f"{symbol_name}: format string is not null terminated")

        format_string = raw_format[:-1].decode("utf-8")
        arg_types = parse_format(format_string, symbol_name)
        if len(arg_types) != argument_count:
            raise ValueError(
                f"{symbol_name}: format expects {len(arg_types)} arguments, "
                f"TRACE call supplies {argument_count}"
            )
        events[str(event_id)] = {
            "format": format_string,
            "arg_types": arg_types,
        }

    return {
        "schema": 1,
        "protocol": {
            "sync": "a55a",
            "version": 1,
            "argument_width_bits": 32,
            "crc": "crc-8-atm",
        },
        "events": events,
    }


def write_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--map", required=True, dest="map_path", type=Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = parser.parse_args()

    try:
        trace_map = build_map(args.elf, args.nm, args.objcopy)
    except (OSError, UnicodeDecodeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"extract_trace_map.py: {error}", file=sys.stderr)
        return 1

    write_if_changed(
        args.map_path,
        json.dumps(trace_map, indent=2, sort_keys=True) + "\n",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
