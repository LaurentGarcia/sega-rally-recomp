#!/usr/bin/env python3
"""Original synthetic vectors for the private offline mesh pipeline."""
import copy
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("native_mesh_export", ROOT / "platform/ps2/tools/export_native_mesh.py")
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)


def original_object():
    def bits(f):
        return struct.unpack("<I", struct.pack("<f", f))[0]
    return {"object": 7, "points": [[bits(x), bits(y), bits(z)] for x,y,z in
                                    [(1,2,3), (4,5,6), (7,8,9)]],
            "faces": [[2,0,1,0x12345678]],
            "matrix_bits": [bits(x) for x in (1,0,0,0,1,0,0,0,1,10,20,30)]}


class MeshExportTests(unittest.TestCase):
    def test_wire_order_indices_crc_and_unbaked_transform(self):
        first = original_object()
        second = copy.deepcopy(first)
        second["object"] = 8
        blob, report = EXPORT.native_payload([first, second])
        header = struct.unpack("<8s10I16x", blob[:64])
        self.assertEqual(header[:6], (b"RLYMESH\0", 1, 64, 6, 2, 2))
        self.assertEqual(header[6], zlib.crc32(blob[64:]))
        self.assertEqual(header[7:], (264, 12, 16, 80))
        self.assertEqual(struct.unpack_from("<3f", blob,64), (1,2,3))
        self.assertEqual(struct.unpack_from("<4I", blob,64+72), (2,0,1,0xffa6c9e8))
        self.assertEqual(struct.unpack_from("<4I", blob,64+72+16), (5,3,4,0xffa6c9e8))
        obj = struct.unpack_from("<4I16f", blob,64+72+32)
        self.assertEqual(obj[:4], (0,3,0,1))
        self.assertEqual(obj[4:], (1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1))
        self.assertEqual(report["objects"],2)

    def test_domain_and_index_rejections(self):
        for word in (1,0x80000001,0x7fc00000,0x7f800000,0x47800001):
            obj = original_object()
            obj["points"][0][0] = word
            with self.assertRaises(ValueError): EXPORT.native_payload([obj])
        for indices in ([0,0,1,0],[0,1,3,0],[-1,1,2,0]):
            obj = original_object(); obj["faces"] = [indices]
            with self.assertRaises(ValueError): EXPORT.native_payload([obj])
        obj = original_object(); obj["matrix_bits"][0] = 1
        with self.assertRaises(ValueError): EXPORT.native_payload([obj])
        with self.assertRaises(ValueError): EXPORT.native_payload([])

    def test_raw_source_metadata_linkage(self):
        # One independently specified quad followed by a normal-mode terminator.
        point = lambda x: [struct.unpack("<I",struct.pack("<f",x))[0],0,0]
        words = [*point(0),*point(1),0x201,11,22,33,*point(2),*point(3),0,44,55,66]
        raw = struct.pack(f"<{len(words)}I",*words)
        obj = {"declared_links": 9,"mode": 0,"points": [point(i) for i in range(4)],
               "faces": [[1,0,2,0x201],[1,2,3,0x201]],"links": 1,
               "hidden_links": 0,"degenerate_faces": 0,"terminated": True}
        EXPORT.verify_decoded_source(obj,raw)
        for key,value in (("points",[point(i+1) for i in range(4)]),
                          ("faces",[[0,1,2,0x201],[1,2,3,0x201]]),
                          ("hidden_links",1),("mode",2),("declared_links",1)):
            altered = dict(obj); altered[key] = value
            with self.assertRaises(ValueError): EXPORT.verify_decoded_source(altered,raw)
        with self.assertRaises(ValueError): EXPORT.verify_decoded_source(obj,raw[:-4])
        with self.assertRaises(ValueError): EXPORT.verify_decoded_source(obj,raw+b"\0\0\0\0")

    def test_strip_topology_and_reader_bounds(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-mesh-decode-") as temp:
            exe = str(Path(temp) / "test")
            run = subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                                  "-I",str(ROOT / "platform/ps2/tools"),
                                  str(ROOT / "platform/ps2/tools/mesh_decode.cpp"),
                                  str(ROOT / "platform/ps2/tests/mesh_decode_test.cpp"),"-o",exe],
                                 capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            run = subprocess.run([exe],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("OFFLINE_MESH_DECODE PASS",run.stdout)


if __name__ == "__main__": unittest.main()
