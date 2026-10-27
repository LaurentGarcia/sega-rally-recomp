import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class VehicleArithmeticTest(unittest.TestCase):
    def test_ieee_boundaries_and_expression_operations(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="rally-vehicle-math-") as directory:
            executable = pathlib.Path(directory) / "test"
            subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                str(ROOT / "platform/ps2/native/vehicle_arithmetic.cpp"),
                str(ROOT / "platform/ps2/tests/vehicle_arithmetic_test.cpp"),
                "-o", str(executable),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
            self.assertIn("VEHICLE_ARITHMETIC PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
