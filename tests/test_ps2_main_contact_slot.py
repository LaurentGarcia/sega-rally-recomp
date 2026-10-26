#!/usr/bin/env python3
"""Original native slot arithmetic and bounded captured-call parsing."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_main_contact_slot as reader
import test_ps2_main_vehicle_capture as vehicle_fixture


class MainSlotPortableTests(unittest.TestCase):
    def test_branch_rounding_preservation_and_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="main-slot-test-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                     "-I",str(ROOT/"platform/ps2/native"),"-I",str(ROOT/"platform/ps2/tests")]
            sources=["platform/ps2/tests/main_contact_slot_test.cpp","platform/ps2/native/main_contact_slot.cpp",
                     "platform/ps2/native/recovered_scalar_metric.cpp"]
            built=subprocess.run(command+[str(ROOT/name) for name in sources]+["-o",str(executable)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertEqual(run.stdout.splitlines(),[
                "MAIN_CONTACT_SLOT_PROBE PASS cases=2876 active=947 hash=32ef92cd4f0d4583",
                "MAIN_CONTACT_SLOT_REJECTION PASS checks=316 known_controls=2",
                "MAIN_CONTACT_SLOT_HOST PASS cases=25852 active=7248 hash=121b47e419609ea7"])


class MainSlotParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=vehicle_fixture.MainVehicleCaptureTests
        cls.harness.setUpClass()
        cls.events,cls.marker=cls.harness().fixture()
        # Original synthetic observer values: supply only the helper's known
        # register/preservation invariants, not captured game input bytes.
        for index in range(4):
            pre,post=cls.events[3+2*index:5+2*index]
            post["registers"][16]=index&1;post["registers"][30]=0
            post["registers"][24:29]=pre["registers"][24:29]
    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()
    def fixture(self): return copy.deepcopy(self.events),copy.deepcopy(self.marker)
    @staticmethod
    def block(event,name): return next(b for b in event["blocks"] if b["name"]==name)

    def test_rows_bind_raw_inputs_and_full_state_in_exact_order(self):
        events,marker=self.fixture();rows,provenance=reader.slot_rows(events,marker)
        self.assertEqual(len(rows),4);self.assertTrue(all(len(row)==69 for row in rows))
        self.assertEqual([row[0] for row in rows],[0,1,2,3])
        self.assertEqual([row[-1] for row in rows],[0,1,0,1])
        pre=events[3];result=self.block(pre,"slot_result")["words"]
        self.assertEqual(rows[0][:6],tuple([0,result[7],result[6],*result[3:6]]))
        self.assertEqual(rows[0][12:15],(self.block(pre,"slot_triple")["words"][1],0x3e800000,pre["object_words"][20]))
        self.assertEqual(rows[0][21],pre["object_words"][4]>>16)
        self.assertEqual(rows[0][23:27],tuple(pre["object_words"][25:29]))
        self.assertEqual([p["slot"] for p in provenance],[0,1,2,3])

    def test_rejects_modified_input_and_unrelated_storage(self):
        for field in ("slot_vector","slot_triple","slot_direction","slot_result","slot_parameters","slot_control"):
            events,marker=self.fixture();self.block(events[4],field)["words"][0]^=1
            with self.subTest(field=field),self.assertRaises(ValueError): reader.slot_rows(events,marker)
        for field in ("object","low-halfword","other-history","other-global","return","preserved","zero","proof","count"):
            events,marker=self.fixture();post=events[4]
            if field=="object":post["object_words"][20]^=1
            if field=="low-halfword":post["object_words"][4]^=1
            if field=="other-history":post["object_words"][26]^=1
            if field=="other-global":self.block(post,"slot_globals")["words"][1]^=1
            if field=="return":post["registers"][16]=2
            if field=="preserved":post["registers"][24]^=1
            if field=="zero":post["registers"][30]=1
            if field=="proof":post["return_frame"]["words"][2]^=4
            if field=="count":marker["vehicle_slot_samples"]+=1
            with self.subTest(field=field),self.assertRaises(ValueError): reader.slot_rows(events,marker)

    def test_output_fields_are_compared_by_oracle_without_parser_reinterpretation(self):
        events,marker=self.fixture();post=events[4]
        post["object_words"][4]=(0xfedc<<16)|(post["object_words"][4]&65535)
        post["object_words"][25]=0x7fc12345
        for i in (0,4,8,12):self.block(post,"slot_globals")["words"][i]=0x80000000+i
        self.block(post,"slot_output")["words"]=[0x80000000,0x7fc12345,1]
        rows,_=reader.slot_rows(events,marker)
        self.assertEqual(rows[0][43],0xfedc);self.assertEqual(rows[0][45],0x7fc12345)
        self.assertEqual(rows[0][-4:-1],(0x80000000,0x7fc12345,1))


if __name__=="__main__": unittest.main()
