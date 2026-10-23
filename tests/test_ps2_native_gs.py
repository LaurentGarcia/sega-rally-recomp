#!/usr/bin/env python3
"""Portable GS texture and packet contracts; no game data or GPU needed."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeGsTests(unittest.TestCase):
    def test_texture_decode_cache_keys_and_packet_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-gs-") as temporary:
            executable = Path(temporary) / "native_gs_test"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off",
                "-I", str(ROOT / "platform/ps2/include"), "-I", str(ROOT / "src"),
                str(ROOT / "platform/ps2/tests/native_gs_test.cpp"),
                str(ROOT / "platform/ps2/src/model2_gs.cpp"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE GS portable decoder/preflight PASS", tested.stdout)


if __name__ == "__main__":
    unittest.main()
