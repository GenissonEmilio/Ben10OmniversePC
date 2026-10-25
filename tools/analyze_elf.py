#!/usr/bin/env python3
"""Analyze a user's decrypted PPU ELF; never run it or apply another game's addresses."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from ps3_metadata import DiscError, PROJECT, executable_kind


def validate_elf(path):
    size = path.stat().st_size
    if size > 512 * 1024 * 1024:
        raise DiscError("ELF maior que 512 MiB; análise cancelada")
    with path.open("rb") as stream:
        header = stream.read(64)
        info = executable_kind(header)
        if info["needs_plain_elf"]:
            raise DiscError("Forneça EBOOT.ELF já descriptografado, não EBOOT.BIN/SELF")
        phoff, shoff = struct.unpack_from(">QQ", header, 32)
        ehsize, phsize, phnum, shsize, shnum, _ = struct.unpack_from(">6H", header, 52)
        if ehsize != 64 or phsize != 56 or not 0 < phnum <= 4096 or phoff < 64:
            raise DiscError("Cabeçalho ELF/segmentos inválido")
        if phoff + phsize * phnum > size or (shnum and (shsize != 64 or shoff < 64 or shoff + shsize * shnum > size)):
            raise DiscError("Tabela ELF truncada")
        stream.seek(phoff)
        segments = []
        for _ in range(phnum):
            kind, flags, offset, address, _, filesz, memsz, _ = struct.unpack(">IIQQQQQQ", stream.read(56))
            if offset + filesz > size or (kind == 1 and (filesz > memsz or address + memsz > 0x100000000)):
                raise DiscError("Segmento ELF truncado ou fora do espaço de endereços PS3")
            if kind == 1:
                segments.append({"address": hex(address), "filesz": filesz, "memsz": memsz, "executable": bool(flags & 1)})
        if not any(s["executable"] for s in segments):
            raise DiscError("ELF sem segmento executável")
    digest = sha256(path)
    return {**info, "sha256": digest, "size": size, "segments": segments}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def analyze(elf, output):
    elf = elf.expanduser().resolve()
    info = validate_elf(elf)
    # Each input fingerprint has its own output: stale addresses from another
    # revision must never be reused just because their filenames are the same.
    folder = output.resolve() / info["sha256"]
    if folder.exists():
        raise DiscError(f"Análise já existe em {folder}; preserve-a ou escolha outra saída")
    folder.mkdir(parents=True)
    sdk = PROJECT / "third_party/ps3recomp/tools"
    jobs = [("functions", [str(sdk / "find_functions.py"), str(elf), "-o", str(folder / "functions.json")]),
            ("imports", [str(sdk / "gen_imports.py"), str(elf), "-o", str(folder / "imports.json")]),
            ("spu", [str(sdk / "extract_spu_images.py"), str(elf), "-o", str(folder / "spu")])]
    report = {"schema": 1, "input": str(elf), "elf": info, "steps": {}, "playable_validated": False}
    try:
        for name, args in jobs:
            with (folder / f"{name}.log").open("w") as log:
                done = subprocess.run([sys.executable, *args], stdout=log, stderr=subprocess.STDOUT, timeout=600)
            report["steps"][name] = done.returncode
            # No embedded SPU ELF is a valid finding, not a successful lift.
            if done.returncode and not (name == "spu" and done.returncode == 1 and "No SPU images found." in (folder / "spu.log").read_text()):
                raise DiscError(f"Falha em {name}; consulte {folder / (name + '.log')}")
        report["stage"] = "static_analysis_only"
        print(f"Análise concluída: {folder}")
        print("Próxima etapa: conferir TOC, limites de funções e jobs SPU desta versão antes de recompilar.")
        return folder
    finally:
        (folder / "analysis.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--output", type=Path, default=PROJECT / "work/analysis")
    args = parser.parse_args()
    try:
        analyze(args.elf, args.output)
    except (DiscError, OSError, struct.error, subprocess.SubprocessError) as error:
        print(f"Análise não concluída: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
