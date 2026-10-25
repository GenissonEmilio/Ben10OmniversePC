#!/usr/bin/env python3
"""Import BLUS31017 locally. Never execute, upload or modify the source disc."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).parent / "desktop"))
from iso9660 import scan
from ps3_metadata import parse_sfo

TITLE_ID = "BLUS31017"
EBOOT_SHA256 = "c2bfdf35a4477a8117725a5cf2e621ef22bece85f1b135794162331a788683a1"
DATA = Path.home() / ".local/share/Ben10OmniversePC"


def read_entry(stream, entries, name, maximum):
    matches = [(lba, size) for path, lba, size in entries if path == name]
    if len(matches) != 1 or not 0 < matches[0][1] <= maximum:
        raise ValueError(f"Arquivo obrigatório inválido: {name}")
    lba, size = matches[0]
    stream.seek(lba * 2048)
    data = stream.read(size)
    if len(data) != size:
        raise ValueError(f"Arquivo truncado: {name}")
    return data


def import_iso(iso, destination, progress=print):
    iso = Path(iso).expanduser().resolve()
    destination = Path(destination).expanduser().absolute()
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise ValueError("O destino já existe; os dados atuais foram preservados")
    with iso.open("rb") as stream:
        files, layer1 = scan(stream)
        if layer1:
            raise ValueError("A imagem não corresponde ao disco PS3 de camada única esperado")
        sfo = parse_sfo(read_entry(stream, files, "PS3_GAME/PARAM.SFO", 1024 * 1024))
        if sfo.get("TITLE_ID") != TITLE_ID or sfo.get("APP_VER") != "01.00" or sfo.get("CATEGORY") != "DG":
            raise ValueError("Use Ben 10 Omniverse USA BLUS31017 versão 1.00 para PS3")
        eboot = read_entry(stream, files, "PS3_GAME/USRDIR/EBOOT.BIN", 64 * 1024 * 1024)
        digest = hashlib.sha256(eboot).hexdigest()
        if digest != EBOOT_SHA256:
            raise ValueError("Executável diferente do verificado; não reutilizaremos endereços de outra edição")
        wanted = [f for f in files if f[0].startswith("PS3_GAME/") or f[0] == "PS3_DISC.SFB"]
        total = sum(size for _, _, size in wanted)
        if shutil.disk_usage(destination.parent).free < total + 1024 ** 3:
            raise ValueError("Espaço livre insuficiente para importar o disco")
        temporary = Path(tempfile.mkdtemp(prefix=".ben10-import-", dir=destination.parent))
        try:
            copied = 0
            for index, (name, lba, size) in enumerate(wanted):
                target = temporary / name
                if not target.resolve().is_relative_to(temporary.resolve()):
                    raise ValueError("Caminho de disco inseguro")
                target.parent.mkdir(parents=True, exist_ok=True)
                stream.seek(lba * 2048)
                remaining = size
                with target.open("xb") as out:
                    while remaining:
                        data = stream.read(min(remaining, 4 * 1024 * 1024))
                        if not data:
                            raise ValueError(f"Disco truncado ao importar {name}")
                        out.write(data)
                        remaining -= len(data)
                        copied += len(data)
                if index % 400 == 0:
                    progress(f"Importando: {copied * 100 // max(total, 1)}% ({index + 1}/{len(wanted)} arquivos)")
            report = {"title_id": TITLE_ID, "app_version": "01.00", "platform": "PS3",
                      "eboot_sha256": digest, "files": len(wanted), "bytes": total,
                      "stage": "disc_imported", "native_gameplay_validated": False}
            (temporary / "import.json").write_text(json.dumps(report, indent=2) + "\n")
            temporary.rename(destination)
            return report
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path)
    parser.add_argument("--destination", type=Path, default=DATA / "game")
    args = parser.parse_args()
    try:
        print(json.dumps(import_iso(args.iso, args.destination), indent=2))
    except (ValueError, OSError) as error:
        print(f"Importação não concluída: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
