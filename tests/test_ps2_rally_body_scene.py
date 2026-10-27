"""Original body geometry exercises fixed-world movement and resource ownership."""
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
from export_rally_body import body_from_verified_mesh

class RallyBodyScene(unittest.TestCase):
    def test_native_ownership_and_fixed_world_updates(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="rally-body-") as directory:
            executable=Path(directory)/"test"
            sources=["native/rally_body_scene.cpp","native/contact_point_preparation.cpp","native/rounded_affine_transform.cpp",
                     "src/world_scene.cpp","src/native_geometry.cpp","src/native_geometry_float.cpp","tests/rally_body_scene_test.cpp"]
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/native"),"-I",str(ROOT/"platform/ps2/include"),
                *(str(ROOT/"platform/ps2"/name) for name in sources),"-o",str(executable)]
            subprocess.run(command,capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertRegex(result.stdout,r"^RALLY_BODY PASS checks=\d+ fixed_translation_pixels=62 course_stationary=1 no_update_allocations=1\n$")

    def test_export_excludes_every_captured_matrix(self):
        vertices=struct.pack("<9f",0,0,0,1,0,0,0,1,0)
        face=struct.pack("<4I",0,1,2,0xff123456)
        def source(matrix):
            payload=vertices+face+struct.pack("<4I16f",0,3,0,1,*matrix)
            return struct.pack("<8s10I16x",b"RLYMESH\0",1,64,3,1,1,zlib.crc32(payload),len(payload),12,16,80)+payload
        first=body_from_verified_mesh(source([1.0]*16));second=body_from_verified_mesh(source([99.0]*16))
        self.assertEqual(first,second);self.assertEqual(first[64:],vertices+face);self.assertEqual(len(first),116)
        with self.assertRaises(ValueError):body_from_verified_mesh(source([1.0]*16)+b"x")
        malformed=bytearray(source([1.0]*16));struct.pack_into("<I",malformed,24,2)
        with self.assertRaises(ValueError):body_from_verified_mesh(malformed)

if __name__=="__main__":unittest.main()
