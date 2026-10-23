#!/usr/bin/env python3
"""Build the recovered native leaf alone, without any board/CPU runtime."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeProgressTests(unittest.TestCase):
    def test_boundaries_and_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-progress-") as temporary:
            executable = Path(temporary) / "progress"
            result = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "platform/ps2/native"),
                str(ROOT / "platform/ps2/native/race_progress.cpp"),
                str(ROOT / "platform/ps2/tests/race_progress_test.cpp"),
                "-o", str(executable),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Native race progress PASS: 121", result.stdout)


if __name__ == "__main__":
    unittest.main()
