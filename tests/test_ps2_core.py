#!/usr/bin/env python3
"""Compile and run the ROM-free paged-memory and CPU-video regressions."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PagedCoreTests(unittest.TestCase):
    def test_resident_and_paged_model2a_memory(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-paged-core-") as temporary:
            output = Path(temporary) / "test_model2a_paged_rom"
            command = [compiler, "-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                       "-Wno-unused-parameter", "-Wno-unused-but-set-variable", "-Wno-sign-compare",
                       "-DM2_PAGED_ROM=1", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "src"),
                       str(ROOT / "tests/test_model2a_paged_rom.cpp"),
                       str(ROOT / "src/runtime/model2a_bus.cpp"), str(ROOT / "src/runtime/m2_tgp_board.cpp"),
                       str(ROOT / "src/runtime/geo.cpp"), "-o", str(output)]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(output)], capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("10,000 ROM reads", tested.stdout)

    def test_cpu_video_allocates_gpu_scratch_only_when_enabled(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-video-storage-") as temporary:
            for defines in ([], ["-DM2_VITA_RENDER_OPT=1"], ["-DM2_COMPACT_TILEMAP=1"]):
                with self.subTest(defines=defines):
                    output = Path(temporary) / "test_video_gpu_storage"
                    command = [compiler, "-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off",
                               *defines, "-I", str(ROOT / "src"),
                               str(ROOT / "tests/test_video_gpu_storage.cpp"),
                               str(ROOT / "src/runtime/video.cpp"), str(ROOT / "src/runtime/raster.cpp"),
                               "-o", str(output)]
                    built = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    tested = subprocess.run([str(output)], capture_output=True, text=True)
                    self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
                    self.assertIn("lazy allocation, resize, release and renderer switches passed", tested.stdout)

    def test_compact_tile_rows_equal_full_plane_composition(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        results = []
        with tempfile.TemporaryDirectory(prefix="rally-video-compact-") as temporary:
            for defines in ([], ["-DM2_COMPACT_TILEMAP=1"]):
                output = Path(temporary) / "test_video_compact"
                command = [compiler, "-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off",
                           *defines, "-I", str(ROOT / "src"), str(ROOT / "tests/test_video_compact.cpp"),
                           str(ROOT / "src/runtime/video.cpp"), str(ROOT / "src/runtime/raster.cpp"),
                           "-o", str(output)]
                built = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                tested = subprocess.run([str(output)], capture_output=True, text=True)
                self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
                self.assertEqual(len(tested.stdout.splitlines()), 64)
                results.append(tested.stdout)
        self.assertEqual(*results)


if __name__ == "__main__":
    unittest.main()
