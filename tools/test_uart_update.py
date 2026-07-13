import json
import tempfile
import unittest
from pathlib import Path

from tools import uart_update
from tools.finalize_image import finalize_bytes


class RecordingUi(uart_update.UpdateUi):
    def __init__(self):
        super().__init__()
        self.states = []
        self.progress = []

    def set_state(self, state):
        self.states.append(state)
        super().set_state(state)

    def set_progress(self, current, total):
        self.progress.append((current, total))
        super().set_progress(current, total)


class FakeSerial:
    def __init__(self):
        self.written = bytearray()
        self.rx = bytearray()

    def write(self, data):
        self.written.extend(data)
        return len(data)

    def read(self, size=1):
        if not self.rx:
            return b""
        chunk = self.rx[:size]
        del self.rx[:size]
        return bytes(chunk)

    def flush(self):
        pass

    def reset_input_buffer(self):
        self.rx.clear()

    def push_ack(self, command, session_id, sequence, status=uart_update.STATUS_OK):
        self.rx.extend(
            uart_update.encode_packet(
                uart_update.Packet(
                    uart_update.CMD_ACK,
                    session_id=session_id,
                    sequence=sequence,
                    payload=bytes([command, status]),
                )
            )
        )


def decode_written_packets(fake):
    packets = []
    data = bytes(fake.written)
    offset = 0
    while offset < len(data):
        payload_len = int.from_bytes(data[offset + 6:offset + 8], "little")
        packet_len = uart_update.HEADER_SIZE + payload_len + uart_update.CRC_SIZE
        packets.append(uart_update.decode_packet(data[offset:offset + packet_len]))
        offset += packet_len
    return packets


class UartUpdateTests(unittest.TestCase):
    def test_packet_round_trip(self):
        packet = uart_update.Packet(
            command=uart_update.CMD_BLOCK,
            session_id=0x11223344,
            sequence=128,
            payload=b"abc",
        )

        decoded = uart_update.decode_packet(uart_update.encode_packet(packet))

        self.assertEqual(decoded, packet)

    def test_expect_ack_rejects_bootloader_error(self):
        fake = FakeSerial()
        fake.push_ack(uart_update.CMD_BEGIN, 1, 0, status=uart_update.STATUS_OK + 1)

        with self.assertRaisesRegex(uart_update.UpdateError, "status"):
            uart_update.expect_ack(fake, uart_update.CMD_BEGIN, 1, 0, 0.01)

    def test_validate_artifacts_checks_crc_and_names(self):
        binary, _manifest, metadata = finalize_bytes(
            bytes([0xFF]) * 1024,
            version=1,
            app_name="vibe",
            board_name="ST NUCLEO-L152RE",
        )
        self.assertNotEqual(metadata["image_crc32"], uart_update.crc32(binary))

        uart_update.validate_artifacts(
            binary, metadata, "vibe", "ST NUCLEO-L152RE"
        )

        with self.assertRaisesRegex(uart_update.UpdateError, "board_name"):
            uart_update.validate_artifacts(binary, metadata, "vibe", "other")

    def test_validate_artifacts_rejects_bad_manifest_crc(self):
        binary, _manifest, metadata = finalize_bytes(
            bytes([0xFF]) * 1024,
            version=1,
            app_name="vibe",
            board_name="ST NUCLEO-L152RE",
        )
        corrupt = bytearray(binary)
        corrupt[32] ^= 0x01
        binary = bytes(corrupt)
        metadata = {
            "image_size": len(binary),
            "image_crc32": metadata["image_crc32"],
            "application_name": "vibe",
            "board_name": "ST NUCLEO-L152RE",
        }

        with self.assertRaisesRegex(uart_update.UpdateError, "image_crc32"):
            uart_update.validate_artifacts(binary, metadata, "vibe", "ST NUCLEO-L152RE")

    def test_run_update_sends_begin_blocks_end_validate_activate(self):
        fake = FakeSerial()
        binary = bytes(range(uart_update.MAX_PAYLOAD + 3))
        metadata = {
            "image_size": len(binary),
            "image_crc32": uart_update.crc32(binary),
        }
        session_id = 0xABCDEF01

        fake.push_ack(uart_update.CMD_BEGIN, session_id, 0)
        fake.push_ack(uart_update.CMD_BLOCK, session_id, 0)
        fake.push_ack(uart_update.CMD_BLOCK, session_id, uart_update.MAX_PAYLOAD)
        fake.push_ack(uart_update.CMD_END, session_id, 0)
        fake.push_ack(uart_update.CMD_VALIDATE, session_id, 0)
        fake.push_ack(uart_update.CMD_ACTIVATE, session_id, 0)

        ui = RecordingUi()
        uart_update.run_update(
            fake, binary, metadata, session_id, 0.01, activate=True,
            inter_byte_delay_s=0.0, ui=ui
        )

        packets = decode_written_packets(fake)
        self.assertEqual(
            [packet.command for packet in packets],
            [
                uart_update.CMD_BEGIN,
                uart_update.CMD_BLOCK,
                uart_update.CMD_BLOCK,
                uart_update.CMD_END,
                uart_update.CMD_VALIDATE,
                uart_update.CMD_ACTIVATE,
            ],
        )
        self.assertEqual(packets[1].sequence, 0)
        self.assertEqual(packets[1].payload, binary[:uart_update.MAX_PAYLOAD])
        self.assertEqual(packets[2].sequence, uart_update.MAX_PAYLOAD)
        self.assertEqual(packets[2].payload, binary[uart_update.MAX_PAYLOAD:])
        self.assertIn("streaming blocks", ui.states)
        self.assertEqual(ui.progress[-1], (len(binary), len(binary)))
        self.assertTrue(any("ACK ACTIVATE" in line for line in ui.logs))

    def test_wait_for_bootloader_retries_discover_until_ack(self):
        fake = FakeSerial()
        session_id = 0x1234
        fake.push_ack(uart_update.CMD_DISCOVER, session_id + 1, 0)
        fake.push_ack(uart_update.CMD_DISCOVER, session_id, 1)

        ui = RecordingUi()
        uart_update.wait_for_bootloader(
            fake, session_id, 0.001, 0.1, 0.001, 0.0, ui=ui
        )

        packets = decode_written_packets(fake)
        self.assertEqual([packet.command for packet in packets], [uart_update.CMD_DISCOVER, uart_update.CMD_DISCOVER])
        self.assertEqual(packets[0].sequence, 0)
        self.assertEqual(packets[1].sequence, 1)
        self.assertIn("bootloader connected", ui.states)

    def test_request_update_mode_sends_enter_update(self):
        fake = FakeSerial()
        session_id = 0x5555
        fake.push_ack(uart_update.CMD_ENTER_UPDATE, session_id, 0)

        ui = RecordingUi()
        requested = uart_update.request_update_mode(
            fake, session_id, 0.001, retries=3, interval_s=0.001,
            inter_byte_delay_s=0.0, ui=ui
        )

        self.assertTrue(requested)
        packets = decode_written_packets(fake)
        self.assertEqual(len(packets), 1)
        self.assertEqual(packets[0].command, uart_update.CMD_ENTER_UPDATE)
        self.assertIn("requesting app reset", ui.states)
        self.assertTrue(any("ACK ENTER_UPDATE" in line for line in ui.logs))


if __name__ == "__main__":
    unittest.main()
