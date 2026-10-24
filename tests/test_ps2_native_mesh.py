#!/usr/bin/env python3
"""Bounded native mesh decoding and atomic failure with an independent writer."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    vertices = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0),
                (0, 0, 1), (2, 0, 1), (2, 2, 1), (0, 2, 1)]
    faces = [(0, 1, 2, 0xffff0000), (0, 2, 3, 0xff00ff00),
             (4, 5, 6, 0xff0000ff), (4, 6, 7, 0xffffffff)]
    payload = b"".join(struct.pack("<3f", *v) for v in vertices)
    payload += b"".join(struct.pack("<4I", *f) for f in faces)
    for index in range(2):
        matrix = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                  index * 10, index * 20, index * 30, 1]
        payload += struct.pack("<4I16f", index * 4, 4, index * 2, 2, *matrix)
    return b"RLYMESH\0" + struct.pack("<10I", 1, 64, 8, 4, 2, zlib.crc32(payload),
                                       len(payload), 12, 16, 80) + bytes(16) + payload


class NativeMeshTests(unittest.TestCase):
    def test_bounded_loader_and_atomic_replacement(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-native-mesh-") as temporary:
            directory = Path(temporary)
            source = directory / "original.bin"
            source.write_bytes(fixture())
            executable = directory / "mesh-test"
            built = subprocess.run([
                compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                "-fno-fast-math", "-ffp-contract=off",
                "-I", str(ROOT / "platform/ps2/include"),
                str(ROOT / "platform/ps2/src/native_mesh.cpp"),
                str(ROOT / "platform/ps2/tests/native_mesh_test.cpp"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable), str(source), str(directory / "mutated.bin")],
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE MESH PASS", tested.stdout)


if __name__ == "__main__":
    unittest.main()
