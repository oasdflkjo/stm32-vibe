import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.decode_trace import FrameDecoder, ItmDecoder, crc8, format_record, load_events
from tools.extract_trace_map import build_map, parse_format


def make_frame(event_id: int, *args: int) -> bytes:
    body = bytearray(
        [
            1,
            event_id & 0xFF,
            event_id >> 8,
            len(args),
        ]
    )
    for value in args:
        body.extend(struct.pack("<I", value & 0xFFFFFFFF))
    return b"\xA5\x5A" + bytes(body) + bytes([crc8(body)])


class TraceMapTests(unittest.TestCase):
    def test_parses_supported_argument_types(self) -> None:
        self.assertEqual(
            parse_format("signed=%d hex=%08X char=%c %%", "test"),
            ["i32", "u32", "char"],
        )

    def test_rejects_unsupported_string_argument(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported format"):
            parse_format("text=%s", "test")

    def test_rejects_format_and_call_argument_mismatch(self) -> None:
        format_bytes = b"value=%u\0"
        symbols = [(0, len(format_bytes), 0, "trace_format_0_0")]

        with (
            patch("tools.extract_trace_map.read_symbols", return_value=symbols),
            patch(
                "tools.extract_trace_map.read_trace_section",
                return_value=format_bytes,
            ),
        ):
            with self.assertRaisesRegex(ValueError, "format expects 1 arguments"):
                build_map(Path("firmware.elf"), "nm", "objcopy")

    def test_applies_image_id_namespace(self) -> None:
        format_bytes = b"boot\0"
        symbols = [(0, len(format_bytes), 0, "trace_format_0_0")]

        with (
            patch("tools.extract_trace_map.read_symbols", return_value=symbols),
            patch(
                "tools.extract_trace_map.read_trace_section",
                return_value=format_bytes,
            ),
        ):
            trace_map = build_map(
                Path("bootloader.elf"),
                "nm",
                "objcopy",
                id_base=0x8000,
                image="bootloader",
            )

        self.assertEqual(trace_map["image"], "bootloader")
        self.assertEqual(trace_map["events"]["32768"]["format"], "boot")

    def test_loads_non_overlapping_maps(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            app_map = directory_path / "app.json"
            boot_map = directory_path / "boot.json"
            app_map.write_text(
                json.dumps({"events": {"0": {"format": "app", "arg_types": []}}}),
                encoding="utf-8",
            )
            boot_map.write_text(
                json.dumps(
                    {"events": {"32768": {"format": "boot", "arg_types": []}}}
                ),
                encoding="utf-8",
            )

            events = load_events([boot_map, app_map])

        self.assertEqual(events["0"]["format"], "app")
        self.assertEqual(events["32768"]["format"], "boot")


class TraceDecoderTests(unittest.TestCase):
    def test_decodes_itm_port_zero_and_trace_frame(self) -> None:
        frame = make_frame(0x1234, 7, 0xFFFFFFFF)
        itm_stream = b"".join(bytes([0x01, byte]) for byte in frame)

        trace_bytes = ItmDecoder().feed(itm_stream)
        records = FrameDecoder().feed(trace_bytes)

        self.assertEqual(records, [(0x1234, [7, 0xFFFFFFFF])])

    def test_resynchronizes_after_bad_crc(self) -> None:
        bad_frame = bytearray(make_frame(0x1111, 1))
        bad_frame[-1] ^= 0xFF
        good_frame = make_frame(0x2222, 2)

        records = FrameDecoder().feed(bytes(bad_frame) + good_frame)

        self.assertEqual(records, [(0x2222, [2])])

    def test_formats_signed_and_hex_arguments(self) -> None:
        event = {
            "format": "signed=%d hex=%08X",
            "arg_types": ["i32", "u32"],
        }

        output = format_record(event, [0xFFFFFFFF, 0x2A])

        self.assertEqual(output, "signed=-1 hex=0000002A")


if __name__ == "__main__":
    unittest.main()
