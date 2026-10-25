#!/usr/bin/env python3
"""Apply the reviewed runtime fixes to the pinned SDK, idempotently."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "third_party/ps3recomp"
REVISION = "a6d76c895f77eb30b90a806c830544e61821b77f"


def main():
    revision = subprocess.check_output(["git", "-C", str(SDK), "rev-parse", "HEAD"], text=True).strip()
    if revision != REVISION:
        raise SystemExit("Runtime diferente da revisão validada; não aplicaremos patches automaticamente")
    for patch in sorted((ROOT / "patches").glob("*.patch")):
        command = ["git", "-C", str(SDK), "apply"]
        applied = subprocess.run([*command, "--reverse", "--check", str(patch)], capture_output=True)
        if applied.returncode == 0:
            continue
        subprocess.run([*command, "--check", str(patch)], check=True)
        subprocess.run([*command, str(patch)], check=True)
        print("Runtime:", patch.name)


if __name__ == "__main__":
    main()
