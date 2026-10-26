#!/usr/bin/env python3
"""ROM-free candidate preparation and private-prefix parser regressions."""
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
import check_main_candidate as reader
import test_ps2_main_vehicle_capture as vehicle_fixture


class MainCandidatePortableTests(unittest.TestCase):
    def test_bounded_dyadic_preparation_matches_independent_arithmetic_and_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        manifest=json.loads((ROOT/"platform/ps2/tests/main_candidate_manifest.json").read_text())
        with tempfile.TemporaryDirectory(prefix="main-candidate-test-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror"]
            for directory in manifest["includes"]: command += ["-I",str(ROOT/directory)]
            result=subprocess.run(command+[str(ROOT/name) for name in manifest["sources"]]+["-o",str(executable)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(executable)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result.stdout.splitlines(),[
                "MAIN_CANDIDATE_PROBE PASS cases=1684 accepted=1641 consumed=971 hash=d782474a4f893dc8",
                "MAIN_CANDIDATE_HOST PASS cases=17044 accepted=16315 consumed=5807 hash=966c10f581152bec",
                "MAIN_CANDIDATE_REJECTION PASS cases=75"])


class MainCandidateParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # These records come from the observer's original synthetic RAM bus;
        # no private program image or captured game state enters the test.
        cls.harness=vehicle_fixture.MainVehicleCaptureTests
        cls.harness.setUpClass()
        cls.events,cls.marker=cls.harness().fixture()

    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()

    def fixture(self): return copy.deepcopy(self.events),copy.deepcopy(self.marker)

    @staticmethod
    def block(event,name): return next(b for b in event["blocks"] if b["name"]==name)

    def test_record_layout_keeps_both_raw_candidate_states_and_marker_provenance(self):
        events,marker=self.fixture();pre,post=events[1:3]
        pre["object_words"][5:20]=list(range(100,115))
        post["object_words"][5:20]=list(range(100,115))
        for index in (5,6,7,8,9,10,11,13): post["object_words"][index]+=1000
        for event,base in ((pre,200),(post,300)):
            self.block(event,"prefix_global")["words"]=[15]
            self.block(event,"candidate_position")["words"]=list(range(base,base+3))
            self.block(event,"candidate_orientation")["words"]=list(range(base+3,base+6))
        rows,provenance=reader.prefix_rows(events,marker)
        self.assertEqual(rows,[(
            100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
            15,200,201,202,203,204,205,
            1100,1101,1102,1103,1104,1105,1106,107,1108,109,110,111,112,113,114,
            15,300,301,302,303,304,305)])
        self.assertEqual(provenance,[{"call_id":pre["call_id"],"frame":pre["frame"],
            "object":pre["object"],"instructions":post["instructions"]-pre["instructions"],
            "input_marker":15,"output_marker":15}])

    def test_unrelated_storage_mutation_is_rejected_even_with_valid_parent_proof(self):
        # The output includes orientation and motion that must remain intact;
        # accepting a valid envelope alone would silently miss these writes.
        for index in (0,2,3,4,12,14,15,16,17,18,19,20,63):
            events,marker=self.fixture();events[2]["object_words"][index]^=1
            with self.subTest(index=index),self.assertRaisesRegex(ValueError,"unrelated object"):
                reader.prefix_rows(events,marker)

    def test_missing_wrong_sized_or_repointed_operand_blocks_reject(self):
        for name in ("prefix_global","candidate_position","candidate_orientation"):
            for event_index in (1,2):
                for defect in ("missing","address","size","duplicate"):
                    events,marker=self.fixture();event=events[event_index]
                    data=self.block(event,name)
                    if defect=="missing": event["blocks"].remove(data)
                    elif defect=="address": data["address"]+=4
                    elif defect=="size": data["words"].append(0)
                    else: event["blocks"].append(copy.deepcopy(data))
                    with self.subTest(name=name,event=event_index,defect=defect),self.assertRaises(ValueError):
                        reader.prefix_rows(events,marker)
        events,marker=self.fixture()
        events[2]["registers"][17]+=4
        for event in events[1:3]: self.block(event,"candidate_position")["address"]+=4
        with self.assertRaises(ValueError): reader.prefix_rows(events,marker)

    def test_prefix_evidence_requires_complete_ordered_and_saved_parent_boundaries(self):
        for defect in ("missing","order","counter","proof","rounding","object","pc"):
            events,marker=self.fixture()
            if defect=="missing": del events[2]
            elif defect=="order": events[2]["instructions"]=events[1]["instructions"]
            elif defect=="counter": marker["vehicle_prefix_completions"]+=1
            elif defect=="proof": events[2]["return_frame"]["words"][2]^=4
            elif defect=="rounding": events[1]["ac"]|=0x40000000
            elif defect=="object": events[1]["registers"][28]+=256
            else: events[2]["pc"]+=4
            with self.subTest(defect=defect),self.assertRaises(ValueError): reader.prefix_rows(events,marker)

    def test_raw_numerical_inputs_are_retained_for_domain_verification_not_filtered(self):
        # Numerical acceptance belongs to the independent original/native
        # comparison. The parser must not quietly omit an unsupported sample.
        events,marker=self.fixture()
        for event in events[1:3]: event["object_words"][5:8]=[0x7fc00001,0x7f800000,1]
        rows,provenance=reader.prefix_rows(events,marker)
        self.assertEqual(len(rows),1)
        self.assertEqual(rows[0][:3],(0x7fc00001,0x7f800000,1))
        self.assertEqual(rows[0][22:25],rows[0][:3])
        self.assertEqual(len(provenance),1)


if __name__=="__main__": unittest.main()
