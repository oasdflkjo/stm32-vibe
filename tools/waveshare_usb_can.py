#!/usr/bin/env python3

import argparse
import dataclasses
import time
from collections import deque
from collections.abc import Iterable


SERIAL_BAUDRATE = 2_000_000
MAX_CAN_DATA_LEN = 8

BITRATE_CODES = {
    1_000_000: 0x01,
    800_000: 0x02,
    500_000: 0x03,
    400_000: 0x04,
    250_000: 0x05,
    200_000: 0x06,
    125_000: 0x07,
    100_000: 0x08,
    50_000: 0x09,
    20_000: 0x0A,
    10_000: 0x0B,
    5_000: 0x0C,
}

MODE_NORMAL = 0x00
MODE_SILENT = 0x01
MODE_LOOPBACK = 0x02
MODE_SILENT_LOOPBACK = 0x03


@dataclasses.dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: bytes
    extended: bool = False
    remote: bool = False


def encode_settings(bitrate: int, mode: int = MODE_NORMAL) -> bytes:
    try:
        bitrate_code = BITRATE_CODES[bitrate]
    except KeyError as error:
        raise ValueError(f"unsupported CAN bitrate: {bitrate}") from error
    if mode not in {MODE_NORMAL, MODE_LOOPBACK, MODE_SILENT,
                    MODE_SILENT_LOOPBACK}:
        raise ValueError(f"unsupported CAN mode: {mode}")

    frame = bytearray([0xAA, 0x55, 0x12, bitrate_code, 0x01])
    frame.extend(b"\x00" * 8)  # filter ID and mask ID: accept all
    frame.extend([mode, 0x00, 0x00, 0x00, 0x00, 0x00])
    frame.append(sum(frame[2:19]) & 0xFF)
    return bytes(frame)


def encode_frame(frame: CanFrame) -> bytes:
    if len(frame.data) > MAX_CAN_DATA_LEN:
        raise ValueError("CAN payload exceeds 8 bytes")
    max_id = 0x1FFFFFFF if frame.extended else 0x7FF
    if frame.can_id < 0 or frame.can_id > max_id:
        raise ValueError("CAN ID is out of range")

    frame_type = 0xC0 | len(frame.data)
    if frame.extended:
        frame_type |= 0x20
    if frame.remote:
        frame_type |= 0x10
    id_size = 4 if frame.extended else 2
    return (bytes([0xAA, frame_type]) +
            frame.can_id.to_bytes(id_size, "little") + frame.data + b"\x55")


class FrameParser:
    def __init__(self) -> None:
        self._buffer = bytearray()

    def feed(self, data: bytes) -> list[CanFrame]:
        self._buffer.extend(data)
        frames: list[CanFrame] = []
        while self._buffer:
            if self._buffer[0] != 0xAA:
                del self._buffer[0]
                continue
            if len(self._buffer) < 2:
                break
            frame_type = self._buffer[1]
            if frame_type == 0x55:
                if len(self._buffer) < 20:
                    break
                del self._buffer[:20]
                continue
            if (frame_type & 0xC0) != 0xC0:
                del self._buffer[0]
                continue
            dlc = frame_type & 0x0F
            if dlc > MAX_CAN_DATA_LEN:
                del self._buffer[0]
                continue
            extended = (frame_type & 0x20) != 0
            id_size = 4 if extended else 2
            frame_len = 2 + id_size + dlc + 1
            if len(self._buffer) < frame_len:
                break
            candidate = bytes(self._buffer[:frame_len])
            del self._buffer[:frame_len]
            if candidate[-1] != 0x55:
                continue
            frames.append(CanFrame(
                can_id=int.from_bytes(candidate[2:2 + id_size], "little"),
                data=candidate[2 + id_size:-1],
                extended=extended,
                remote=(frame_type & 0x10) != 0,
            ))
        return frames


class UsbCanA:
    def __init__(self, port: str, bitrate: int = 500_000,
                 mode: int = MODE_NORMAL) -> None:
        try:
            import serial
        except ImportError as error:
            raise RuntimeError("pyserial is required for USB-CAN-A") from error

        self._serial = serial.Serial(port, SERIAL_BAUDRATE, timeout=0.02)
        self._parser = FrameParser()
        self._received: deque[CanFrame] = deque()
        self._serial.reset_input_buffer()
        self._serial.write(encode_settings(bitrate, mode))
        self._serial.flush()
        time.sleep(0.05)
        self._serial.reset_input_buffer()

    def close(self) -> None:
        self._serial.close()

    def __enter__(self) -> "UsbCanA":
        return self

    def __exit__(self, exc_type: object, exc_value: object,
                 traceback: object) -> None:
        self.close()

    def send(self, frame: CanFrame) -> None:
        self._serial.write(encode_frame(frame))
        self._serial.flush()

    def reset_input_buffer(self) -> None:
        self._received.clear()
        self._parser = FrameParser()
        self._serial.reset_input_buffer()

    def receive(self, timeout: float) -> CanFrame | None:
        deadline = time.monotonic() + timeout
        if self._received:
            return self._received.popleft()
        while time.monotonic() < deadline:
            self._received.extend(self._parser.feed(self._serial.read(64)))
            if self._received:
                return self._received.popleft()
        return None


def parse_data(value: str) -> bytes:
    compact = value.replace(" ", "").replace(":", "")
    try:
        data = bytes.fromhex(compact)
    except ValueError as error:
        raise argparse.ArgumentTypeError("data must be hexadecimal bytes") from error
    if len(data) > MAX_CAN_DATA_LEN:
        raise argparse.ArgumentTypeError("CAN payload exceeds 8 bytes")
    return data


def print_frames(frames: Iterable[CanFrame]) -> None:
    for frame in frames:
        print(f"{frame.can_id:08X}  [{len(frame.data)}]  " +
              frame.data.hex(" ").upper())


def main() -> int:
    parser = argparse.ArgumentParser(description="Waveshare USB-CAN-A utility")
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--bitrate", type=int, default=500_000,
                        choices=BITRATE_CODES)
    parser.add_argument("--send-id", type=lambda value: int(value, 0))
    parser.add_argument("--data", type=parse_data, default=b"")
    parser.add_argument("--count", type=int, default=0,
                        help="frames to receive; 0 listens until interrupted")
    parser.add_argument("--timeout", type=float, default=1.0)
    args = parser.parse_args()

    with UsbCanA(args.port, args.bitrate) as adapter:
        if args.send_id is not None:
            adapter.send(CanFrame(args.send_id, args.data))
        received = 0
        while args.count == 0 or received < args.count:
            frame = adapter.receive(args.timeout)
            if frame is None:
                if args.count != 0:
                    return 1
                continue
            print_frames([frame])
            received += 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
