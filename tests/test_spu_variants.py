"""Synthetic overlays share LS addresses, while their code stays distinct."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from lift_spu_variants import bank_translation, code_banks, lift_bank
from wrap_spu_elf import _pack_ehdr
from test_spurs_pending import production_function


def synthetic_banks():
    data = bytearray(0xA00)
    data[:52] = _pack_ehdr(0x5C00, 52, 0, 3, 0, 0)
    for i, (offset, virtual, physical, size) in enumerate([
            (0x100, 0x5C00, 0x5C00, 16),
            (0x200, 0x3080, 0x40000, 0x200),
            (0x400, 0x3080, 0x40200, 0x200)]):
        struct.pack_into('>8I', data, 52 + i * 32,
                         1, offset, virtual, physical, size, size, 5, 16)
    # Common code calls shared address 3080; banks return different constants.
    displacement = ((0x3080 - 0x5C00) // 4) & 0xFFFF
    struct.pack_into('>II', data, 0x100, (0x66 << 23) | (displacement << 7), 0x35000000)
    struct.pack_into('>II', data, 0x200, 0x40800383, 0x35000000)  # r3=7
    struct.pack_into('>II', data, 0x400, 0x40803183, 0x35000000)  # r3=99
    return bytes(data)


class OverlayBanks(unittest.TestCase):
    def test_translation_keeps_banks_and_resident_calls_separate(self):
        blob = synthetic_banks()
        _, common, overlays = code_banks(blob)
        self.assertEqual(common, [0]); self.assertEqual(overlays, [1, 2])
        text_a, base_a, bounds_a = bank_translation(blob, [1])
        text_b, base_b, bounds_b = bank_translation(blob, [2])
        self.assertEqual(base_a, 0x3080); self.assertEqual(base_b, base_a)
        self.assertNotEqual(text_a, text_b)
        self.assertEqual(bounds_a, bounds_b)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            lift_bank(blob, common, 'resident', output)
            source = (output / 'spu_recomp.c').read_text()
            # Shared bank entry is a dispatch stub, never a stale bank body.
            self.assertIn('ctx->pc = 0x3080u;', source)
            self.assertIn('g_spu_trampoline_fn = spu_indirect_branch', source)
            self.assertNotIn('spu_il(99)', source)
            self.assertNotIn('spu_il(7)', source)
            self.assertNotIn('{ 0x00003080u,', source)

    def test_production_loader_leaves_overlays_for_dma(self):
        source = (ROOT / 'third_party/ps3recomp/runtime/spu/spu_workload.c').read_text()
        functions = '\n'.join(production_function(source, name)
                              for name in ['rd_be16', 'rd_be32', 'spu_elf_load_to_ls'])
        fixture = '''#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define SPU_LS_SIZE 0x40000
/* FUNCTIONS */
int main(int argc,char**argv) {
  uint8_t image[0xA00],ls[SPU_LS_SIZE];uint32_t entry=0;
  FILE*f=fopen(argv[1],"rb");assert(f && fread(image,1,sizeof image,f)==sizeof image);fclose(f);
  memset(ls,0xAB,sizeof ls);
  assert(spu_elf_load_to_ls(image,sizeof image,ls,&entry));
  assert(entry==0x5C00 && !memcmp(ls+0x5C00,image+0x100,16));
  for(unsigned i=0x3080;i<0x3280;++i)assert(ls[i]==0xAB);
  // File bounds are checked even for unloaded overlay payloads.
  assert(!spu_elf_load_to_ls(image,0x500,ls,&entry));
  puts("PASS");
}'''.replace('/* FUNCTIONS */', functions)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); c = root / 'test.c'; elf = root / 'test.elf'; binary = root / 'test'
            c.write_text(fixture); elf.write_bytes(synthetic_banks())
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-UNDEBUG',
                            str(c), '-o', str(binary)], check=True, capture_output=True, timeout=20)
            result = subprocess.run([str(binary), str(elf)], check=True,
                                    capture_output=True, text=True, timeout=4)
            self.assertIn('PASS', result.stdout)

    def test_segment_bounds_reject_invalid_input(self):
        data = bytearray(synthetic_banks())
        struct.pack_into('>I', data, 52 + 32 + 4, len(data) - 1)
        with self.assertRaises(ValueError): code_banks(data)
