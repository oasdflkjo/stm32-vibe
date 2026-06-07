import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.decode_trace import FrameDecoder, ItmDecoder, crc8, format_record
from tools.gen_trace import collect_events, render_header, render_map


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


class TraceGeneratorTests(unittest.TestCase):
    def test_generates_id_macro_without_format_string(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "main.c"
            source.write_text(
                'void f(void) { TRACE(SENSOR_VALUE, "sensor=%u", 42U); }\n',
                encoding="utf-8",
            )

            events = collect_events([source])
            header = render_header(events)
            trace_map = json.loads(render_map(events))

            self.assertIn("TRACE_ID_SENSOR_VALUE", header)
            self.assertIn("trace_emit1", header)
            self.assertNotIn("sensor=%u", header)
            self.assertEqual(
                next(iter(trace_map["events"].values()))["format"],
                "sensor=%u",
            )

    def test_rejects_unsupported_string_argument(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "main.c"
            source.write_text(
                'void f(void) { TRACE(TEXT, "text=%s", "hello"); }\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "unsupported format"):
                collect_events([source])


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
