"""Synthetic PPC switch whose table base is cached on the worker's stack."""
from pathlib import Path
import struct
import json
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "third_party/ps3recomp/tools"))
from ppu_lifter import disassemble_bytes, discover_jump_tables


class StackTableTest(unittest.TestCase):
    def test_spilled_toc_table(self):
        # lwz r9,0(r2); std r9,184(r1); cmplwi r30,5;
        # ld r11,184(r1); rlwinm r9,r30,2,0,29; lwzx r0,r9,r11;
        # extsw r0,r0; add r11,r0,r11; mtctr r11; bctr
        words = [0x81220000, 0xF92100B8, 0x281E0005, 0xE96100B8,
                 0x57C9103A, 0x7C09582E, 0x7C0007B4, 0x7D605A14,
                 0x7D6903A6, 0x4E800420]
        code = b''.join(struct.pack('>I', word) for word in words)
        table = 0x1800
        memory = {0x4000: table, **{table + i * 4: 0x100 + i * 4 for i in range(6)}}
        result = discover_jump_tables(disassemble_bytes(code, 0x1000, True),
                                      memory.get, [0x4000], 0x1000, 0x2000)
        self.assertEqual(result[0x1024], [0x1900 + i * 4 for i in range(6)])

    def test_embedded_foreign_code_is_not_lifted_as_ppu(self):
        sdk = Path(__file__).resolve().parents[1] / "third_party/ps3recomp/tools"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Two PPU functions surround data that deliberately looks like a
            # function prologue. It must not create a PPU function or body.
            words = [0x38600007, 0x4E800020, 0x7C0802A6, 0xF821FF81,
                     0x38600055, 0x4E800020, 0x38600009, 0x4E800020]
            source = root / "image.bin"
            source.write_bytes(b''.join(struct.pack('>I', word) for word in words))
            functions = root / "functions.json"
            functions.write_text(json.dumps([dict(start=hex(a), end=hex(b)) for a, b in
                [(0x1000, 0x1020), (0x1008, 0x1018), (0x1018, 0x1020)]]))
            ranges = root / "data.json"
            ranges.write_text('[{"start":"0x1008","end":"0x1018"}]')
            subprocess.run([sys.executable, str(sdk / "ppu_lifter.py"), str(source),
                            "--raw", "--base", "0x1000", "--code-end", "0x1020",
                            "--functions", str(functions), "--data-ranges", str(ranges),
                            "--output", str(root / "out"), "--jobs", "1"],
                           check=True, stdout=subprocess.DEVNULL, timeout=20)
            output = '\n'.join(p.read_text() for p in (root / "out").glob('*.cpp'))
            self.assertIn('void func_00001000(', output)
            self.assertIn('void func_00001018(', output)
            self.assertNotIn('func_00001008', output)
            self.assertNotIn('int32_t)(0x55)', output)
