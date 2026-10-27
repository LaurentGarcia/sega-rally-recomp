#!/usr/bin/env python3
"""Float-only platform clipping: contracts and measured reference differences."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeFloatGeometryTests(unittest.TestCase):
    def test_authored_boundaries_atomic_failures_and_reference_difference(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-float-geometry-") as temporary:
            executable = Path(temporary) / "geometry"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off",
                       "-I", str(ROOT / "platform/ps2/include")]
            native_object = Path(temporary) / "native_float.o"
            native = subprocess.run(command + ["-Wdouble-promotion", "-c",
                str(ROOT / "platform/ps2/src/native_geometry_float.cpp"),
                "-o", str(native_object)], capture_output=True, text=True, timeout=120)
            self.assertEqual(native.returncode, 0, native.stdout + native.stderr)
            command += [str(ROOT / p) for p in (
                "platform/ps2/src/native_geometry.cpp",
                "platform/ps2/tests/native_geometry_float_test.cpp")]
            built = subprocess.run(command + [str(native_object), "-o", str(executable)], capture_output=True,
                                   text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("FLOAT GEOMETRY PASS", tested.stdout)
            self.assertIn("compared=20010 ", tested.stdout)


if __name__ == "__main__":
    unittest.main()
