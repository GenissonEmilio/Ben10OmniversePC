from pathlib import Path
import hashlib
import unittest
from tools.optimize_vertex import transform, FUNCTION

SOURCE = (Path(__file__).parent/'vertex_cache_fixture.c').read_text()
def digest(source):
    start = source.index('void ' + FUNCTION + '(')
    end = source.index('\n}', start) + 2
    return hashlib.sha256(source[start:end].encode()).hexdigest()

class VertexTransformTests(unittest.TestCase):
    def test_fingerprint_rejects_changed_program(self):
        with self.assertRaises(ValueError): transform(SOURCE)
        changed = SOURCE.replace('spu_xor(', 'spu_or(')
        with self.assertRaises(ValueError): transform(changed, digest(SOURCE))

    def test_context_observer_is_rejected(self):
        changed = SOURCE.replace('loc_fixture:', 'loc_fixture:\n    observe_context(ctx, 0);')
        with self.assertRaises(ValueError): transform(changed, digest(changed))

    def test_unknown_context_field_is_rejected(self):
        changed = SOURCE.replace('loc_fixture:', 'loc_fixture:\n    ctx->status = 0;')
        with self.assertRaises(ValueError): transform(changed, digest(changed))

