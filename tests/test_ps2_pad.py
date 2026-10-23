#!/usr/bin/env python3
"""Exercise the real PS2 controller adapter with a ROM-free SDK mock."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PadInputTests(unittest.TestCase):
    def test_dma_port_lifetime_and_analog_request_retry(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-ps2-pad-") as temporary:
            output = Path(temporary) / "pad_input_test"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-I" + str(ROOT / "platform/ps2/tests/sdk"),
                       "-I" + str(ROOT / "platform/ps2/include"), "-I" + str(ROOT / "src"),
                       str(ROOT / "platform/ps2/tests/pad_input_test.cpp"),
                       str(ROOT / "platform/ps2/src/pad_input.cpp"), "-o", str(output)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("mocked pad lifecycle and analog retry checks passed", tested.stdout)


if __name__ == "__main__":
    unittest.main()
