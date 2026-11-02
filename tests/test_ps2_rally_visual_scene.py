"""Original textured assets follow explicit native poses and recovered view math."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SOURCES=[
    "platform/ps2/native/rally_visual_scene.cpp",
    "platform/ps2/native/car_render_bridge.cpp",
    "platform/ps2/native/driving_render_view.cpp",
    "platform/ps2/native/rounded_affine_transform.cpp",
    "platform/ps2/native/main_balance_math.cpp",
    "platform/ps2/native/vehicle_arithmetic.cpp",
    "platform/ps2/native/angle_fold.cpp",
    "platform/ps2/src/native_textured_model.cpp",
    "platform/ps2/src/world_scene.cpp",
    "platform/ps2/src/native_geometry.cpp",
    "platform/ps2/src/native_geometry_float.cpp",
    "platform/ps2/tests/rally_visual_scene_test.cpp",
]

class RallyVisualScene(unittest.TestCase):
    def test_native_pose_material_lifetime_and_config_binding(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="rally-visual-") as directory:
            executable=Path(directory)/"fixture"
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/include"),
                "-I",str(ROOT/"platform/ps2/native"),
                *(str(ROOT/source) for source in SOURCES),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable),directory],capture_output=True,text=True,check=True)
            self.assertIn("RALLY_VISUAL PASS",result.stdout)
            self.assertIn("native_pose_movement=1 projection=1 reset=1 asset_binding=1 atomic_history=1 no_update_allocations=1",result.stdout)

if __name__=="__main__":unittest.main()
