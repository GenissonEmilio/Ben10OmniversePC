#!/usr/bin/env python3
"""Translate the disc firmware's ATRAC SPU codec at its observed LS residence.

Input is the user's own flashATRAC.pic from PS3 firmware 4.25. Generated code,
content signatures and firmware bytes stay local. This is not a codec stub.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys

from lift_jobs import SDK
from find_spu_functions import detect_functions

SHA256 = "1edf54e1323dd736b71958ca56307d536dedc7c05ccb0dd3b2a3be1af5b50e30"
DELTA = 0x33100  # observed branch 0x33190 matches the codec's entry bytes at 0x90


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pic", type=Path)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    blob = args.pic.read_bytes()
    if hashlib.sha256(blob).hexdigest() != SHA256:
        raise ValueError("flashATRAC.pic diferente do firmware 4.25 verificado")
    phoff = struct.unpack_from(">I", blob, 28)[0]
    phsize, phnum = struct.unpack_from(">HH", blob, 42)
    code = [struct.unpack_from(">8I", blob, phoff + i * phsize) for i in range(phnum)]
    code = [p for p in code if p[0] == 1 and p[6] & 1]
    if len(code) != 1 or code[0][2] != 0x80:
        raise ValueError("Segmento de código ATRAC inesperado")
    p = code[0]
    images = args.output / "images"
    lifted = args.output / "lifted"
    images.mkdir(parents=True, exist_ok=True)
    name = "fw425_atrac"
    functions, _ = detect_functions(blob)
    starts = ','.join(hex(start + DELTA) for start, _ in functions)
    subprocess.run([sys.executable, str(SDK / "rebase_spu_elf.py"), str(args.pic),
                    str(images / (name + ".elf")), hex(DELTA)], check=True)
    subprocess.run([sys.executable, str(SDK / "build_spu_workloads.py"),
                    "--images", str(images), "--lifted", str(lifted),
                    "--out", str(lifted / "spu_firmware.c"), "--relift",
                    "--extra-funcs", name + "=" + starts], check=True)
    signature = ','.join(f'0x{x:02X}' for x in blob[p[1]:p[1] + 16])
    registry = f'''/* Generated locally from the user's firmware; do not distribute. */
#include <stdint.h>
extern void spu_begin_image(int);
extern void spu_overlay_register_sig_region(const uint8_t*, uint32_t, int);
extern void {name}_spu_recomp_register(void);
__attribute__((constructor)) static void ben10_register_spu_firmware(void) {{
    static const uint8_t sig[16] = {{{signature}}};
    spu_begin_image(300); {name}_spu_recomp_register();
    spu_overlay_register_sig_region(sig, {p[4]}, 300);
    spu_begin_image(0);
}}
'''
    (lifted / "spu_firmware.c").write_text(registry)
    print("Codec ATRAC traduzido; validar execução e saída de áudio antes de empacotar.")


if __name__ == "__main__":
    main()
