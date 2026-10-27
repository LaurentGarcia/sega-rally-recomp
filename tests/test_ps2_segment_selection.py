"""Original segment-selection corpus and strict private-record parser checks."""
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
import check_segment_selection as reader
import test_ps2_main_motion_capture as motion_fixture


def named(event,name):return next(b for b in event["blocks"] if b["name"]==name)


class SegmentSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=motion_fixture.MainMotionCaptureTests
        cls.harness.setUpClass()
        cls.motion,cls.vehicle,cls.marker=cls.harness().fixture()

    @classmethod
    def tearDownClass(cls):cls.harness.tearDownClass()

    def fixture(self):
        motion,vehicle,marker=copy.deepcopy((self.motion,self.vehicle,self.marker))
        for event in motion[:2]:named(event,"course_selector")["words"]=[2]
        return motion,vehicle,marker

    def test_native_host_probe_boundary_order_and_rejection_corpus(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="segment-selection-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            sources=["platform/ps2/tests/segment_selection_test.cpp","platform/ps2/native/segment_selection.cpp","platform/ps2/native/contact_projection.cpp"]
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off",
                     "-I",str(ROOT/"platform/ps2/native"),*[str(ROOT/name) for name in sources],"-o",str(executable)]
            built=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            tested=subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertEqual(tested.stdout.splitlines(),[
                "SEGMENT_SELECTION_PROBE PASS cases=3634 hash=1c1c549c187f8285",
                "SEGMENT_SELECTION_REJECTION PASS checks=55 controls=2",
                "SEGMENT_SELECTION_HOST PASS cases=69170 hash=b4ab429abeb08285"])

    def test_raw_point_progression_and_unclamped_miss_mapping(self):
        motion,vehicle,marker=self.fixture()
        before=motion[0]["object_words"]
        rows,provenance=reader.segment_rows(motion,vehicle,marker)
        self.assertEqual(rows,[(2,before[5],before[7],before[34]>>16,0xffffffff,0xffff,0xffff)])
        self.assertFalse(provenance[0]["matched"])
        motion[1]["object_words"][34]=(12<<16)|11
        named(motion[1],"segment_indices")["words"]=[(12<<16)|11]
        motion[1]["registers"][16]=11
        rows,provenance=reader.segment_rows(motion,vehicle,marker)
        self.assertEqual(rows[0][-3:],(11,11,12));self.assertTrue(provenance[0]["matched"])

    def test_selector_point_and_unrelated_object_preservation_are_required(self):
        for defect in ("selector","changed-selector","point-domain","point-y","unrelated","block"):
            motion,vehicle,marker=self.fixture()
            if defect=="selector":
                for e in motion[:2]:named(e,"course_selector")["words"]=[8]
            elif defect=="changed-selector":named(motion[1],"course_selector")["words"]=[1]
            elif defect=="point-domain":
                for e in motion[:2]:
                    e["object_words"][5]=0x7f800000;named(e,"segment_position")["words"][0]=0x7f800000
            elif defect=="point-y":
                motion[1]["object_words"][6]^=1;named(motion[1],"segment_position")["words"][1]^=1
            elif defect=="unrelated":motion[1]["object_words"][63]^=1
            else:named(motion[1],"segment_indices")["words"][0]=0
            with self.subTest(defect=defect),self.assertRaises(ValueError):reader.segment_rows(motion,vehicle,marker)

    def test_return_encoding_saved_parent_and_completion_cannot_be_forged(self):
        for defect in ("return","miss-progression","large-index","proof","count","boundary"):
            motion,vehicle,marker=self.fixture()
            if defect=="return":motion[1]["registers"][16]=0
            elif defect=="miss-progression":
                motion[1]["object_words"][34]=0xffff;named(motion[1],"segment_indices")["words"]=[0xffff]
            elif defect=="large-index":motion[1]["registers"][16]=65535
            elif defect=="proof":motion[0]["return_frame"]["words"][2]^=4
            elif defect=="count":marker["motion_segment_samples"]+=1
            else:motion[1]["pc"]+=4
            with self.subTest(defect=defect),self.assertRaises(ValueError):reader.segment_rows(motion,vehicle,marker)


if __name__=="__main__":unittest.main()
