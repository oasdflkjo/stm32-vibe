import unittest

from tools.waveshare_usb_can import (
    CanFrame,
    FrameParser,
    MODE_LOOPBACK,
    encode_frame,
    encode_settings,
    parse_data,
)


class WaveshareUsbCanTest(unittest.TestCase):
    def test_encodes_documented_standard_frame(self) -> None:
        frame = CanFrame(0x123, bytes.fromhex("11 22 33 44 55 66 77 88"))
        self.assertEqual(
            encode_frame(frame),
            bytes.fromhex("AA C8 23 01 11 22 33 44 55 66 77 88 55"),
        )

    def test_encodes_settings_with_checksum(self) -> None:
        settings = encode_settings(500_000, MODE_LOOPBACK)
        self.assertEqual(len(settings), 20)
        self.assertEqual(settings[:5], bytes.fromhex("AA 55 12 03 01"))
        self.assertEqual(settings[13], MODE_LOOPBACK)
        self.assertEqual(settings[14], 0x00)
        self.assertEqual(settings[-1], sum(settings[2:19]) & 0xFF)

    def test_parser_handles_noise_and_split_frames(self) -> None:
        expected = CanFrame(0x321, bytes.fromhex("CA FE"))
        encoded = encode_frame(expected)
        parser = FrameParser()
        self.assertEqual(parser.feed(b"noise" + encoded[:3]), [])
        self.assertEqual(parser.feed(encoded[3:]), [expected])

    def test_parser_handles_extended_frame(self) -> None:
        expected = CanFrame(0x1234567, b"abc", extended=True)
        parser = FrameParser()
        self.assertEqual(parser.feed(encode_frame(expected)), [expected])

    def test_rejects_oversized_payload(self) -> None:
        with self.assertRaises(ValueError):
            encode_frame(CanFrame(0x123, b"123456789"))

    def test_parses_cli_hex_data(self) -> None:
        self.assertEqual(parse_data("CA:FE 12"), bytes.fromhex("CA FE 12"))


if __name__ == "__main__":
    unittest.main()
