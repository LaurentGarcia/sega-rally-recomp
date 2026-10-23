#!/usr/bin/env python3
"""Portable contract/oracle checks; an EE ELF run additionally tests COP2."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Vu0PackingTests(unittest.TestCase):
    def test_gs_fixed_coordinates_match_independent_binary32_oracle(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-vu0-packing-") as temporary:
            executable = Path(temporary) / "vu0_pack_test"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/include"),
                str(ROOT / "platform/ps2/tests/vu0_pack_test.cpp"),
                str(ROOT / "platform/ps2/src/vu0_pack.cpp"), "-o", str(executable),
            ], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("backend=host scalar", tested.stdout)
            self.assertIn("VU0 PACK PASS components=295872 hash=b262a2e1bbd6091a", tested.stdout)


if __name__ == "__main__":
    unittest.main()
