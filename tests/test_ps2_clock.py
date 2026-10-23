#!/usr/bin/env python3
"""Portable exact queue contracts and the ROM-free EE benchmark workload."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ClockContracts(unittest.TestCase):
    def test_queue_order_and_benchmark_oracles(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        fixtures = (
            ("tests/test_model2a_clock.cpp", ["src/runtime/model2a_control.cpp"],
             "exact screen phase and global IRQ ties passed"),
            ("tests/test_model2a_bus_clock.cpp", ["src/runtime/model2a_control.cpp",
              "src/runtime/model2a_bus.cpp", "src/runtime/cpu.cpp"], "passed"),
            ("platform/ps2/tests/clock_benchmark.cpp", [], "CLOCK BENCH PASS"),
        )
        with tempfile.TemporaryDirectory(prefix="rally-clock-") as temporary:
            for fixture, dependencies, expected in fixtures:
                with self.subTest(fixture=fixture):
                    executable = Path(temporary) / "clock-test"
                    result = subprocess.run([
                        compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                        "-I", str(ROOT / "src"), str(ROOT / fixture),
                        str(ROOT / "src/runtime/model2a_clock.cpp"),
                        *(str(ROOT / source) for source in dependencies),
                        "-o", str(executable),
                    ], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(executable)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn(expected, result.stdout)


if __name__ == "__main__":
    unittest.main()
