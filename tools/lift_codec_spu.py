#!/usr/bin/env python3
"""Translate SPU programs embedded in the locally relocated media firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

from lift_jobs import SDK
from lift_firmware_ppu import MODULES
from extract_spu_images import find_spu_images
from build_spu_workloads import fnv1a64, img_size, text_fp, be32, write_registry


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("firmware_ppu", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()
    images = args.output / "images"
    lifted = args.output / "lifted"
    inventory, seen = [], set()
    images.mkdir(parents=True, exist_ok=True)
    for module, _, _ in MODULES:
        image = args.firmware_ppu / module / (module + "_image.bin")
        blob = image.read_bytes()
        for offset, size in find_spu_images(blob):
            data = blob[offset:offset + size]
            digest = hashlib.sha256(data).hexdigest()
            if digest in seen: continue
            seen.add(digest)
            name = f"codec_{module}_{offset:08X}"
            (images / (name + ".elf")).write_bytes(data)
            inventory.append(dict(name=name, module=module, offset=offset, size=size, sha256=digest))
    if not inventory: raise ValueError("Nenhum programa SPU de codec encontrado")
    subprocess.run([sys.executable, str(SDK / "build_spu_workloads.py"),
                    "--images", str(images), "--lifted", str(lifted),
                    "--out", str(lifted / "spu_codecs.c"), "--relift",
                    "--register-fn", "ben10_register_codec_spu", "--title", "ben10-media-codecs"], check=True)
    registrations, ids, fingerprints = [], {}, {}
    for i, item in enumerate(inventory, 400):
        name = item["name"]
        blob = (images / (name + ".elf")).read_bytes()
        registrations.append((name, name + "_", fnv1a64(blob[:img_size(blob)]), be32(blob, 24)))
        ids[name] = i
        fingerprints[name] = text_fp(blob)
    write_registry(SimpleNamespace(out=str(lifted / "spu_codecs.c"), title="ben10-media-codecs",
                                   register_fn="ben10_register_codec_spu", constructor=True),
                   registrations, fingerprints, image_ids=ids)
    (args.output / "inventory.json").write_text(json.dumps(inventory, indent=2))
    print(f"{len(inventory)} programas SPU de codec traduzidos. Validar reprodução antes de publicar.")


if __name__ == "__main__":
    main()
