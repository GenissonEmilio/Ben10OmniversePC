#!/usr/bin/env python3
"""Lift OVIS code banks individually, without flattening overlapping LS ranges.

Inputs and all generated code remain in the user's local firmware workspace.
The original ELF is retained for workload fingerprinting and guest DMA reads.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

from lift_jobs import SDK
from find_spu_functions import detect_functions, parse_elf, text_image

LS_SIZE = 0x40000


def code_banks(blob):
    elf = parse_elf(blob)
    offset = struct.unpack_from('>I', blob, 0x1C)[0]
    stride, count = struct.unpack_from('>HH', blob, 0x2A)
    if stride < 32 or offset + count * stride > len(blob):
        raise ValueError('Invalid SPU program header table')
    common, overlays = [], []
    for i, ph in enumerate(elf['phs']):
        if ph['type'] != 1 or not ph['flags'] & 1:
            continue
        physical = struct.unpack_from('>I', blob, offset + i * stride + 12)[0]
        if not ph['filesz'] or ph['off'] + ph['filesz'] > len(blob):
            raise ValueError('Invalid executable segment file range')
        if ph['vaddr'] + ph['memsz'] > LS_SIZE:
            raise ValueError('Executable segment exceeds local store')
        (overlays if physical >= LS_SIZE else common).append(i)
    return elf, common, overlays


def bank_translation(blob, selected):
    """Return text and discovered bounds restricted to the selected PT_LOADs."""
    elf = parse_elf(blob)
    offset = struct.unpack_from('>I', blob, 0x1C)[0]
    stride = struct.unpack_from('>H', blob, 0x2A)[0]
    filtered = bytearray(blob)
    intervals = []
    for i, ph in enumerate(elf['phs']):
        if i in selected:
            intervals.append((ph['vaddr'], ph['vaddr'] + ph['filesz']))
        else:
            struct.pack_into('>I', filtered, offset + i * stride + 24, ph['flags'] & ~1)
    if not intervals:
        raise ValueError('Empty executable bank')
    functions, _ = detect_functions(bytes(filtered), verbose=False)
    # The detector spans gaps between executable segments. Those bytes do not
    # belong to this bank, including branch targets in another OVIS variant.
    bounds = [(start, min(end, hi)) for start, end in functions
              for lo, hi in intervals if lo <= start < hi]
    text, base = text_image(bytes(filtered), parse_elf(filtered)['phs'])
    return text, base, bounds


def lift_bank(blob, selected, name, output):
    text, base, bounds = bank_translation(blob, selected)
    output.mkdir(parents=True, exist_ok=True)
    code, functions = output / 'bank.code', output / 'bank.functions.json'
    code.write_bytes(text)
    functions.write_text(json.dumps([dict(start=s, end=e) for s, e in bounds]))
    subprocess.run([sys.executable, str(SDK / 'spu_lifter.py'), str(code),
                    '--base', hex(base), '--functions', str(functions),
                    '--symbol-prefix', name + '_', '--output', str(output)], check=True)
    for file in ('spu_recomp.h', 'spu_recomp.c'):
        path = output / file
        path.write_text(re.sub(r'#include\s+"(?:\.\./)+runtime/spu/([A-Za-z0-9_]+\.h)"',
                               r'#include "\1"', path.read_text()))


def lift_variants(workspace):
    inventory = json.loads((workspace / 'inventory.json').read_text())
    lifted = workspace / 'lifted'
    variants = []
    for item in inventory:
        name = item['name']
        blob = (workspace / 'images' / (name + '.elf')).read_bytes()
        elf, common, overlays = code_banks(blob)
        if not overlays:
            continue
        if not common:
            raise ValueError('Overlay ELF has no resident code: ' + name)
        lift_bank(blob, common, name, lifted / name)
        for index in overlays:
            ph = elf['phs'][index]
            variant = name + '_bank_' + str(index)
            lift_bank(blob, [index], variant, lifted / variant)
            signature = blob[ph['off']:ph['off'] + 16]
            if ph['filesz'] < 512 or len(signature) != 16:
                raise ValueError('Overlay too small for content registration')
            variants.append((variant, ph['filesz'], signature))
    if len({v[2] for v in variants}) != len(variants):
        raise ValueError('Ambiguous overlay signatures')
    lines = ['/* Generated locally. Do not distribute firmware-derived code. */',
             '#include <stdint.h>', 'extern void spu_begin_image(int);',
             'extern void spu_overlay_register_sig_region(const uint8_t*,uint32_t,int);']
    lines += [f'extern void {name}_spu_recomp_register(void);' for name, _, _ in variants]
    lines.append('__attribute__((constructor)) static void ben10_register_codec_variants(void) {')
    for image_id, (name, size, sig) in enumerate(variants, 500):
        values = ','.join(str(v) for v in sig)
        lines += [f'  static const uint8_t sig_{image_id}[16] = {{{values}}};',
                  f'  spu_begin_image({image_id}); {name}_spu_recomp_register();',
                  f'  spu_overlay_register_sig_region(sig_{image_id}, {size}, {image_id});']
    lines += ['  spu_begin_image(0);', '}']
    (lifted / 'spu_codec_variants.c').write_text('\n'.join(lines) + '\n')
    print(f'{len(variants)} OVIS code variants lifted separately')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workspace', type=Path)
    lift_variants(parser.parse_args().workspace)
