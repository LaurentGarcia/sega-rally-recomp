"""Bounded native affine arithmetic; no original game or firmware inputs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RoundedAffineTests(unittest.TestCase):
    def test_rounding_order_aliases_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-rounded-affine-") as directory:
            executable = Path(directory) / "affine"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/rounded_affine_transform.cpp"),
                       str(ROOT / "platform/ps2/tests/rounded_affine_transform_test.cpp"), "-o", str(executable)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("ROUNDED_AFFINE_PROBE PASS cases=4065 hash=eb07dfee53139c87", result.stdout)
            self.assertIn("ROUNDED_AFFINE_HOST PASS cases=84273 hash=7005cd920cda465e", result.stdout)
            self.assertIn("ROUNDED_AFFINE_REJECTION PASS cases=720 order_guard=1 fused_guard=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
