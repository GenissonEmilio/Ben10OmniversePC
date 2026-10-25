"""PRX variable relocations, using synthetic bytes and no firmware."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from lift_firmware_ppu import apply_variable_refs


class VariableReferences(unittest.TestCase):
    def test_width_addend_and_adjusted_high_half(self):
        image = bytearray(256)
        base = 0x28000000
        for i, (kind, target, addend) in enumerate([(1, 128, 4), (4, 136, 0),
                                                  (6, 138, 0), (57, 140, 0)]):
            struct.pack_into('>III', image, i * 12, kind, base + target, addend)
        struct.pack_into('>H', image, 140, 3)
        apply_variable_refs(image, base, base, 0x28518004)
        self.assertEqual(struct.unpack_from('>I', image, 128)[0], 0x28518008)
        self.assertEqual(struct.unpack_from('>3H', image, 136), (0x8004, 0x2852, 0x8007))

    def test_rejects_unknown_type_and_out_of_image_target(self):
        image = bytearray(64)
        struct.pack_into('>III', image, 0, 99, 32, 0)
        with self.assertRaises(ValueError): apply_variable_refs(image, 0, 0, 1)
        struct.pack_into('>III', image, 0, 1, 63, 0)
        with self.assertRaises(ValueError): apply_variable_refs(image, 0, 0, 1)
