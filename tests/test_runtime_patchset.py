"""Patch series remain reproducible after overlapping fixes and preserve edits."""
import difflib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from apply_runtime_patches import apply_patchset


class PatchsetTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.sdk = root / 'sdk'
        self.sdk.mkdir()
        self.base = 'first zero\nsecond zero\nthird zero\n'
        (self.sdk / 'a.c').write_text(self.base)
        (self.sdk / 'b.c').write_text('unchanged\n')
        subprocess.run(['git', 'init', '-q', str(self.sdk)], check=True)
        subprocess.run(['git', '-C', str(self.sdk), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(self.sdk), '-c', 'user.name=Fixture', '-c',
                        'user.email=fixture@example.invalid', '-c', 'commit.gpgsign=false',
                        'commit', '-qm', 'synthetic base'], check=True)
        self.revision = subprocess.check_output(['git', '-C', str(self.sdk), 'rev-parse', 'HEAD'], text=True).strip()
        one = self.base.replace('first zero', 'first one')
        self.final = one.replace('second zero', 'second two')
        self.patches = []
        for i, (name, before, after) in enumerate([
                ('a.c', self.base, one), ('a.c', one, self.final),
                ('b.c', 'unchanged\n', 'patched\n')]):
            p = root / f'{i:04d}.patch'
            p.write_text(''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                         fromfile='a/' + name, tofile='b/' + name)))
            self.patches.append(p)

    def test_overlapping_upgrade_and_idempotency(self):
        apply_patchset(self.sdk, self.patches[:1], self.revision)
        self.assertEqual(apply_patchset(self.sdk, self.patches, self.revision), ['a.c', 'b.c'])
        self.assertEqual((self.sdk / 'a.c').read_text(), self.final)
        self.assertEqual((self.sdk / 'b.c').read_text(), 'patched\n')
        self.assertEqual(apply_patchset(self.sdk, self.patches, self.revision), [])

    def test_unknown_edit_rejects_before_any_write(self):
        (self.sdk / 'b.c').write_text('user edit\n')
        with self.assertRaisesRegex(ValueError, 'desconhecidas'):
            apply_patchset(self.sdk, self.patches, self.revision)
        self.assertEqual((self.sdk / 'a.c').read_text(), self.base)
        self.assertEqual((self.sdk / 'b.c').read_text(), 'user edit\n')

    def test_wrong_pin_preserves_checkout(self):
        with self.assertRaisesRegex(ValueError, 'revisão'):
            apply_patchset(self.sdk, self.patches, '0' * 40)
        self.assertEqual((self.sdk / 'a.c').read_text(), self.base)
