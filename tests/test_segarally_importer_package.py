"""A transient disk-image service failure may retry; corrupt images never pass."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

path = Path(__file__).resolve().parents[1] / 'platform/segarally/macos/importer/package.py'
spec = importlib.util.spec_from_file_location('importer_package', path)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class Verification(unittest.TestCase):
    def result(self, code, error=''):
        return subprocess.CompletedProcess(['hdiutil', 'verify', 'image'], code, '', error)

    def test_retry_preserves_failure_and_requires_success(self):
        with tempfile.TemporaryDirectory() as root, patch.object(package.time, 'sleep'), \
             patch.object(package.subprocess, 'run', side_effect=[
                 self.result(1, 'Resource temporarily unavailable'), self.result(0)]) as run:
            image = Path(root) / 'image.dmg'
            self.assertEqual(package.verify_dmg(image), 2)
            self.assertEqual(run.call_count, 2)
            self.assertIn('Resource temporarily unavailable', (image.parent / 'verify-1.log').read_text())

    def test_checksum_failure_is_not_retried(self):
        with tempfile.TemporaryDirectory() as root, \
             patch.object(package.subprocess, 'run', return_value=self.result(1, 'checksum mismatch')) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                package.verify_dmg(Path(root) / 'image.dmg')
            self.assertEqual(run.call_count, 1)

    def test_exhausted_transient_failure_is_not_success(self):
        with tempfile.TemporaryDirectory() as root, patch.object(package.time, 'sleep'), \
             patch.object(package.subprocess, 'run', return_value=self.result(1, 'Resource temporarily unavailable')) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                package.verify_dmg(Path(root) / 'image.dmg')
            self.assertEqual(run.call_count, 3)

if __name__ == '__main__':
    unittest.main()
