"""Original projected contact arithmetic, without game assets or firmware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ContactProjectionTests(unittest.TestCase):
    def test_rounded_edges_signs_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-projection-") as directory:
            executable = Path(directory) / "projection"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/contact_projection.cpp"),
                       str(ROOT / "platform/ps2/tests/contact_projection_test.cpp"), "-o", str(executable)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("CONTACT_PROJECTION_HOST PASS cases=100774 hash=128f72100657ddf0", result.stdout)
            self.assertIn("CONTACT_PROJECTION_REJECTION PASS cases=64", result.stdout)


if __name__ == "__main__":
    unittest.main()
