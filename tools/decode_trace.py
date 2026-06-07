#!/usr/bin/env python3

import argparse
import json
import struct
import sys
import time
from pathlib import Path


TRACE_SYNC = b"\xA5\x5A"
TRACE_VERSION = 1
TRACE_MAX_ARGS = 4


def crc8(data: bytes) -> int:
    crc = 0

    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF

    return crc


class ItmDecoder:
    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> bytes:
        self.buffer.extend(data)
        output = bytearray()

        while self.buffer:
            header = self.buffer[0]

            if header == 0x00:
                sync_end = 0
                while sync_end < len(self.buffer) and self.buffer[sync_end] == 0x00:
                    sync_end += 1
                if sync_end == len(self.buffer):
                    break
                if self.buffer[sync_end] == 0x80:
                    del self.buffer[:sync_end + 1]
                else:
                    del self.buffer[0]
                continue

            size_code = header & 0x03
            if size_code != 0:
                payload_size = {1: 1, 2: 2, 3: 4}[size_code]
                if len(self.buffer) < payload_size + 1:
                    break

                is_software = (header & 0x04) == 0
                port = header >> 3
                payload = self.buffer[1:payload_size + 1]
                del self.buffer[:payload_size + 1]

                if is_software and port == 0:
                    output.extend(payload)
                continue

            if header == 0x70:
                del self.buffer[0]
                continue

            packet_end = 1
            while packet_end < len(self.buffer):
                value = self.buffer[packet_end]
                packet_end += 1
                if value & 0x80 == 0:
                    break
            else:
                break

            del self.buffer[:packet_end]

        return bytes(output)


class FrameDecoder:
    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[tuple[int, list[int]]]:
        self.buffer.extend(data)
        records = []

        while True:
            sync_index = self.buffer.find(TRACE_SYNC)
            if sync_index < 0:
                if self.buffer[-1:] == TRACE_SYNC[:1]:
                    del self.buffer[:-1]
                else:
                    self.buffer.clear()
                break

            if sync_index > 0:
                del self.buffer[:sync_index]

            if len(self.buffer) < 7:
                break

            version = self.buffer[2]
            event_id = self.buffer[3] | (self.buffer[4] << 8)
            arg_count = self.buffer[5]

            if version != TRACE_VERSION or arg_count > TRACE_MAX_ARGS:
                del self.buffer[0]
                continue

            frame_size = 7 + (arg_count * 4)
            if len(self.buffer) < frame_size:
                break

            frame = bytes(self.buffer[:frame_size])
            if crc8(frame[2:-1]) != frame[-1]:
                del self.buffer[0]
                continue

            args = [
                struct.unpack_from("<I", frame, 6 + (index * 4))[0]
                for index in range(arg_count)
            ]
            records.append((event_id, args))
            del self.buffer[:frame_size]

        return records


def normalize_format(format_string: str) -> str:
    output = []
    index = 0

    while index < len(format_string):
        if format_string[index] != "%":
            output.append(format_string[index])
            index += 1
            continue

        start = index
        index += 1
        if index < len(format_string) and format_string[index] == "%":
            output.append("%%")
            index += 1
            continue

        while index < len(format_string) and format_string[index] not in "diuoxXc":
            index += 1
        if index >= len(format_string):
            output.append(format_string[start:])
            break

        specifier = format_string[start:index + 1].replace("l", "")
        output.append(specifier)
        index += 1

    return "".join(output)


def format_record(event: dict, raw_args: list[int]) -> str:
    values = []
    for arg_type, value in zip(event["arg_types"], raw_args, strict=True):
        if arg_type == "i32" and value & 0x80000000:
            value -= 1 << 32
        values.append(value)

    if not values:
        return event["format"]

    return normalize_format(event["format"]) % tuple(values)


def load_events(map_paths: list[Path]) -> dict:
    events = {}

    for map_path in map_paths:
        trace_map = json.loads(map_path.read_text(encoding="utf-8"))
        for event_id, event in trace_map["events"].items():
            if event_id in events:
                raise ValueError(f"duplicate trace event ID {event_id}")
            events[event_id] = event

    return events


def decode_stream(map_paths: list[Path], input_path: Path, follow: bool) -> int:
    events = load_events(map_paths)
    itm_decoder = ItmDecoder()
    frame_decoder = FrameDecoder()

    with input_path.open("rb") as stream:
        while True:
            chunk = stream.read(4096)
            if chunk:
                trace_bytes = itm_decoder.feed(chunk)
                for event_id, args in frame_decoder.feed(trace_bytes):
                    event = events.get(str(event_id))
                    if event is None:
                        print(f"[unknown trace event 0x{event_id:04X}] {args}")
                    else:
                        print(format_record(event, args), flush=True)
                continue

            if not follow:
                break
            time.sleep(0.05)

    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--map",
        required=True,
        action="append",
        dest="map_paths",
        type=Path,
    )
    parser.add_argument("--input", required=True, dest="input_path", type=Path)
    parser.add_argument("--follow", action="store_true")
    args = parser.parse_args()

    try:
        return decode_stream(args.map_paths, args.input_path, args.follow)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"decode_trace.py: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
