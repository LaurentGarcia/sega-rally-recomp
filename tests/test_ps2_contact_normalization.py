"""Bounded native contact normalization; no original game or firmware inputs."""
from pathlib import Path
import sys
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_contact_normalization as CHECKER
finally:
    sys.path.pop(0)


class ContactNormalizationTests(unittest.TestCase):
    def test_wrong_original_images_are_rejected(self):
        for image in (b"", bytes(124), bytes(2097152)):
            with self.assertRaises(ValueError):
                CHECKER.verify_image(image)

    def test_rounding_order_aliases_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-normalization-") as directory:
            executable = Path(directory) / "affine"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/contact_normalization.cpp"),
                       str(ROOT / "platform/ps2/tests/contact_normalization_test.cpp"), "-o", str(executable)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("CONTACT_NORMALIZATION_PROBE PASS cases=4064 hash=2f11e06fc7af1915", result.stdout)
            self.assertIn("CONTACT_NORMALIZATION_HOST PASS cases=59912 hash=53b71c272e0a45ea", result.stdout)
            self.assertIn("CONTACT_NORMALIZATION_REJECTION PASS cases=48 underflow_guard=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
