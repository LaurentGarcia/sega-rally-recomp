"""Fixed world transforms use original geometry and do not advance game state."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class WorldScene(unittest.TestCase):
    def test_group_culling_matches_ordinary_path(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-groups-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_groups_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_GROUPS PASS frames=2052",result.stdout)
            self.assertIn("no_update_allocations=1",result.stdout)

    def test_native_transform_ownership_and_capacity(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-scene-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_SCENE PASS checks=50 fixed_translation_pixels=62 no_update_allocations=1",result.stdout)
if __name__=="__main__":unittest.main()
