import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VideoMetadata(unittest.TestCase):
    def test_metadata_can_run_ahead_of_pixel_consumer(self):
        sdk = ROOT / 'third_party/ps3recomp'
        with tempfile.TemporaryDirectory(prefix='ben10-vdec-') as directory:
            binary = Path(directory) / 'metadata'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu17', '-O1', '-UNDEBUG',
                            '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                            '-I' + str(sdk), '-I' + str(sdk / 'include'),
                            str(ROOT / 'tests/fixtures/vdec_metadata.c'), '-lm', '-o', str(binary)],
                           check=True, capture_output=True, text=True, timeout=30)
            result = subprocess.run([str(binary)], check=True,
                                    capture_output=True, text=True, timeout=4)
            self.assertIn('PASS independent metadata', result.stdout)
