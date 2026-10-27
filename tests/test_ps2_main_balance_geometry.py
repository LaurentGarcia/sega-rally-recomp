#!/usr/bin/env python3
"""Native balance geometry and strict additive capture parsing."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_main_balance_geometry as reader
import test_ps2_main_motion_capture as motion_fixture

class MainGeometryPortableTests(unittest.TestCase):
    def test_original_boundary_vectors_and_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler required")
        manifest=json.loads((ROOT/"platform/ps2/tests/main_balance_geometry_manifest.json").read_text())
        with tempfile.TemporaryDirectory(prefix="balance-geometry-test-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off"]
            for directory in manifest["includes"]:command.extend(("-I",str(ROOT/directory)))
            built=subprocess.run(command+[str(ROOT/p) for p in manifest["sources"]]+["-o",str(executable)],text=True,capture_output=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],text=True,capture_output=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertEqual(run.stdout.splitlines(),[
                "MAIN_BALANCE_GEOMETRY_PROBE PASS cases=1504 hash=3e2a507aab74e9d4",
                "MAIN_BALANCE_GEOMETRY_REJECTION PASS checks=84 known_controls=2",
                "MAIN_BALANCE_GEOMETRY_HOST PASS cases=20480 hash=56b32fd92694a6a0"])

class MainGeometryParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=motion_fixture.MainMotionCaptureTests
        cls.harness.setUpClass();cls.motion,cls.vehicle,cls.marker=cls.harness().fixture()
        pre,post=cls.motion[2:4]
        for event in (pre,post):event["registers"][30]=0
        for register in (3,9,10,15):post["registers"][register]=pre["registers"][register]
        cls.block(post,"motion_prior")["words"]=[pre["registers"][10]]
    @classmethod
    def tearDownClass(cls):cls.harness.tearDownClass()
    @staticmethod
    def block(event,name):return next(b for b in event["blocks"] if b["name"]==name)
    def fixture(self):return copy.deepcopy((self.motion,self.vehicle,self.marker))
    def test_complete_raw_row_mapping_and_parent_provenance(self):
        m,v,c=self.fixture();rows,proof=reader.geometry_rows(m,v,c)
        self.assertEqual(len(rows),1);self.assertEqual(len(rows[0]),23)
        regs=m[2]["registers"]
        self.assertEqual(rows[0][:6],tuple(regs[i] for i in (6,8,5,4,14,13)))
        self.assertEqual(rows[0][6:9],(m[2]["object_words"][29],m[2]["object_words"][15],regs[10]))
        self.assertEqual(rows[0][9:12],tuple(self.block(m[2],"balance_vector")["words"]))
        self.assertEqual(rows[0][12:15],tuple(self.block(m[2],"balance_output")["words"]))
        self.assertEqual(rows[0][16:19],tuple(self.block(m[3],"balance_vector")["words"]))
        self.assertEqual(rows[0][19:22],tuple(self.block(m[3],"balance_output")["words"]))
        self.assertEqual(proof[0]["selected_index"],regs[10])
    def test_modified_object_pointer_index_zero_and_parent_proof_reject(self):
        for mutation in ("object","vector","output","identity","index","published","zero","proof","count","order"):
            m,v,c=self.fixture();post=m[3]
            if mutation=="object":post["object_words"][29]^=1
            if mutation=="vector":post["registers"][3]+=4
            if mutation=="output":post["registers"][15]+=4
            if mutation=="identity":post["registers"][9]+=256
            if mutation=="index":post["registers"][10]^=1
            if mutation=="published":self.block(post,"motion_prior")["words"][0]^=1
            if mutation=="zero":m[2]["registers"][30]=1
            if mutation=="proof":post["return_frame"]["words"][2]^=4
            if mutation=="count":c["motion_tail_samples"]+=1
            if mutation=="order":m[2],m[3]=m[3],m[2]
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):reader.geometry_rows(m,v,c)
    def test_numerical_domain_and_outputs_are_retained_for_original_oracle(self):
        m,v,c=self.fixture();m[2]["registers"][6]=0x7f800001
        self.block(m[3],"balance_vector")["words"]=[1,0x80000000,0x7fc12345]
        rows,_=reader.geometry_rows(m,v,c)
        self.assertEqual(rows[0][0],0x7f800001)
        self.assertEqual(rows[0][16:19],(1,0x80000000,0x7fc12345))
        for word,negative in ((0,False),(0x80000000,False),(0x80000001,True),(0xbf800000,True),(0x3f800000,False)):
            m,v,c=self.fixture();m[2]["registers"][6]=word
            self.assertEqual(reader.geometry_rows(m,v,c)[1][0]["negative_first_longitudinal"],negative)

if __name__=="__main__":unittest.main()
