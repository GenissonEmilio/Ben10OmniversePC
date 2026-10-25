#!/usr/bin/env python3
"""Verify and translate BLUS31017's four raw, position-independent SPURS jobs.

Addresses/fingerprints were surveyed by andrebrumdev/ben10-omniverse-recomp
(spu_jobs.toml). Every byte range is checked against this user's ELF before use.
No game bytes or translated functions belong in this repository.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
from types import SimpleNamespace

SDK = Path(__file__).resolve().parents[1] / "third_party/ps3recomp/tools"
sys.path.insert(0, str(SDK))
from wrap_spu_elf import wrap
from build_spu_workloads import fnv1a64, img_size, write_registry

ELF_SHA256 = "3ed23bca61cc9e392582e0c2cf0f114f24ce746f5724a2ab194e5aba944f1bea"
JOBS = ((0x844E80, 7888, 0xC144A6C46B720864),
        (0x84E500, 6480, 0xE7C9451016A3F0A8),
        (0x84B600, 6352, 0xE1845CEA62CEBD9E),
        (0x843080, 7632, 0x0EB2972DCBD8707D))


def extract_jobs(blob):
    if hashlib.sha256(blob).hexdigest() != ELF_SHA256:
        raise ValueError("ELF diferente de BLUS31017 01.00; endereços não serão reutilizados")
    phoff = struct.unpack_from(">Q", blob, 32)[0]
    phsize, phnum = struct.unpack_from(">HH", blob, 54)
    segments = [struct.unpack_from(">IIQQQQQQ", blob, phoff + i * phsize) for i in range(phnum)]
    jobs = []
    for ea, size, fingerprint in JOBS:
        loads = [p for p in segments if p[0] == 1 and p[3] <= ea and ea + size <= p[3] + p[5]]
        if len(loads) != 1:
            raise ValueError(f"Job fora de um único segmento: {ea:08X}")
        p = loads[0]; offset = p[2] + ea - p[3]
        code = blob[offset:offset + size]
        if len(code) != size or fnv1a64(code) != fingerprint:
            raise ValueError(f"Fingerprint de job inválido: {ea:08X}")
        jobs.append((f"job_{ea:08X}", code, fingerprint))
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()
    jobs = extract_jobs(args.elf.read_bytes())  # validate all before generating anything
    images = args.output / "images"; lifted = args.output / "lifted"
    images.mkdir(parents=True, exist_ok=True)
    for name, code, _ in jobs:
        # The public runtime stages PIC jobs at LS 0; lift at the same address
        # so their CRT computes its relocation delta and pointers consistently.
        (images / (name + ".elf")).write_bytes(wrap(code, base=0, entry=0))
    subprocess.run([sys.executable, str(SDK / "build_spu_workloads.py"),
                    "--images", str(images), "--lifted", str(lifted),
                    "--out", str(lifted / "spu_jobs.c"),
                    "--register-fn", "ben10_register_raw_jobs", "--title", "ben10-jobs"], check=True)
    registrations = []
    ids = {}
    text_fps = {}
    for i, (name, _, fp) in enumerate(jobs):
        b = (images / (name + ".elf")).read_bytes()
        registrations.append((name, name + "_", fnv1a64(b[:img_size(b)]), 0))
        ids[name] = 100 + i  # distinct from the 22 embedded ELF dispatch tables
        text_fps[name] = fp
    write_registry(SimpleNamespace(out=str(lifted / "spu_jobs.c"), title="ben10-jobs",
                                   register_fn="ben10_register_raw_jobs", constructor=True),
                   registrations, text_fps, image_ids=ids)
    print("Quatro jobs verificados e traduzidos; gameplay ainda requer validação.")


if __name__ == "__main__":
    main()
