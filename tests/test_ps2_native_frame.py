#!/usr/bin/env python3
"""Pure native GS frame validation with no arcade/runtime dependency."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeFrameTests(unittest.TestCase):
    def test_bounded_original_packet_contract(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-frame-") as temporary:
            executable = Path(temporary) / "test"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "platform/ps2/include"),
                str(ROOT / "platform/ps2/tests/native_frame_test.cpp"),
                str(ROOT / "platform/ps2/src/native_frame.cpp"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)


if __name__ == "__main__":
    unittest.main()
