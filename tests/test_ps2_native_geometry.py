#!/usr/bin/env python3
"""Native homogeneous clipping against independent rational constraints."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeGeometryTests(unittest.TestCase):
    def test_clipping_contract_and_independent_rational_oracle(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-geometry-") as temporary:
            executable = Path(temporary) / "geometry"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off",
                "-I", str(ROOT / "platform/ps2/include"),
                str(ROOT / "platform/ps2/src/native_geometry.cpp"),
                str(ROOT / "platform/ps2/tests/native_geometry_test.cpp"),
                "-o", str(executable),
            ], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE GEOMETRY PASS", tested.stdout)
            self.assertIn("oracle cases=20000", tested.stdout)


if __name__ == "__main__":
    unittest.main()
