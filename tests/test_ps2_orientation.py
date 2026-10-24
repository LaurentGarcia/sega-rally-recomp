#!/usr/bin/env python3
"""Check typed orientation finalization without original game data/runtime."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeOrientationTests(unittest.TestCase):
    def test_orientation_rounding_channels_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-orientation-") as temporary:
            executable = Path(temporary) / "orientation"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       *(str(ROOT / p) for p in ["platform/ps2/native/angle_fold.cpp",
                         "platform/ps2/native/orientation.cpp", "platform/ps2/tests/orientation_test.cpp"]),
                       "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE_ORIENTATION PASS", tested.stdout)


if __name__ == "__main__":
    unittest.main()
