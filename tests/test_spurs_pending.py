"""A running SPU task must admit its published peers before waiting on them."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'third_party/ps3recomp'


def production_function(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\([^;\n]*\)\s*\{', source, re.M)
    if not match:
        raise AssertionError('Missing production function ' + name)
    brace = source.index('{', match.start())
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


class PendingTasks(unittest.TestCase):
    def test_peer_creation_then_wait_without_exit(self):
        source = (SDK / 'runtime/spu/spu_workload.c').read_text()
        tables = source[source.index('#define TS_TRACK '):source.index('static uint8_t* ts_running(')]
        functions = '\n'.join(production_function(source, name) for name in [
            'rd_be16', 'rd_be32', 'spu_elf_image_size', 'ts_running', 'ts_lock', 'ts_unlock',
            'spu_taskset_task_exited', 'spu_taskset_start_pending',
            'spu_signalled_update', 'spu_taskset_wait_signal'])
        fixture = (ROOT / 'tests/fixtures/spurs_pending.c').read_text()
        fixture = fixture.replace('/* PRODUCTION_FUNCTIONS */', tables + functions)
        with tempfile.TemporaryDirectory(prefix='ben10-pending-') as directory:
            root = Path(directory); c = root / 'test.c'; binary = root / 'test'
            c.write_text(fixture)
            subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu17', '-O1', '-UNDEBUG',
                            '-I' + str(SDK / 'include'), str(c), '-pthread', '-o', str(binary)],
                           check=True, capture_output=True, text=True, timeout=30)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=4)
            self.assertIn('PASS', result.stdout)
