"""A synthetic SPU ELF has one constructor absent from the direct call graph."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[1] / 'third_party/ps3recomp/tools'
sys.path.insert(0, str(SDK))
from find_spu_functions import collect_init_array_targets, detect_functions
from wrap_spu_elf import _pack_ehdr, _pack_phdr, _pack_shdr


class ConstructorTest(unittest.TestCase):
    def test_single_constructor_is_discovered_and_translated(self):
        elf = bytearray(0x2A0)
        elf[:52] = _pack_ehdr(0x3000, 52, 0x200, 2, 4, 3)
        elf[52:84] = _pack_phdr(0x100, 0x3000, 0x20, 5)
        elf[84:116] = _pack_phdr(0x140, 0x4000, 12, 6)
        # il r3,7; bi r0. The isolated constructor returns 99.
        struct.pack_into('>II', elf, 0x100, 0x40800383, 0x35000000)
        struct.pack_into('>II', elf, 0x110, 0x40803183, 0x35000000)
        struct.pack_into('>III', elf, 0x140, 0xFFFFFFFF, 0x3010, 0)
        names = b'\0.text\0.ctors\0.shstrtab\0'
        elf[0x160:0x160+len(names)] = names
        elf[0x228:0x250] = _pack_shdr(1, 1, 6, 0x3000, 0x100, 0x20)
        elf[0x250:0x278] = _pack_shdr(7, 1, 3, 0x4000, 0x140, 12)
        elf[0x278:0x2A0] = _pack_shdr(14, 3, 0, 0, 0x160, len(names))
        funcs, _ = detect_functions(elf, verbose=False)
        self.assertIn(0x3010, [a for a, _ in funcs])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / 'synthetic.elf';source.write_bytes(elf)
            subprocess.run([sys.executable, str(SDK / 'spu_lifter.py'), str(source),
                            '--auto-functions', str(source), '-o', str(root / 'out')],
                           check=True, stdout=subprocess.DEVNULL, timeout=20)
            output = (root / 'out/spu_recomp.c').read_text()
            self.assertIn('void spu_func_00003010(', output)
            self.assertIn('spu_il(99)', output)

    def test_arrays_reject_bounds_and_ignore_sentinels_and_non_code(self):
        words = struct.pack('>6I', 0, 0xFFFFFFFF, 0x3010, 0x3011, 0x9000, 0x3020)
        for name in ['.ctors', '.dtors', '.init_array', '.fini_array', '.preinit_array']:
            section = dict(name=name, off=0, size=len(words))
            self.assertEqual(collect_init_array_targets(words, [section], {0x3010: object()}), {0x3010})
        with self.assertRaises(ValueError):
            collect_init_array_targets(words, [dict(name='.ctors', off=0, size=100)], {})
