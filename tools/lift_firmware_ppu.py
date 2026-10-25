#!/usr/bin/env python3
"""Build local PPU lifts and binding metadata for the user's media firmware.

This experimental path is opt-in through BEN10_LLE_MEDIA=1. It does not make
missing decoder dependencies complete. No firmware data is written to Git.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

from lift_jobs import SDK
from lift_prx import Prx
from extract_spu_images import find_spu_images

MODULES = (("libpamf", 0x28100000, 0x0012),
           ("libdmux", 0x28200000, 0x0007),
           ("libdmuxpamf", 0x28300000, 0x002A),
           ("libadec", 0x28400000, 0x0006),
           ("libatxdec", 0x28500000, 0x0028),
           ("libvdec", 0x28600000, 0x0005),
           ("libavcdec", 0x28700000, 0x0025),
           ("libvpost", 0x28A00000, 0x0008),
           ("libapostsrc_mini", 0x28B00000, 0),
           ("libsail", 0x28000000, 0x001D))


def variable_tables(prx, image, base):
    """PRX exports append variables after functions; imports have VNID/VREF tables."""
    def read(address, fmt):
        offset = address - base
        if offset < 0 or offset + struct.calcsize(fmt) > len(image):
            raise ValueError("Tabela PRX fora da imagem")
        return struct.unpack_from(fmt, image, offset)[0]
    def u32(address): return read(address, ">I")
    exports, imports = {}, []
    mi = prx.modinfo_off
    for importing in (False, True):
        begin, end = struct.unpack_from(">II", prx.data, mi + (0x2C if importing else 0x24))
        cursor = base + begin
        while cursor < base + end:
            size = read(cursor, ">B") or (0x2C if importing else 0x1C)
            nfunc, nvar = read(cursor + 6, ">H"), read(cursor + 8, ">H")
            name_ptr = u32(cursor + 16)
            if not name_ptr:
                # Nameless PRX management exports include module-info data;
                # they are not variable imports/exports of a linkable library.
                cursor += size; continue
            name_off = name_ptr - base
            if not 0 <= name_off < len(image):
                if nvar: raise ValueError("Biblioteca de variáveis PRX sem nome")
                cursor += size; continue
            name = image[name_off:].split(b'\0', 1)[0].decode('ascii')
            if importing:
                nids, values = u32(cursor + 28), u32(cursor + 32)
            else:
                nids, values = u32(cursor + 20) + nfunc * 4, u32(cursor + 24) + nfunc * 4
            for i in range(nvar):
                key = (name, u32(nids + 4 * i))
                value = u32(values + 4 * i)
                if importing: imports.append((key, value))
                else: exports[key] = value
            cursor += size
    return exports, imports


def apply_variable_refs(image, base, reference, value):
    """Apply bounded PRX VREF {type, address, addend} records before lifting."""
    def offset(address, width):
        result = address - base
        if result < 0 or result + width > len(image):
            raise ValueError("Referência de variável fora da imagem PRX")
        return result
    for i in range(len(image) // 12 + 1):
        cursor = offset(reference + i * 12, 4)
        kind = struct.unpack_from(">I", image, cursor)[0]
        if kind == 0: return
        cursor = offset(reference + i * 12, 12)
        _, address, addend = struct.unpack_from(">III", image, cursor)
        data = (value + addend) & 0xFFFFFFFF
        if kind == 1:
            struct.pack_into(">I", image, offset(address, 4), data)
        elif kind in (4, 6, 57):
            target = offset(address, 2)
            if kind == 6: data = ((data + 0x8000) >> 16) & 0xFFFF
            if kind == 57:
                old = struct.unpack_from(">H", image, target)[0]
                data = (data & 0xFFFC) | (old & 3)
            struct.pack_into(">H", image, target, data & 0xFFFF)
        else:
            raise ValueError(f"Relocação de variável PRX não implementada: {kind}")
    raise ValueError("Tabela VREF sem terminador")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("decrypted", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()
    descriptions = []
    prepared = []
    variable_providers = {}
    for name, base, module_id in MODULES:
        src = args.decrypted / (name + ".prx")
        prx = Prx(src)
        out = args.output / name
        subprocess.run([sys.executable, str(SDK / "lift_prx.py"), str(src),
                        "--base", hex(base), "--output", str(out)], check=True)
        imports = json.loads((out / (name + "_imports.json")).read_text())
        image = bytearray((out / (name + "_image.bin")).read_bytes())
        excluded = []
        for file_offset, length in find_spu_images(prx.data):
            for offset, address, file_size, _ in prx.loads:
                if offset <= file_offset and file_offset + length <= offset + file_size:
                    begin = base + address + file_offset - offset
                    excluded.append((begin, (begin + length + 3) & ~3))
                    break
        variables, references = variable_tables(prx, image, base)
        for key, address in variables.items():
            if key in variable_providers: raise ValueError(f"Variável ambígua: {key}")
            variable_providers[key] = (name, address)
        prepared.append((name, base, module_id, prx, out, imports, image, references, excluded))
    dependencies = {name: set() for name, _, _ in MODULES}
    foreign_calls = []
    for name, base, module_id, prx, out, imports, image, references, excluded in prepared:
        for key, reference in references:
            provider = variable_providers.get(key)
            if provider and provider[0] != name: dependencies[name].add(provider[0])
            if not provider: print(f"Optional codec variable has no provider: {name}: {key}")
            apply_variable_refs(image, base, reference, provider[1] if provider else 0)
        (out / (name + "_image.bin")).write_bytes(image)
        hle = out / (name + "_hle.json")
        hle.write_text(json.dumps([dict(library=lib, nid=nid, stub=stub)
                                  for lib, items in imports.items() for nid, stub in items.items()]))
        exports = json.loads((out / (name + "_exports.json")).read_text())
        text, size = prx.text_extent()
        end = text + size
        # Internal dispatch tables also hold plain code pointers without a TOC
        # word. They are valid entries even though an OPD-only scan misses them.
        seeds_path = out / (name + "_functions.json")
        seeds = {int(f["start"], 0) for f in json.loads(seeds_path.read_text())}
        for offset in range((end + 3) & ~3, len(image) - 3, 4):
            address = struct.unpack_from(">I", image, offset)[0]
            if base + text <= address < base + end and address % 4 == 0:
                seeds.add(address)
        seeds = sorted(a for a in seeds if not any(lo <= a < hi for lo, hi in excluded))
        ranges_file = out / (name + "_data_ranges.json")
        ranges_file.write_text(json.dumps([dict(start=hex(lo), end=hex(hi)) for lo, hi in excluded]))
        seeds_path.write_text(json.dumps([dict(start=hex(a), end=hex(b))
            for a, b in zip(seeds, seeds[1:] + [base + end])]))
        subprocess.run([sys.executable, str(SDK / "ppu_lifter.py"),
                        str(out / (name + "_image.bin")), "--raw", "--base", hex(base),
                        "--toc", exports["toc"], "--functions", str(out / (name + "_functions.json")),
                        "--code-end", hex(base + end), "--hle-stubs", str(hle),
                        "--data-ranges", str(ranges_file),
                        "--symbol-prefix", name + "_", "--header-name", name + "_recomp.h",
                        "--output", str(out / "lifted"), "--jobs", "2", "--chunks",
                        str(min(16, max(2, (size - sum(hi - lo for lo, hi in excluded)) // 0x10000)))], check=True)
        header = (out / "lifted" / (name + "_recomp.h")).read_text()
        for symbol, address in re.findall(r'void (\w+func_([0-9A-F]+))\(ppu_context\* ctx\); /\* external \*/', header):
            if any(lo <= int(address, 16) < hi for lo, hi in excluded):
                foreign_calls.append(symbol)
        descriptions.append((name, base, module_id, exports))
    function_providers = {nid: name for name, _, _, exp in descriptions
                          for lib, items in exp["exports"].items() if not lib.startswith("!")
                          for nid in items}
    for name, _, _, _, _, imports, _, _, _ in prepared:
        for items in imports.values():
            for nid in items:
                provider = function_providers.get(nid)
                if provider and provider != name: dependencies[name].add(provider)
    lines = ['/* Generated locally from firmware metadata; do not distribute. */',
             '#include "ppu_recomp.h"', '#include "prx_loader.h"',
             '#include <cstdio>', '#include <cstdlib>', '#include <string>',
             'extern "C" void ppu_register_function(uint64_t, void (*)(ppu_context*));',
             'extern "C" uint64_t ppu_guest_call(uint32_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);',
             'static bool enabled;',
             'static void bind_function(uint32_t a, void (*f)(void*)) { ppu_register_function(a, (void (*)(ppu_context*))f); }']
    for symbol in foreign_calls:
        # A decoder's data may resemble a PPU branch. Keep the link explicit,
        # but stop visibly if such a path is ever reached; never fake its work.
        lines += [f'void {symbol}(ppu_context*) {{',
                  f'    std::fputs("PPU call into excluded SPU data: {symbol}\\n", stderr);',
                  '    std::abort();', '}']
    for name, base, _, exp in descriptions:
        lines += [f'extern "C" const func_entry {name}_function_table[];',
                  f'extern "C" const uint64_t {name}_function_table_count;',
                  f'static const prx_export {name}_exports[] = {{']
        for lib, items in exp["exports"].items():
            if lib.startswith("!"): continue
            for nid, opd in items.items():
                lines.append(f'    {{{nid}u, 0x{int(opd, 0) - base:X}u}},')
        lines += ['};']
    lines += ['extern "C" void ps3_load_prx_modules(void) {',
              '    const char* gate = std::getenv("BEN10_LLE_MEDIA");',
              '    if (!gate || std::string(gate) != "1") return;',
              '    const char* root = std::getenv("BEN10_FIRMWARE_PPU_DIR");',
              '    if (!root) { std::fputs("BEN10_FIRMWARE_PPU_DIR is required\\n", stderr); std::abort(); }',
              '    enabled = true;']
    for name, base, _, _ in descriptions:
        expected = (args.output / name / (name + "_image.bin")).stat().st_size
        lines += [f'    uint32_t {name}_size = 0;',
                  f'    auto {name}_path = std::string(root) + "/{name}/{name}_image.bin";',
                  f'    auto {name}_image = prx_image_load_file({name}_path.c_str(), &{name}_size);',
                  f'    if (!{name}_image || {name}_size != {expected}u) std::abort();',
                  f'    prx_module {name} = {{"{name}", 0x{base:X}, {name}_image, {name}_size,',
                  f'        (const prx_func_entry*){name}_function_table, {name}_function_table_count,',
                  f'        {name}_exports, sizeof({name}_exports)/sizeof(prx_export)}};',
                  f'    if (!prx_load_module(&{name}, bind_function).ok) std::abort();',
                  f'    std::free({name}_image);']
    lines += ['}', f'static unsigned state[{len(descriptions)}];',
              'static int32_t start_module(unsigned index) {',
              '    if (state[index]) return 0;', '    state[index] = 1;',
              '    int32_t result = 0;', '    switch (index) {']
    indices = {name: i for i, (name, _, _, _) in enumerate(descriptions)}
    for i, (name, _, _, exp) in enumerate(descriptions):
        start = exp["exports"].get("!syslib", {}).get("0xBC9A0086")
        lines += [f'    case {i}:']
        for dep in sorted(dependencies[name]):
            lines += [f'        if ((result = start_module({indices[dep]}))) break;']
        if start:
            lines += [f'        result = (int32_t)ppu_guest_call({start}u, 0,0,0,0,0,0,0,0);',
                      f'        std::fprintf(stderr, "[ben10-lle] {name} module_start returned 0x%08X\\n", (unsigned)result);']
        lines += ['        break;']
    lines += ['    }', '    state[index] = result ? 0 : 2;', '    return result;', '}',
              'extern "C" int32_t ben10_start_lle_module(uint16_t id) {',
              '    if (!enabled) return 0;', '    switch (id) {']
    for i, (_, _, module_id, _) in enumerate(descriptions):
        if module_id: lines += [f'    case 0x{module_id:04X}: return start_module({i});']
    lines += ['    }', '    return 0;', '}']
    (args.output / "modules.cpp").write_text('\n'.join(lines) + '\n')


if __name__ == "__main__":
    main()
