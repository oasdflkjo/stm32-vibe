#!/usr/bin/env python3

import argparse
import ast
import binascii
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TRACE_NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
FORMAT_RE = re.compile(
    r"%(?:[-+ #0]*)(?:\d+)?(?:\.\d+)?(?:l)?([diuoxXc%])"
)


@dataclass(frozen=True)
class TraceEvent:
    name: str
    format_string: str
    arg_types: tuple[str, ...]
    source: str
    line: int

    @property
    def event_id(self) -> int:
        return binascii.crc_hqx(self.name.encode("ascii"), 0xFFFF)


def strip_comments(source: str) -> str:
    output = list(source)
    index = 0
    state = "code"

    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if state == "code":
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "/" and next_char == "/":
                output[index] = output[index + 1] = " "
                index += 1
                state = "line_comment"
            elif char == "/" and next_char == "*":
                output[index] = output[index + 1] = " "
                index += 1
                state = "block_comment"
        elif state == "string":
            if char == "\\":
                index += 1
            elif char == '"':
                state = "code"
        elif state == "char":
            if char == "\\":
                index += 1
            elif char == "'":
                state = "code"
        elif state == "line_comment":
            if char == "\n":
                state = "code"
            else:
                output[index] = " "
        elif state == "block_comment":
            if char == "*" and next_char == "/":
                output[index] = output[index + 1] = " "
                index += 1
                state = "code"
            elif char != "\n":
                output[index] = " "

        index += 1

    return "".join(output)


def split_arguments(arguments: str) -> list[str]:
    parts = []
    start = 0
    depth = 0
    state = "code"
    index = 0

    while index < len(arguments):
        char = arguments[index]

        if state == "code":
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char in "([{":
                depth += 1
            elif char in ")]}":
                depth -= 1
            elif char == "," and depth == 0:
                parts.append(arguments[start:index].strip())
                start = index + 1
        elif state == "string":
            if char == "\\":
                index += 1
            elif char == '"':
                state = "code"
        elif state == "char":
            if char == "\\":
                index += 1
            elif char == "'":
                state = "code"

        index += 1

    parts.append(arguments[start:].strip())
    return parts


def parse_c_string(value: str, location: str) -> str:
    if not re.fullmatch(r'"(?:\\.|[^"\\])*"', value, flags=re.DOTALL):
        raise ValueError(f"{location}: trace format must be one C string literal")

    try:
        parsed = ast.literal_eval(value)
    except (SyntaxError, ValueError) as error:
        raise ValueError(f"{location}: invalid trace format string: {error}") from error

    if not isinstance(parsed, str):
        raise ValueError(f"{location}: trace format must decode to a string")

    return parsed


def parse_format(format_string: str, location: str) -> tuple[str, ...]:
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

    return tuple(arg_types)


def extract_trace_events(path: Path) -> list[TraceEvent]:
    source = path.read_text(encoding="utf-8")
    searchable = strip_comments(source)
    events = []
    index = 0

    while True:
        match = re.search(r"\bTRACE\s*\(", searchable[index:])
        if match is None:
            break

        call_start = index + match.start()
        open_paren = index + match.end() - 1
        cursor = open_paren + 1
        depth = 1
        state = "code"

        while cursor < len(searchable) and depth > 0:
            char = searchable[cursor]

            if state == "code":
                if char == '"':
                    state = "string"
                elif char == "'":
                    state = "char"
                elif char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
            elif state == "string":
                if char == "\\":
                    cursor += 1
                elif char == '"':
                    state = "code"
            elif state == "char":
                if char == "\\":
                    cursor += 1
                elif char == "'":
                    state = "code"

            cursor += 1

        line = source.count("\n", 0, call_start) + 1
        location = f"{path}:{line}"

        if depth != 0:
            raise ValueError(f"{location}: unterminated TRACE call")

        arguments = split_arguments(source[open_paren + 1:cursor - 1])
        if len(arguments) < 2:
            raise ValueError(f"{location}: TRACE requires an event name and format")

        name = arguments[0]
        if TRACE_NAME_RE.fullmatch(name) is None:
            raise ValueError(
                f"{location}: event name {name!r} must use uppercase identifiers"
            )

        format_string = parse_c_string(arguments[1], location)
        arg_types = parse_format(format_string, location)
        supplied_count = len(arguments) - 2

        if supplied_count != len(arg_types):
            raise ValueError(
                f"{location}: format expects {len(arg_types)} arguments, "
                f"TRACE call supplies {supplied_count}"
            )

        events.append(
            TraceEvent(
                name=name,
                format_string=format_string,
                arg_types=arg_types,
                source=str(path),
                line=line,
            )
        )
        index = cursor

    return events


def collect_events(paths: list[Path]) -> list[TraceEvent]:
    by_name: dict[str, TraceEvent] = {}
    by_id: dict[int, TraceEvent] = {}

    for path in paths:
        for event in extract_trace_events(path):
            previous = by_name.get(event.name)
            if previous is not None:
                if (
                    previous.format_string != event.format_string
                    or previous.arg_types != event.arg_types
                ):
                    raise ValueError(
                        f"{event.source}:{event.line}: event {event.name} conflicts with "
                        f"{previous.source}:{previous.line}"
                    )
                continue

            collision = by_id.get(event.event_id)
            if collision is not None:
                raise ValueError(
                    f"trace ID collision 0x{event.event_id:04X}: "
                    f"{event.name} and {collision.name}"
                )

            by_name[event.name] = event
            by_id[event.event_id] = event

    return sorted(by_name.values(), key=lambda event: event.name)


def render_header(events: list[TraceEvent]) -> str:
    lines = [
        "#pragma once",
        "",
        "/* Generated by tools/gen_trace.py. Do not edit. */",
        "",
    ]

    for event in events:
        params = ", ".join(f"arg{index}" for index in range(len(event.arg_types)))
        casts = ", ".join(
            f"(uint32_t)(arg{index})" for index in range(len(event.arg_types))
        )
        call_args = f", {casts}" if casts else ""

        lines.append(f"#define TRACE_ID_{event.name} UINT16_C(0x{event.event_id:04X})")
        lines.append(
            f"#define TRACE_CALL_{event.name}({params}) "
            f"trace_emit{len(event.arg_types)}(TRACE_ID_{event.name}{call_args})"
        )
        lines.append("")

    return "\n".join(lines)


def render_map(events: list[TraceEvent]) -> str:
    payload = {
        "schema": 1,
        "protocol": {
            "sync": "a55a",
            "version": 1,
            "argument_width_bits": 32,
            "crc": "crc-8-atm",
        },
        "events": {
            str(event.event_id): {
                "name": event.name,
                "format": event.format_string,
                "arg_types": list(event.arg_types),
                "source": event.source,
                "line": event.line,
            }
            for event in sorted(events, key=lambda item: item.event_id)
        },
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def write_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--map", required=True, dest="map_path", type=Path)
    parser.add_argument("sources", nargs="+", type=Path)
    args = parser.parse_args()

    try:
        events = collect_events(args.sources)
    except (OSError, ValueError) as error:
        print(f"gen_trace.py: {error}", file=sys.stderr)
        return 1

    write_if_changed(args.header, render_header(events))
    write_if_changed(args.map_path, render_map(events))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
