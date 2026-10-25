#!/usr/bin/env python3
"""Inspect a local PS3 game directory/ISO without running game code or decrypting it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import threading

PROJECT = Path(__file__).resolve().parents[1]
MAX_SFO = 1024 * 1024


class DiscError(ValueError):
    pass


def parse_sfo(data):
    if len(data) < 20 or len(data) > MAX_SFO or data[:4] != b"\x00PSF":
        raise DiscError("PARAM.SFO ausente, truncado ou com formato inválido")
    version, keys, values, count = struct.unpack_from("<4I", data, 4)
    table_end = 20 + count * 16
    if version != 0x101 or count > 4096 or not table_end <= keys <= values <= len(data):
        raise DiscError("Tabelas de PARAM.SFO inválidas")
    result = {}
    for i in range(count):
        key_offset, kind, size, capacity, value_offset = struct.unpack_from("<HHIII", data, 20 + i * 16)
        start = keys + key_offset
        if not keys <= start < values:
            raise DiscError("Chave fora de PARAM.SFO")
        end = data.find(b"\0", start, values)
        if end < 0:
            raise DiscError("Chave de PARAM.SFO sem terminador")
        key = data[start:end].decode("utf-8", errors="strict")
        start = values + value_offset
        if size > capacity or start < values or start + capacity > len(data):
            raise DiscError("Valor fora de PARAM.SFO")
        if key in result:
            raise DiscError("Chave duplicada em PARAM.SFO")
        raw = data[start:start + size]
        if kind == 0x0204:
            result[key] = raw.rstrip(b"\0").decode("utf-8", errors="strict")
        elif kind == 0x0404 and size == 4:
            result[key] = struct.unpack("<I", raw)[0]
    return result


def executable_kind(header):
    if header[:4] == b"SCE\0":
        if len(header) < 64:
            raise DiscError("Cabeçalho SELF truncado")
        return {"format": "PS3_SELF", "needs_plain_elf": True}
    if len(header) < 64 or header[:7] != b"\x7fELF\x02\x02\x01":
        raise DiscError("EBOOT não é um SELF de PS3 nem um ELF64 big-endian")
    kind, machine = struct.unpack_from(">HH", header, 16)
    if machine != 21 or kind != 2:
        raise DiscError("O ELF precisa ser um executável PPU PowerPC64 de PS3")
    return {"format": "PPU_ELF64_BE", "needs_plain_elf": False,
            "entry": hex(struct.unpack_from(">Q", header, 24)[0])}


