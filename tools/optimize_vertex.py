#!/usr/bin/env python3
"""Cache private SPU registers in locals for the fingerprinted LS-only loop."""
from pathlib import Path
import argparse
import hashlib
import re

ROOT = Path(__file__).resolve().parents[1]
FUNCTION = 'job_00844E80_spu_func_000002E0'
KERNEL_DIGEST = '317b8b616d3b595fa7ecb3e8e6bab464f65b093a1c58da893c7d447281832303'
SKIN_FUNCTION = 'job_0084B600_spu_func_00000268'
KERNELS = {
    FUNCTION: (KERNEL_DIGEST, 'return;'),
    SKIN_FUNCTION: ('44e3d91d36f514b2764bf88bdf8d05644e469fb45b3a4d1bd913d75ce02f5132', 'SPU_RET(ctx);'),
}


def transform(source, expected_digest=None, function=FUNCTION, exit_marker=None):
    known_digest, known_exit = KERNELS[function]
    if expected_digest is None: expected_digest = known_digest
    if exit_marker is None: exit_marker = known_exit
    marker = 'void ' + function + '(spu_context* ctx) {'
    if source.count(marker) != 1:
        raise ValueError('Kernel identificado ausente ou duplicado')
    start = source.index(marker)
    end = source.index('\n}', start) + 2
    original = source[start:end]
    if hashlib.sha256(original.encode()).hexdigest() != expected_digest:
        raise ValueError('Kernel diferente do validado; desative BEN10_VERTEX_CACHE para investigar')
    if original.count(exit_marker) != 1:
        raise ValueError('Saída do kernel mudou')
    fields = set(re.findall(r'ctx->(\w+)', original))
    if not fields <= {'gpr', 'pc'} or 'gpr' not in fields:
        raise ValueError('Acesso de contexto não permitido no kernel')
    # This loop cannot call a dispatcher/channel or publish its private regs.
    context_calls = re.findall(r'\b(\w+)\(ctx\s*(?:,|\))', original)
    if set(context_calls) - {'spu_ls_read128', 'spu_ls_write128', 'SPU_RET'}:
        raise ValueError('Chamada que observa contexto dentro do kernel')
    used = sorted({int(i) for i in re.findall(r'ctx->gpr\[(\d+)\]', original)})
    written = sorted({int(i) for i in re.findall(r'ctx->gpr\[(\d+)\]\s*=(?!=)', original)})
    if not used or max(used) > 127 or re.search(r'ctx->gpr\[(?!\d+\])', original):
        raise ValueError('Índice de registrador não permitido')
    body = re.sub(r'ctx->gpr\[(\d+)\]', lambda m: 'vertex_reg_' + m[1], original)
    declarations = ''.join(f'    u128 vertex_reg_{i} = ctx->gpr[{i}];\n' for i in used)
    fallback = 'ben10_vertex_uncached_kernel'
    guard = f'    if (!ben10_vertex_cache_safe(ctx)) {{ {fallback}(ctx); return; }}\n'
    body = body.replace('{\n', '{\n' + guard + declarations, 1)
    writeback = ''.join(f'ctx->gpr[{i}] = vertex_reg_{i};\n' for i in written)
    # Only the original exit is replaced; the guard must keep its direct return.
    exit_at = body.rindex(exit_marker)
    body = body[:exit_at] + writeback + body[exit_at:]
    body = body.replace('spu_ls_read128(', 'ben10_vertex_ls_read128(')
    body = body.replace('spu_ls_write128(', 'ben10_vertex_ls_write128(')
    baseline = original.replace('void ' + function, 'static __attribute__((noinline,cold)) void ' + fallback, 1)
    return source[:start] + baseline + '\n\n' + body + source[end:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--function', choices=list(KERNELS), default=FUNCTION)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(output.is_relative_to(ROOT/folder) for folder in ('build', 'artifacts')):
        parser.error('O C gerado deve permanecer em build/ ou artifacts/, fora do Git')
    result = transform(args.source.read_text(), function=args.function)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('#include "vertex_memory.h"\n' + result)


if __name__ == '__main__':
    main()
