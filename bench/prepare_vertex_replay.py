#!/usr/bin/env python3
"""Prepare a private benchmark TU from the user's own translated vertex job."""
from pathlib import Path
import argparse
import os

ROOT = Path(__file__).resolve().parents[1]
FUNCTION = 'void job_00844E80_spu_func_000002E0(spu_context* ctx) {'


def extract_kernel(source):
    if source.count(FUNCTION) != 1:
        raise ValueError('A função de geometria identificada não está presente uma única vez')
    start = source.index(FUNCTION)
    end = source.index('\n}', start) + 2
    body = source[start:end]
    # This replay is deliberately limited to the LS-only loop and its one exit.
    # Never pretend that a different generated function is a safe standalone job.
    if body.count('return;') != 1 or 'ctx->pc = 0x5E0;' not in body:
        raise ValueError('A estrutura do kernel mudou; reveja o replay antes de executar')
    return body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_data = Path(os.environ.get('XDG_DATA_HOME', Path.home()/'.local/share'))/'Ben10OmniversePC'
    parser.add_argument('--source', type=Path, default=default_data/'native/jobs/lifted/job_00844E80/spu_recomp.c')
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/vertex-replay/kernel.c')
    args = parser.parse_args()
    output = args.output.expanduser().resolve()
    # Generated game C is allowed only in the ignored artifacts directory.
    if not output.is_relative_to(ROOT/'artifacts'):
        parser.error('O código local deve permanecer em artifacts/, fora do Git')
    body = extract_kernel(args.source.expanduser().read_text())
    preamble = '''/* Generated locally from the user's own game. Never redistribute. */
#include "spu_context.h"
#include "spu_helpers.h"
#ifdef VERTEX_INT
#include "vertex_integer.h"
#endif
#ifdef VERTEX_FMA
#include "vertex_float.h"
#endif
extern void job_00844E80_spu_func_000005E0(spu_context*);
'''
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(preamble + body + '\n')
    print(output)


if __name__ == '__main__':
    main()
