"""Numerical boundary regression for the native driver's angle operation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RecoveredAtangentTest(unittest.TestCase):
    def test_original_reference_numeric_boundary(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory() as directory:
            target = str(Path(directory)/"angle")
            sources = [ROOT/"platform/ps2/native/recovered_atangent.cpp",
                       ROOT/"platform/ps2/native/vehicle_arithmetic.cpp",
                       ROOT/"platform/ps2/tests/recovered_atangent_test.cpp"]
            subprocess.run([compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off",
                            "-I",str(ROOT/"platform/ps2/native"),*map(str,sources),"-o",target],
                           check=True,capture_output=True,text=True,timeout=60)
            result = subprocess.run([target],check=True,capture_output=True,text=True,timeout=90)
            self.assertEqual(result.stdout.strip(),"RECOVERED_ATANGENT PASS comparisons=564066 hash=234504b840688020")


if __name__=="__main__":
    unittest.main()
