import unittest

from tools.finalize_image import (
    MANIFEST_APP_ID_WORD,
    MANIFEST_BOARD_ID_WORD,
    MANIFEST_FORMAT,
    MANIFEST_OFFSET,
    MANIFEST_VECTOR_WORDS_WORD,
    MANIFEST_GOT_OFFSET_WORD,
    finalize_bytes,
    metadata_id,
)
import struct


class FinalizeImageTests(unittest.TestCase):
    def test_finalize_bytes_stamps_metadata_ids(self):
        image = bytes([0xFF]) * 1024

        patched, manifest, metadata = finalize_bytes(
            image, 3, "vibe", "ST NUCLEO-L152RE"
        )

        fields = struct.unpack(MANIFEST_FORMAT, manifest)
        reserved = fields[8:]
        self.assertEqual(metadata["application_name"], "vibe")
        self.assertEqual(metadata["board_name"], "ST NUCLEO-L152RE")
        self.assertEqual(reserved[MANIFEST_APP_ID_WORD], metadata_id("vibe"))
        self.assertEqual(
            reserved[MANIFEST_BOARD_ID_WORD], metadata_id("ST NUCLEO-L152RE")
        )
        self.assertEqual(fields[6], metadata_id("ST NUCLEO-L152RE"))
        self.assertEqual(
            patched[MANIFEST_OFFSET:MANIFEST_OFFSET + len(manifest)], manifest
        )
        self.assertEqual(metadata["image_crc32"], fields[4])

    def test_finalize_bytes_stamps_relocation_metadata(self):
        image = bytes([0xFF]) * 1024
        relocation = {
            "vector_words": 78,
            "got_offset": 700,
            "got_size": 24,
            "data_load_offset": 724,
        }
        _, manifest, metadata = finalize_bytes(
            image, 3, "vibe", "board", relocation
        )
        reserved = struct.unpack(MANIFEST_FORMAT, manifest)[8:]
        self.assertEqual(reserved[MANIFEST_VECTOR_WORDS_WORD], 78)
        self.assertEqual(reserved[MANIFEST_GOT_OFFSET_WORD], 700)
        self.assertEqual(metadata["got_size"], 24)


if __name__ == "__main__":
    unittest.main()
