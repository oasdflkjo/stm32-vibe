#!/usr/bin/env python3

import argparse
import curses
import json
import struct
import sys
import time
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO


SYNC = 0x5544
VERSION = 1
HEADER_SIZE = 16
CRC_SIZE = 4
MAX_PAYLOAD = 128
MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD + CRC_SIZE
MANIFEST_OFFSET = 0x200
MANIFEST_CRC32_OFFSET = 16

CMD_DISCOVER = 1
CMD_BEGIN = 3
CMD_BLOCK = 4
CMD_END = 5
CMD_VALIDATE = 6
CMD_ACTIVATE = 7
CMD_ACK = 10
CMD_ENTER_UPDATE = 11

STATUS_OK = 0
DEFAULT_PROBE_TIMEOUT_S = 10.0
DEFAULT_PROBE_INTERVAL_S = 0.05

COMMAND_NAMES = {
    CMD_DISCOVER: "DISCOVER",
    CMD_BEGIN: "BEGIN",
    CMD_BLOCK: "BLOCK",
    CMD_END: "END",
    CMD_VALIDATE: "VALIDATE",
    CMD_ACTIVATE: "ACTIVATE",
    CMD_ACK: "ACK",
    CMD_ENTER_UPDATE: "ENTER_UPDATE",
}


class UpdateError(Exception):
    pass


class UpdateUi:
    def __init__(self) -> None:
        self.state = "idle"
        self.progress_current = 0
        self.progress_total = 0
        self.logs: list[str] = []

    def __enter__(self):
        self.render()
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()

    def set_state(self, state: str) -> None:
        self.state = state
        self.render()

    def set_progress(self, current: int, total: int) -> None:
        self.progress_current = current
        self.progress_total = total
        self.render()

    def log(self, message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.logs.append(f"{timestamp} {message}")
        self.render()

    def render(self) -> None:
        pass

    def close(self) -> None:
        pass


class PlainUi(UpdateUi):
    def log(self, message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.logs.append(f"{timestamp} {message}")
        print(f"[{timestamp}] {message}")


class CursesUi(UpdateUi):
    def __init__(self, title: str, metadata: dict, port: str) -> None:
        super().__init__()
        self.title = title
        self.metadata = metadata
        self.port = port
        self.screen = None

    def __enter__(self):
        self.screen = curses.initscr()
        curses.noecho()
        curses.cbreak()
        self.screen.keypad(True)
        try:
            curses.curs_set(0)
        except curses.error:
            pass
        self.render()
        return self

    def close(self) -> None:
        if self.screen is None:
            return
        self.screen.keypad(False)
        curses.nocbreak()
        curses.echo()
        curses.endwin()
        self.screen = None

    def _add(self, row: int, col: int, text: str, attr: int = 0) -> None:
        if self.screen is None:
            return
        height, width = self.screen.getmaxyx()
        if row >= height or col >= width:
            return
        self.screen.addnstr(row, col, text, max(0, width - col - 1), attr)

    def render(self) -> None:
        if self.screen is None:
            return

        self.screen.erase()
        height, width = self.screen.getmaxyx()
        app_name = self.metadata.get("application_name", "?")
        board_name = self.metadata.get("board_name", "?")
        image_size = self.metadata.get("image_size", 0)
        image_crc32 = self.metadata.get("image_crc32", 0)
        percent = 0
        if self.progress_total:
            percent = int((self.progress_current * 100) / self.progress_total)

        self._add(0, 0, self.title[:width - 1], curses.A_BOLD)
        self._add(2, 0, f"Port: {self.port}")
        self._add(3, 0, f"Image: {app_name} for {board_name}")
        self._add(4, 0, f"Size: {image_size} bytes   CRC32: 0x{image_crc32:08X}")
        self._add(6, 0, f"State: {self.state}")

        bar_width = max(10, width - 18)
        filled = 0
        if self.progress_total:
            filled = int((self.progress_current * bar_width) / self.progress_total)
        bar = "[" + ("#" * filled) + ("." * (bar_width - filled)) + "]"
        self._add(8, 0, bar)
        self._add(8, min(width - 8, bar_width + 3), f"{percent:3d}%")
        self._add(9, 0, f"{self.progress_current}/{self.progress_total} bytes")

        self._add(11, 0, "Log", curses.A_BOLD)
        log_rows = max(0, height - 13)
        for index, line in enumerate(self.logs[-log_rows:]):
            self._add(12 + index, 0, line)

        self.screen.refresh()


@dataclass(frozen=True)
class Packet:
    command: int
    flags: int = 0
    session_id: int = 0
    sequence: int = 0
    payload: bytes = b""


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def image_manifest_crc32(image: bytes) -> int:
    patched = bytearray(image)
    crc_offset = MANIFEST_OFFSET + MANIFEST_CRC32_OFFSET
    if len(patched) >= crc_offset + 4:
        patched[crc_offset:crc_offset + 4] = b"\x00\x00\x00\x00"
    return crc32(bytes(patched))


def encode_packet(packet: Packet) -> bytes:
    if len(packet.payload) > MAX_PAYLOAD:
        raise ValueError("payload exceeds update protocol maximum")

    header = struct.pack(
        "<HBBBBHII",
        SYNC,
        VERSION,
        packet.command,
        packet.flags,
        0,
        len(packet.payload),
        packet.session_id,
        packet.sequence,
    )
    body = header + packet.payload
    return body + struct.pack("<I", crc32(body))


def decode_packet(data: bytes) -> Packet:
    if len(data) < HEADER_SIZE + CRC_SIZE:
        raise UpdateError("short packet")

    sync, version, command, flags, _reserved, payload_len, session_id, sequence = (
        struct.unpack("<HBBBBHII", data[:HEADER_SIZE])
    )
    if sync != SYNC:
        raise UpdateError("bad sync")
    if version != VERSION:
        raise UpdateError("bad protocol version")
    if payload_len > MAX_PAYLOAD:
        raise UpdateError("bad payload length")

    expected_len = HEADER_SIZE + payload_len + CRC_SIZE
    if len(data) != expected_len:
        raise UpdateError("packet length mismatch")

    expected_crc = struct.unpack("<I", data[-CRC_SIZE:])[0]
    actual_crc = crc32(data[:-CRC_SIZE])
    if actual_crc != expected_crc:
        raise UpdateError("bad packet crc")

    return Packet(
        command=command,
        flags=flags,
        session_id=session_id,
        sequence=sequence,
        payload=data[HEADER_SIZE:HEADER_SIZE + payload_len],
    )


def read_exact(port: BinaryIO, size: int, timeout_s: float) -> bytes:
    deadline = time.monotonic() + timeout_s
    data = bytearray()
    while len(data) < size:
        if time.monotonic() > deadline:
            raise TimeoutError("timed out waiting for bootloader response")
        chunk = port.read(size - len(data))
        if chunk:
            data.extend(chunk)
        else:
            time.sleep(0.005)
    return bytes(data)


def read_packet(port: BinaryIO, timeout_s: float) -> Packet:
    deadline = time.monotonic() + timeout_s
    window = bytearray()

    while True:
        if time.monotonic() > deadline:
            raise TimeoutError("timed out waiting for packet sync")
        byte = port.read(1)
        if not byte:
            time.sleep(0.005)
            continue
        window.append(byte[0])
        if len(window) > 2:
            del window[0]
        if len(window) == 2 and window[0] == (SYNC & 0xFF) and window[1] == (SYNC >> 8):
            rest = read_exact(port, HEADER_SIZE - 2, timeout_s)
            header = bytes(window) + rest
            payload_len = struct.unpack("<H", header[6:8])[0]
            if payload_len > MAX_PAYLOAD:
                raise UpdateError("response payload too large")
            tail = read_exact(port, payload_len + CRC_SIZE, timeout_s)
            return decode_packet(header + tail)


def send_packet(port: BinaryIO, packet: Packet, inter_byte_delay_s: float = 0.0) -> None:
    encoded = encode_packet(packet)
    if inter_byte_delay_s > 0.0:
        for byte in encoded:
            port.write(bytes([byte]))
            flush = getattr(port, "flush", None)
            if flush is not None:
                flush()
            time.sleep(inter_byte_delay_s)
    else:
        port.write(encoded)
    flush = getattr(port, "flush", None)
    if flush is not None:
        flush()


def expect_ack(port: BinaryIO, command: int, session_id: int, sequence: int, timeout_s: float) -> None:
    ack = read_packet(port, timeout_s)
    if ack.command != CMD_ACK:
        raise UpdateError(f"expected ACK, got command {ack.command}")
    if ack.session_id != session_id:
        raise UpdateError("ACK session mismatch")
    if ack.sequence != sequence:
        raise UpdateError("ACK sequence mismatch")
    if len(ack.payload) != 2:
        raise UpdateError("malformed ACK payload")

    acked_command, status = ack.payload
    if acked_command != command:
        raise UpdateError(f"ACK command mismatch: expected {command}, got {acked_command}")
    if status != STATUS_OK:
        raise UpdateError(f"bootloader returned status {status} for command {command}")


def command_name(command: int) -> str:
    return COMMAND_NAMES.get(command, f"COMMAND_{command}")


def send_and_ack(
    port: BinaryIO,
    packet: Packet,
    timeout_s: float,
    inter_byte_delay_s: float = 0.0,
    ui: UpdateUi | None = None,
) -> None:
    if ui is not None:
        ui.log(
            f"TX {command_name(packet.command)} seq={packet.sequence} "
            f"payload={len(packet.payload)}"
        )
    send_packet(port, packet, inter_byte_delay_s=inter_byte_delay_s)
    expect_ack(port, packet.command, packet.session_id, packet.sequence, timeout_s)
    if ui is not None:
        ui.log(f"ACK {command_name(packet.command)} seq={packet.sequence}")


def wait_for_bootloader(
    port: BinaryIO,
    session_id: int,
    timeout_s: float,
    probe_timeout_s: float,
    interval_s: float,
    inter_byte_delay_s: float,
    ui: UpdateUi | None = None,
) -> None:
    deadline = time.monotonic() + probe_timeout_s
    sequence = 0

    if ui is not None:
        ui.set_state("waiting for bootloader")
        ui.log("Waiting for bootloader update mode")

    while time.monotonic() < deadline:
        discover = Packet(CMD_DISCOVER, session_id=session_id, sequence=sequence)
        if ui is not None:
            ui.log(f"TX DISCOVER seq={sequence}")
        send_packet(port, discover, inter_byte_delay_s=inter_byte_delay_s)
        try:
            expect_ack(port, CMD_DISCOVER, session_id, sequence, timeout_s)
            if ui is not None:
                ui.log(f"ACK DISCOVER seq={sequence}")
                ui.set_state("bootloader connected")
            return
        except (TimeoutError, UpdateError):
            time.sleep(interval_s)
            sequence = (sequence + 1) & 0xFFFFFFFF

    raise TimeoutError("bootloader did not respond to DISCOVER")


def request_update_mode(
    port: BinaryIO,
    session_id: int,
    timeout_s: float,
    retries: int,
    interval_s: float,
    inter_byte_delay_s: float,
    ui: UpdateUi | None = None,
) -> bool:
    if ui is not None:
        ui.set_state("requesting app reset")

    for sequence in range(retries):
        packet = Packet(CMD_ENTER_UPDATE, session_id=session_id, sequence=sequence)
        if ui is not None:
            ui.log(f"TX ENTER_UPDATE seq={sequence}")
        send_packet(port, packet, inter_byte_delay_s=inter_byte_delay_s)
        try:
            expect_ack(port, CMD_ENTER_UPDATE, session_id, sequence, timeout_s)
            if ui is not None:
                ui.log(f"ACK ENTER_UPDATE seq={sequence}")
                ui.log("Device accepted update-mode request")
            return True
        except (TimeoutError, UpdateError):
            time.sleep(interval_s)

    if ui is not None:
        ui.log("No app update-mode ACK; continuing bootloader discovery")
    return False


def load_metadata(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def validate_artifacts(binary: bytes, metadata: dict, app_name: str | None, board_name: str | None) -> None:
    if metadata.get("image_size") != len(binary):
        raise UpdateError("metadata image_size does not match binary length")

    image_crc32 = image_manifest_crc32(binary)
    if metadata.get("image_crc32") != image_crc32:
        raise UpdateError("metadata image_crc32 does not match binary")

    if app_name is not None and metadata.get("application_name") != app_name:
        raise UpdateError("metadata application_name mismatch")

    if board_name is not None and metadata.get("board_name") != board_name:
        raise UpdateError("metadata board_name mismatch")


def run_update(
    port: BinaryIO,
    binary: bytes,
    metadata: dict,
    session_id: int,
    timeout_s: float,
    activate: bool,
    inter_byte_delay_s: float,
    ui: UpdateUi | None = None,
) -> None:
    if ui is not None:
        ui.set_state("begin")
        ui.set_progress(0, len(binary))

    begin_payload = struct.pack("<II", len(binary), metadata["image_crc32"])
    send_and_ack(
        port,
        Packet(CMD_BEGIN, session_id=session_id, payload=begin_payload),
        timeout_s,
        inter_byte_delay_s,
        ui,
    )

    if ui is not None:
        ui.set_state("streaming blocks")
    for offset in range(0, len(binary), MAX_PAYLOAD):
        block = binary[offset:offset + MAX_PAYLOAD]
        send_and_ack(
            port,
            Packet(CMD_BLOCK, session_id=session_id, sequence=offset, payload=block),
            timeout_s,
            inter_byte_delay_s,
            ui,
        )
        if ui is not None:
            ui.set_progress(offset + len(block), len(binary))

    if ui is not None:
        ui.set_state("ending transfer")
    send_and_ack(
        port, Packet(CMD_END, session_id=session_id), timeout_s,
        inter_byte_delay_s, ui
    )
    if ui is not None:
        ui.set_state("validating candidate")
    send_and_ack(
        port, Packet(CMD_VALIDATE, session_id=session_id), timeout_s,
        inter_byte_delay_s, ui
    )
    if activate:
        if ui is not None:
            ui.set_state("activating candidate")
        send_and_ack(
            port, Packet(CMD_ACTIVATE, session_id=session_id), timeout_s,
            inter_byte_delay_s, ui
        )
    if ui is not None:
        ui.set_state("complete")


def open_serial(device: str, baudrate: int, timeout_s: float) -> BinaryIO:
    try:
        import serial
    except ModuleNotFoundError as error:
        raise UpdateError("pyserial is required for UART updates: python3 -m pip install pyserial") from error

    return serial.Serial(device, baudrate=baudrate, timeout=timeout_s, write_timeout=timeout_s)


def main() -> int:
    parser = argparse.ArgumentParser(description="Send an inactive-slot firmware image over UART")
    parser.add_argument("--port", required=True, help="serial device, for example /dev/ttyACM0")
    parser.add_argument("--bin", required=True, type=Path, dest="binary")
    parser.add_argument("--metadata", type=Path, help="image JSON sidecar; defaults to .json next to --bin")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--session-id", type=lambda value: int(value, 0), default=0x55445731)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--probe-timeout", type=float, default=DEFAULT_PROBE_TIMEOUT_S)
    parser.add_argument("--probe-interval", type=float, default=DEFAULT_PROBE_INTERVAL_S)
    parser.add_argument("--byte-delay", type=float, default=0.001)
    parser.add_argument("--app-reset-byte-delay", type=float, default=0.002)
    parser.add_argument("--app-name")
    parser.add_argument("--board-name")
    parser.add_argument("--no-activate", action="store_true")
    parser.add_argument("--no-discover", action="store_true")
    parser.add_argument("--no-app-reset", action="store_true")
    parser.add_argument("--tui", action="store_true", help="force terminal UI")
    parser.add_argument("--no-tui", action="store_true", help="disable terminal UI")
    args = parser.parse_args()

    metadata_path = args.metadata if args.metadata is not None else args.binary.with_suffix(".json")

    try:
        binary = args.binary.read_bytes()
        metadata = load_metadata(metadata_path)
        validate_artifacts(binary, metadata, args.app_name, args.board_name)
        use_tui = args.tui or (not args.no_tui and sys.stdout.isatty())
        ui: UpdateUi
        ui = CursesUi("STM32 UART Firmware Update", metadata, args.port) if use_tui else PlainUi()
        with ui, open_serial(args.port, args.baud, args.timeout) as port:
            ui.log(f"Opened {args.port} at {args.baud} baud")
            ui.log(f"Loaded {args.binary} ({len(binary)} bytes)")
            reset_input = getattr(port, "reset_input_buffer", None)
            if reset_input is not None:
                reset_input()
            if not args.no_discover:
                if not args.no_app_reset:
                    request_update_mode(
                        port,
                        args.session_id,
                        args.timeout,
                        retries=5,
                        interval_s=args.probe_interval,
                        inter_byte_delay_s=args.app_reset_byte_delay,
                        ui=ui,
                    )
                wait_for_bootloader(
                    port,
                    args.session_id,
                    args.timeout,
                    args.probe_timeout,
                    args.probe_interval,
                    args.byte_delay,
                    ui,
                )
            run_update(
                port,
                binary,
                metadata,
                args.session_id,
                args.timeout,
                activate=not args.no_activate,
                inter_byte_delay_s=args.byte_delay,
                ui=ui,
            )
    except (OSError, TimeoutError, UpdateError, ValueError) as error:
        print(f"uart_update.py: {error}", file=sys.stderr)
        return 1

    if not args.no_activate:
        print("Update activated. Device reset requested.")
    else:
        print("Update transferred and validated without activation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
