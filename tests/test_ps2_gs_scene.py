#!/usr/bin/env python3
"""ROM-free private GS replay serialization and source-boundary tests."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GsSceneTests(unittest.TestCase):
    def test_roundtrip_bounds_and_private_output(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ps2-scene-test-", dir=ROOT / "build") as temporary:
            output = Path(temporary) / "scene-test"
            command = [compiler, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror",
                       "-I", str(ROOT / "platform/ps2/include"), "-I", str(ROOT / "src"),
                       str(ROOT / "platform/ps2/tests/gs_scene_test.cpp"),
                       str(ROOT / "platform/ps2/src/gs_scene.cpp"), "-o", str(output)]
            built = subprocess.run(command, capture_output=True, text=True, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(output), str(Path(temporary) / "scene.bin")],
                                    capture_output=True, text=True, cwd=ROOT)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("PS2 scene round-trip, bounds, checksums, atomicity and private output checks passed", tested.stdout)

    def test_scene_suffix_is_excluded_from_source_exports(self):
        spec = importlib.util.spec_from_file_location("source_gate", ROOT / "scripts/check_source_release.py")
        gate = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(gate)
        with self.assertRaisesRegex(ValueError, "ROM, binary, capture"):
            gate.classify("tests/scene.bin", b"even text cannot bypass the scene suffix")


if __name__ == "__main__":
    unittest.main()
