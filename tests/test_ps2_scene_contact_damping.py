"""Scene-relative damping controls and native motion publication are asset-free."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SceneContactDamping(unittest.TestCase):
    def test_original_controls_and_atomic_motion_integration(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="scene-contact-") as directory:
            executable = Path(directory) / "test"
            names = ("scene_contact_damping", "vehicle_arithmetic", "recovered_scalar_metric",
                     "main_motion_update", "angle_fold", "rounded_cross_product")
            command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
                "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "platform/ps2/native"),
                *(str(ROOT / f"platform/ps2/native/{name}.cpp") for name in names),
                str(ROOT / "platform/ps2/tests/scene_contact_damping_test.cpp"), "-o", str(executable)]
            subprocess.run(command, capture_output=True, text=True, check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
            self.assertEqual("SCENE_CONTACT_DAMPING PASS checks=110 active_motion=1 atomic_rejection=1\n", result.stdout)


if __name__ == "__main__":
    unittest.main()
