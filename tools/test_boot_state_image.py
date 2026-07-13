import struct
import unittest
import zlib

from tools import boot_state_image


class BootStateImageTest(unittest.TestCase):
    def unpack_record(self, image):
        record = image[:boot_state_image.BOOT_STATE_SIZE]
        return struct.unpack("<" + ("I" * boot_state_image.BOOT_STATE_WORDS),
                             record)

    def test_confirmed_a_image_contains_valid_record(self):
        image = boot_state_image.image_for_mode("confirmed-a", 0x1000)
        words = self.unpack_record(image)
        crc_record = bytearray(image[:boot_state_image.BOOT_STATE_SIZE])
        struct.pack_into("<I", crc_record,
                         boot_state_image.BOOT_STATE_CRC_WORD * 4, 0)

        self.assertEqual(0x1000, len(image))
        self.assertEqual(boot_state_image.BOOT_STATE_MAGIC, words[0])
        self.assertEqual(boot_state_image.BOOT_STATE_VERSION, words[1])
        self.assertEqual(boot_state_image.BOOT_STATE_SIZE, words[2])
        self.assertEqual(boot_state_image.BOOT_SLOT_A, words[4])
        self.assertEqual(boot_state_image.BOOT_STATE_NO_SLOT, words[5])
        self.assertEqual(boot_state_image.BOOT_SLOT_STATUS_CONFIRMED, words[7])
        self.assertEqual(boot_state_image.BOOT_SLOT_STATUS_EMPTY, words[8])
        self.assertEqual(zlib.crc32(crc_record) & 0xFFFFFFFF, words[11])
        self.assertEqual(b"\xff" * boot_state_image.BOOT_STATE_SIZE,
                         image[boot_state_image.BOOT_STATE_COPY_B_OFFSET:
                               boot_state_image.BOOT_STATE_COPY_B_OFFSET +
                               boot_state_image.BOOT_STATE_SIZE])

    def test_empty_image_marks_both_slots_empty(self):
        image = boot_state_image.image_for_mode("empty", 0x1000)
        words = self.unpack_record(image)

        self.assertEqual(boot_state_image.BOOT_SLOT_A, words[4])
        self.assertEqual(boot_state_image.BOOT_SLOT_STATUS_EMPTY, words[7])
        self.assertEqual(boot_state_image.BOOT_SLOT_STATUS_EMPTY, words[8])

    def test_rejects_too_small_image(self):
        with self.assertRaisesRegex(ValueError, "too small"):
            boot_state_image.image_for_mode("confirmed-a", 128)


if __name__ == "__main__":
    unittest.main()
