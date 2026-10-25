"""Compile a manufactured kernel through the real register-cache transformer."""
from pathlib import Path
import hashlib
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.optimize_vertex import transform, FUNCTION

source = (ROOT/'tests/vertex_cache_fixture.c').read_text()
exit_marker = 'return;'
if len(sys.argv) > 2:
    source = source.replace('{ ctx->pc = 0x5E0; g_spu_trampoline_fn = job_00844E80_spu_func_000005E0; return; }', 'SPU_RET(ctx);')
    exit_marker = 'SPU_RET(ctx);'
start = source.index('void ' + FUNCTION + '(')
end = source.index('\n}', start) + 2
digest = hashlib.sha256(source[start:end].encode()).hexdigest()
Path(sys.argv[1]).write_text('#include "vertex_memory.h"\n' + transform(source, digest, exit_marker=exit_marker))
