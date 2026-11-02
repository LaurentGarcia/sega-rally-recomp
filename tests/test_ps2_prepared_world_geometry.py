"""Owned corner proof preserves the old gather bytes and dynamic admission."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ["platform/ps2/src/world_scene.cpp", "platform/ps2/src/native_geometry.cpp",
           "platform/ps2/src/native_geometry_float.cpp", "platform/ps2/src/vu1_transform.cpp",
           "platform/ps2/src/vu1_transform_math.cpp", "platform/ps2/src/vu0_pack.cpp",
           "platform/ps2/tests/prepared_world_geometry_test.cpp"]


class PreparedWorldGeometry(unittest.TestCase):
    def test_owned_corner_bytes_ranges_and_dynamic_admission(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="prepared-world-geometry-") as directory:
            binary = Path(directory) / "test"
            command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                       "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "platform/ps2/include"),
                       *[str(ROOT / source) for source in SOURCES], "-o", str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PREPARED_GEOMETRY PASS", result.stdout)
            self.assertIn("byte_parity=1 partial_ranges=1 dynamic_admission=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
