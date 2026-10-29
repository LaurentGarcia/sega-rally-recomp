"""Fixed world transforms use original geometry and do not advance game state."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class WorldScene(unittest.TestCase):
    def test_cached_inside_projection_and_fallback(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-projection-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_projection_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_PROJECTION PASS frames=1000",result.stdout)
            self.assertIn("collision_and_failure=1 atomic_capacity=1",result.stdout)

    def test_identity_bounds_and_near_identity_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-identity-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_identity_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_IDENTITY PASS frames=25601",result.stdout)
            self.assertIn("signed_zero_masks=4096 single_bit_mutations=512",result.stdout)

    def test_batch_lookup_collisions_and_generation_wrap(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-lookup-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_lookup_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_LOOKUP PASS",result.stdout)
            self.assertIn("wrap_updates=70000 largest_batch=96",result.stdout)

    def test_owned_prepared_models_and_transactional_updates(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-prepared-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_prepared_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_PREPARED PASS frames=776",result.stdout)
            self.assertIn("immutable_ownership=1 atomic_replacement=1 no_update_allocations=1",result.stdout)

    def test_native_float_pipeline_and_outside_provider(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-float-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),
                str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_float_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_FLOAT PASS",result.stdout)
            self.assertIn("native_clipping=1 no_update_allocations=1",result.stdout)

    def test_bounded_transform_provider_and_fallback(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-batch-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_batch_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_BATCH PASS",result.stdout)
            self.assertIn("bounded_batches=96 guarded_fallback=1 no_update_allocations=1",result.stdout)

    def test_transformed_vertex_cache_and_profile(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-cache-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_cache_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_CACHE PASS",result.stdout)
            self.assertIn("mutation_and_collision=1 no_update_allocations=1",result.stdout)

    def test_group_culling_matches_ordinary_path(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="world-groups-") as directory:
            executable=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
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
                str(ROOT/"platform/ps2/src/world_scene.cpp"),str(ROOT/"platform/ps2/src/native_geometry.cpp"),str(ROOT/"platform/ps2/src/native_geometry_float.cpp"),
                str(ROOT/"platform/ps2/tests/world_scene_test.cpp"),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("WORLD_SCENE PASS checks=50 fixed_translation_pixels=62 no_update_allocations=1",result.stdout)
if __name__=="__main__":unittest.main()
