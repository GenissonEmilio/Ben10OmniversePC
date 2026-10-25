#!/usr/bin/env python3
"""Lift verified embedded SPU plugins at their observed runtime LS addresses.

The title copies these PIC images into its heap and DMA-loads their executable
segments at LS 0x30080. Content signatures identify them independently of heap
addresses. All generated code/signatures stay in the user's local data folder.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys

from lift_jobs import ELF_SHA256, SDK
from find_spu_functions import detect_functions


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", type=Path)
    ap.add_argument("--images", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    original = args.elf.read_bytes()
    if hashlib.sha256(original).hexdigest() != ELF_SHA256:
        raise ValueError("ELF diferente da versão verificada")
    plugins = []
    for path in sorted(args.images.glob("*.elf")):
        b = path.read_bytes()
        if b not in original:
            raise ValueError(f"Imagem SPU não corresponde ao ELF: {path.name}")
        entry, phoff = struct.unpack_from(">II", b, 24)
        if entry == 0x3050:
            continue  # the resident task at 0x3000 is translated separately
        phsize, phnum = struct.unpack_from(">HH", b, 42)
        code = [struct.unpack_from(">8I", b, phoff + i * phsize) for i in range(phnum)]
        code = [p for p in code if p[0] == 1 and p[6] & 1]
        if (len(code) != 1 or code[0][2] != 0x80 or code[0][4] < 512
                or not code[0][2] <= entry < code[0][2] + code[0][4]):
            raise ValueError(f"Layout de plugin SPU desconhecido: {path.name}")
        p = code[0]
        plugins.append(("ovl_" + path.stem, path, b[p[1]:p[1] + 16]))
    if len(plugins) != 21 or len({sig for _, _, sig in plugins}) != len(plugins):
        raise ValueError("Inventário de plugins incompleto ou assinaturas ambíguas")
    images = args.output / "images"; lifted = args.output / "lifted"
    images.mkdir(parents=True, exist_ok=True)
    for name, source, _ in plugins:
        subprocess.run([sys.executable, str(SDK / "rebase_spu_elf.py"), str(source),
                        str(images / (name + ".elf")), "0x30000"], check=True)
    extra = []
    for name, source, _ in plugins:
        # PIC instructions still materialize link-time addresses. Discover
        # exports/tables before rebasing as well: otherwise those constants
        # look outside the shifted text and valid indirect entries disappear.
        functions, _ = detect_functions(source.read_bytes())
        starts = ','.join(hex(start + 0x30000) for start, _ in functions)
        extra.extend(["--extra-funcs", name + "=" + starts])
    subprocess.run([sys.executable, str(SDK / "build_spu_workloads.py"),
                    "--images", str(images), "--lifted", str(lifted),
                    "--out", str(lifted / "spu_overlays.c"),
                    "--register-fn", "ben10_register_spu_overlays", "--title", "ben10-overlays",
                    "--relift", *extra], check=True)
    lines = ['/* Generated locally from verified game data; do not distribute. */',
             '#include <stdint.h>', 'extern void spu_begin_image(int);',
             'extern void spu_overlay_register_sig(const uint8_t*, int);']
    for name, _, _ in plugins:
        lines.append(f'extern void {name}_spu_recomp_register(void);')
    lines.append('__attribute__((constructor)) static void ben10_register_spu_overlays(void) {')
    for i, (name, _, signature) in enumerate(plugins, 200):
        values = ','.join(f'0x{v:02X}' for v in signature)
        lines.extend([f'    static const uint8_t sig_{i}[16] = {{{values}}};',
                      f'    spu_begin_image({i}); {name}_spu_recomp_register();',
                      f'    spu_overlay_register_sig(sig_{i}, {i});'])
    lines.extend(['    spu_begin_image(0);', '}'])
    (lifted / "spu_overlays.c").write_text('\n'.join(lines) + '\n')
    print("21 plugins SPU traduzidos para LS 0x30080; validar execução antes de empacotar.")


if __name__ == "__main__":
    main()
