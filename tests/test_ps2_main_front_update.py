#!/usr/bin/env python3
"""Original bounded front-stage behavior and atomicity without game assets."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
NAMES=("main_front_update","vehicle_arithmetic","main_balance_math","recovered_atangent",
       "contact_point_preparation","rounded_affine_transform","main_contact_slot","contact_search",
       "contact_neighborhood","contact_projection","contact_result","recovered_scalar_metric","rounded_cross_product")

class MainFrontUpdateTests(unittest.TestCase):
    def test_native_composite(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="rally-front-") as directory:
            probe=Path(directory)/"front"
            subprocess.run([compiler,"-std=c++20","-O1","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off",
                            "-I",str(ROOT/"platform/ps2/native"),*(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NAMES),
                            str(ROOT/"platform/ps2/tests/main_front_update_test.cpp"),"-o",str(probe)],check=True)
            result=subprocess.run([str(probe)],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result.stdout,"MAIN_FRONT_UPDATE PASS checks=36 atomic_rejections=4 event_transition=1\n")

if __name__=="__main__":unittest.main()
