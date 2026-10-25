#!/usr/bin/env python3
"""Reproduce the reviewed SDK patch series without overwriting unknown edits."""
from pathlib import Path, PurePosixPath
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "third_party/ps3recomp"
REVISION = "a6d76c895f77eb30b90a806c830544e61821b77f"


def apply_patchset(sdk, patches, revision):
    sdk = Path(sdk)
    patches = [Path(p).resolve() for p in patches]
    actual_revision = subprocess.check_output(
        ["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip()
    if actual_revision != revision:
        raise ValueError("Runtime diferente da revisão validada; patches não aplicados")
    paths = set()
    for patch in patches:
        old = [line[6:] for line in patch.read_text().splitlines() if line.startswith('--- a/')]
        new = [line[6:] for line in patch.read_text().splitlines() if line.startswith('+++ b/')]
        if not old or old != new:
            raise ValueError("Patch deve editar arquivos existentes sem renomear: " + patch.name)
        for name in new:
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError("Caminho de patch inválido")
            paths.add(name)
    with tempfile.TemporaryDirectory(prefix="ben10-runtime-patches-") as directory:
        scratch = Path(directory)
        subprocess.run(["git", "init", "-q", str(scratch)], check=True)
        known = {}
        for name in paths:
            content = subprocess.check_output(["git", "-C", str(sdk), "show", "HEAD:" + name])
            target = scratch / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            known[name] = {content}
        # Later fixes may edit an earlier patch's context. Reverse-checking
        # patches individually then misidentifies an already patched checkout.
        # Reconstruct the whole ordered series from the pin instead.
        for patch in patches:
            subprocess.run(["git", "-C", str(scratch), "apply", "--check", str(patch)], check=True)
            subprocess.run(["git", "-C", str(scratch), "apply", str(patch)], check=True)
            for name in paths:
                known[name].add((scratch / name).read_bytes())
        before = {}
        for name in paths:
            target = sdk / name
            if target.is_symlink():
                raise ValueError("Arquivo de runtime é um link simbólico: " + name)
            before[name] = target.read_bytes()
            if before[name] not in known[name]:
                raise ValueError("Alterações locais desconhecidas preservadas: " + name)
        changed = []
        # Validate every file before any write; atomic per-file replacement also
        # permits recovery from an interrupted update via the known states above.
        for name in sorted(paths):
            content = (scratch / name).read_bytes()
            if content == before[name]:
                continue
            target = sdk / name
            if target.read_bytes() != before[name]:
                raise ValueError("Runtime alterado durante a aplicação: " + name)
            with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as out:
                temporary = Path(out.name)
                out.write(content)
            try:
                temporary.chmod(target.stat().st_mode & 0o777)
                os.replace(temporary, target)
            finally:
                temporary.unlink(missing_ok=True)
            changed.append(name)
        return changed


def main():
    try:
        changed = apply_patchset(SDK, sorted((ROOT / "patches").glob("*.patch")), REVISION)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    for name in changed:
        print("Runtime:", name)


if __name__ == "__main__":
    main()
