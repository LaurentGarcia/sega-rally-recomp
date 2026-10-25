"""Portable bit-preserving main-car pose contract; no game assets required."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_main_car_pose as CHECKER
finally:
    sys.path.pop(0)


class MainCarPoseTests(unittest.TestCase):
    def test_all_low_flag_words_bit_lanes_and_independent_snapshots(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="main-car-pose-") as directory:
            executable = Path(directory) / "test"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "platform/ps2/native"), str(ROOT / "platform/ps2/native/main_car_pose.cpp"),
                str(ROOT / "platform/ps2/tests/main_car_pose_test.cpp"), "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stderr)
            self.assertIn("PROBE PASS cases=2184 words=48048 hash=d2fae2297e475353", tested.stdout)
            self.assertIn("HOST PASS cases=77208 words=1698576 hash=b74eb51b13296a1c", tested.stdout)

    def test_wrong_original_images_are_rejected(self):
        for image in (b"", bytes(184), bytes(2097152)):
            with self.assertRaises(ValueError):
                CHECKER.verify_image(image)


if __name__ == "__main__":
    unittest.main()
