#!/usr/bin/env python3
"""Check the bounded native angle operation without guest runtime code."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeAngleFoldTests(unittest.TestCase):
    def test_every_folded_binary32_value_and_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-angle-") as temporary:
            executable = Path(temporary) / "angle"
            result = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off",
                "-I", str(ROOT / "platform/ps2/native"),
                str(ROOT / "platform/ps2/native/angle_fold.cpp"),
                str(ROOT / "platform/ps2/tests/angle_fold_test.cpp"),
                "-o", str(executable),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable), "--exhaustive-folded"],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("folded interval exhaustive=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
