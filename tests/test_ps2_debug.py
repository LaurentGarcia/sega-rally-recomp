#!/usr/bin/env python3
"""Validate diagnostic attribution and opt-in launch switches without ROMs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DebugFlowTests(unittest.TestCase):
    def test_measured_costs_and_launch_options(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-ps2-debug-") as directory:
            output = Path(directory) / "debug-test"
            subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "platform/ps2/include"),
                            str(ROOT / "platform/ps2/tests/debug_flow_test.cpp"), "-o", str(output)], check=True)
            subprocess.run([str(output), str(Path(directory) / "rally.cfg")], check=True)


if __name__ == "__main__":
    unittest.main()
