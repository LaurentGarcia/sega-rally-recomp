#!/usr/bin/env python3
"""Native model-space scene preparation, bounds and transactional failure."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeSceneTests(unittest.TestCase):
    def test_diagnostic_mesh_camera_projection_contract(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-scene-") as temporary:
            executable = Path(temporary) / "scene"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/include"),
                *[str(ROOT / path) for path in (
                    "platform/ps2/src/native_scene.cpp", "platform/ps2/src/native_geometry.cpp",
                    "platform/ps2/src/native_frame.cpp", "platform/ps2/tests/native_scene_test.cpp")],
                "-o", str(executable),
            ], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE SCENE PASS", tested.stdout)


if __name__ == "__main__":
    unittest.main()
