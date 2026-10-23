#!/usr/bin/env python3
"""Build native pose publication independently of all guest runtime code."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativePoseTests(unittest.TestCase):
    def test_raw_bits_and_every_flag_pair(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-pose-") as temporary:
            executable = Path(temporary) / "pose"
            result = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "platform/ps2/native"),
                str(ROOT / "platform/ps2/native/car_pose.cpp"),
                str(ROOT / "platform/ps2/tests/car_pose_test.cpp"),
                "-o", str(executable),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Native car pose PASS: 65536", result.stdout)


if __name__ == "__main__":
    unittest.main()
