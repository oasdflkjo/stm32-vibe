import unittest

from tools.can_update import PacketReassembler, fragment_packet


class CanUpdateTransportTest(unittest.TestCase):
    def test_round_trip_maximum_packet(self) -> None:
        packet = bytes(range(148))
        frames = fragment_packet(packet, 0x601)
        reassembler = PacketReassembler(0x601)
        decoded = None
        for frame in frames:
            decoded = reassembler.feed(frame) or decoded
        self.assertEqual(len(frames), 22)
        self.assertTrue(all(len(frame.data) == 8 for frame in frames))
        self.assertEqual(decoded, packet)

    def test_reassembler_ignores_other_id(self) -> None:
        packet = bytes(range(20))
        reassembler = PacketReassembler(0x681)
        for frame in fragment_packet(packet, 0x682):
            self.assertIsNone(reassembler.feed(frame))

    def test_reassembler_rejects_missing_fragment(self) -> None:
        packet = bytes(range(40))
        frames = fragment_packet(packet, 0x681)
        reassembler = PacketReassembler(0x681)
        self.assertIsNone(reassembler.feed(frames[0]))
        self.assertIsNone(reassembler.feed(frames[2]))
        for frame in frames[3:]:
            self.assertIsNone(reassembler.feed(frame))


if __name__ == "__main__":
    unittest.main()
