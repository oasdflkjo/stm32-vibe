#!/usr/bin/env python3

import argparse
import sys
import time
from pathlib import Path

try:
    from tools.uart_update import (
        CursesUi,
        DEFAULT_PROBE_INTERVAL_S,
        DEFAULT_PROBE_TIMEOUT_S,
        PlainUi,
        UpdateError,
        UpdateUi,
        load_metadata,
        request_update_mode,
        run_update,
        validate_artifacts,
        wait_for_bootloader,
    )
    from tools.waveshare_usb_can import CanFrame, UsbCanA
except ModuleNotFoundError:
    from uart_update import (
        CursesUi,
        DEFAULT_PROBE_INTERVAL_S,
        DEFAULT_PROBE_TIMEOUT_S,
        PlainUi,
        UpdateError,
        UpdateUi,
        load_metadata,
        request_update_mode,
        run_update,
        validate_artifacts,
        wait_for_bootloader,
    )
    from waveshare_usb_can import CanFrame, UsbCanA


REQUEST_BASE_ID = 0x600
RESPONSE_BASE_ID = 0x680
FRAGMENT_START = 0x80
FRAGMENT_END = 0x40
FRAGMENT_INDEX_MASK = 0x3F
MIN_PACKET_SIZE = 20
MAX_PACKET_SIZE = 148
DEFAULT_FRAGMENT_DELAY_S = 0.01


def fragment_packet(packet: bytes, can_id: int) -> list[CanFrame]:
    if len(packet) < MIN_PACKET_SIZE or len(packet) > MAX_PACKET_SIZE:
        raise ValueError("update packet length is out of range")
    frames: list[CanFrame] = []
    offset = 0
    index = 0
    while offset < len(packet):
        if offset == 0:
            header = bytes([
                FRAGMENT_START,
                len(packet) & 0xFF,
                len(packet) >> 8,
            ])
        else:
            header = bytes([index & FRAGMENT_INDEX_MASK])
        chunk = packet[offset:offset + 8 - len(header)]
        offset += len(chunk)
        control = header[0]
        if offset == len(packet):
            control |= FRAGMENT_END
        data = bytes([control]) + header[1:] + chunk
        frames.append(CanFrame(can_id, data.ljust(8, b"\x00")))
        index += 1
    return frames


class PacketReassembler:
    def __init__(self, can_id: int) -> None:
        self.can_id = can_id
        self.reset()

    def reset(self) -> None:
        self.expected_len = 0
        self.next_index = 0
        self.packet = bytearray()
        self.active = False

    def feed(self, frame: CanFrame) -> bytes | None:
        if frame.can_id != self.can_id or not frame.data:
            return None
        control = frame.data[0]
        index = control & FRAGMENT_INDEX_MASK
        if control & FRAGMENT_START:
            if index != 0 or len(frame.data) < 3:
                self.reset()
                return None
            expected_len = frame.data[1] | (frame.data[2] << 8)
            if expected_len < MIN_PACKET_SIZE or expected_len > MAX_PACKET_SIZE:
                self.reset()
                return None
            self.expected_len = expected_len
            self.next_index = 1
            self.packet = bytearray(frame.data[3:])
            self.active = True
        else:
            if not self.active or index != self.next_index:
                self.reset()
                return None
            self.next_index += 1
            self.packet.extend(frame.data[1:])
        if len(self.packet) > self.expected_len:
            padding = self.packet[self.expected_len:]
            if not (control & FRAGMENT_END) or any(padding):
                self.reset()
                return None
            del self.packet[self.expected_len:]
        if control & FRAGMENT_END:
            if len(self.packet) != self.expected_len:
                self.reset()
                return None
            packet = bytes(self.packet)
            self.reset()
            return packet
        if len(self.packet) == self.expected_len:
            self.reset()
        return None


class CanPacketPort:
    def __init__(self, adapter: UsbCanA, node_id: int,
                 fragment_delay_s: float = DEFAULT_FRAGMENT_DELAY_S) -> None:
        if node_id < 0 or node_id > 0x7F:
            raise ValueError("CAN node ID must be between 0 and 127")
        self.adapter = adapter
        self.request_id = REQUEST_BASE_ID + node_id
        self.reassembler = PacketReassembler(RESPONSE_BASE_ID + node_id)
        self.fragment_delay_s = fragment_delay_s
        self.read_buffer = bytearray()

    def write(self, packet: bytes) -> int:
        for frame in fragment_packet(packet, self.request_id):
            self.adapter.send(frame)
            if self.fragment_delay_s > 0:
                time.sleep(self.fragment_delay_s)
        return len(packet)

    def flush(self) -> None:
        pass

    def reset_input_buffer(self) -> None:
        self.read_buffer.clear()
        self.reassembler.reset()
        self.adapter.reset_input_buffer()

    def read(self, size: int) -> bytes:
        if not self.read_buffer:
            frame = self.adapter.receive(0.02)
            if frame is not None:
                packet = self.reassembler.feed(frame)
                if packet is not None:
                    self.read_buffer.extend(packet)
        data = bytes(self.read_buffer[:size])
        del self.read_buffer[:size]
        return data


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send an inactive-slot firmware image over CAN")
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--bitrate", type=int, default=500_000)
    parser.add_argument("--node-id", type=lambda value: int(value, 0),
                        default=1)
    parser.add_argument("--bin", required=True, type=Path, dest="binary")
    parser.add_argument("--metadata", type=Path)
    parser.add_argument("--session-id", type=lambda value: int(value, 0),
                        default=0x55444331)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--probe-timeout", type=float,
                        default=DEFAULT_PROBE_TIMEOUT_S)
    parser.add_argument("--probe-interval", type=float,
                        default=DEFAULT_PROBE_INTERVAL_S)
    parser.add_argument("--fragment-delay", type=float,
                        default=DEFAULT_FRAGMENT_DELAY_S)
    parser.add_argument("--app-name")
    parser.add_argument("--board-name")
    parser.add_argument("--no-activate", action="store_true")
    parser.add_argument("--no-discover", action="store_true")
    parser.add_argument("--no-app-reset", action="store_true")
    parser.add_argument("--tui", action="store_true")
    parser.add_argument("--no-tui", action="store_true")
    args = parser.parse_args()

    metadata_path = (args.metadata if args.metadata is not None
                     else args.binary.with_suffix(".json"))
    try:
        binary = args.binary.read_bytes()
        metadata = load_metadata(metadata_path)
        validate_artifacts(binary, metadata, args.app_name, args.board_name)
        use_tui = args.tui or (not args.no_tui and sys.stdout.isatty())
        ui: UpdateUi = (
            CursesUi("STM32 CAN Firmware Update", metadata, args.port)
            if use_tui else PlainUi()
        )
        with ui, UsbCanA(args.port, args.bitrate) as adapter:
            port = CanPacketPort(adapter, args.node_id, args.fragment_delay)
            ui.log(f"Opened {args.port} at CAN {args.bitrate} bit/s")
            ui.log(f"Using node {args.node_id}, request ID 0x{port.request_id:03X}")
            ui.log(f"Loaded {args.binary} ({len(binary)} bytes)")
            port.reset_input_buffer()
            if not args.no_discover:
                if not args.no_app_reset:
                    request_update_mode(
                        port, args.session_id, args.timeout, retries=5,
                        interval_s=args.probe_interval,
                        inter_byte_delay_s=0.0, ui=ui)
                wait_for_bootloader(
                    port, args.session_id, args.timeout,
                    args.probe_timeout, args.probe_interval, 0.0, ui)
            run_update(
                port, binary, metadata, args.session_id, args.timeout,
                activate=not args.no_activate, inter_byte_delay_s=0.0,
                ui=ui)
    except (OSError, TimeoutError, UpdateError, ValueError, RuntimeError) as error:
        print(f"can_update.py: {error}", file=sys.stderr)
        return 1

    if args.no_activate:
        print("CAN update transferred and validated without activation.")
    else:
        print("CAN update activated. Device reset requested.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
