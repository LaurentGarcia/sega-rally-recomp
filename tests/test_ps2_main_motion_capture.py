#!/usr/bin/env python3
"""Original synthetic motion records: parent binding and sealed input failures."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import main_motion_capture as reader
import test_ps2_driving_capture as observer_tests


class MainMotionCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=observer_tests.DrivingCaptureTests
        cls.harness.setUpClass()
        path,result=cls.harness().run_case("vehicle-motion-valid","motion-reader-fixture")
        if result.returncode: raise AssertionError(result.stderr)
        cls.motion=[json.loads(line) for line in (path/"main-motion.jsonl").read_text().splitlines()]
        cls.vehicle=[json.loads(line) for line in (path/"main-vehicle.jsonl").read_text().splitlines()]
        cls.marker=json.loads((path/"capture.json").read_text())

    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()

    def fixture(self): return copy.deepcopy((self.motion,self.vehicle,self.marker))

    def test_original_observer_groups_bind_to_unchanged_vehicle_format(self):
        motion,vehicle,marker=self.fixture()
        self.assertEqual(reader.motion_envelopes(motion,vehicle,marker),[motion])
        self.assertEqual(len(vehicle),18)
        self.assertEqual(motion[0]["return_frame"]["pc"],0x5d3fc8)
        self.assertEqual(motion[2]["return_frame"],vehicle[11]["return_frame"])

    def test_missing_reordered_duplicate_and_wrong_completion_counts_reject(self):
        variants=[]
        for index in range(4):
            m,v,c=self.fixture();del m[index];variants.append((m,v,c))
        m,v,c=self.fixture();m[0],m[1]=m[1],m[0];variants.append((m,v,c))
        m,v,c=self.fixture();m[3]["instructions"]=m[2]["instructions"];variants.append((m,v,c))
        for key in ("motion_events","motion_segment_entries","motion_segment_completions",
                    "motion_segment_samples","motion_tail_entries","motion_tail_completions","motion_tail_samples"):
            m,v,c=self.fixture();c[key]+=1;variants.append((m,v,c))
        for index,args in enumerate(variants):
            with self.subTest(index=index),self.assertRaises(ValueError): reader.motion_envelopes(*args)

    def test_child_parent_and_saved_return_proofs_cannot_be_coherently_repointed(self):
        for defect in ("parent","call","object","frame","helper","words","source","pfp","restored","tailproof"):
            m,v,c=self.fixture()
            if defect=="parent": m[0]["parent_return_frame"]["words"][3]^=1
            elif defect=="call": m[0]["call_id"]+=1
            elif defect=="object": m[0]["object"]+=256
            elif defect=="frame": m[0]["frame"]+=1
            elif defect=="helper": m[3]["helper_frame_pointer"]+=64
            elif defect=="words": m[0]["return_frame"]["words"][2]^=4
            elif defect=="source": m[0]["return_frame"]["source"]="unverified"
            elif defect=="pfp": m[0]["registers"][0]+=64
            elif defect=="restored": m[1]["registers"][0]+=64
            else: m[2]["return_frame"]["words"][3]^=1
            with self.subTest(defect=defect),self.assertRaises(ValueError): reader.motion_envelopes(m,v,c)
        m,v,c=self.fixture()
        m[0]["registers"][16]+=4
        for e in m[:2]: next(b for b in e["blocks"] if b["name"]=="segment_position")["address"]+=4
        with self.assertRaises(ValueError): reader.motion_envelopes(m,v,c)
        m,v,c=self.fixture();m[2]["registers"][3]+=4
        for e in m[2:]: next(b for b in e["blocks"] if b["name"]=="balance_vector")["address"]+=4
        with self.assertRaises(ValueError): reader.motion_envelopes(m,v,c)

    def test_raw_operand_blocks_rounding_and_parent_interval_are_required(self):
        for defect in ("size","word","alias","device","duplicate","rounding","before","after","callback"):
            m,v,c=self.fixture()
            if defect=="size": m[2]["fp_bits"].pop()
            elif defect=="word": m[2]["registers"][0]=-1
            elif defect=="alias": next(b for b in m[1]["blocks"] if b["name"]=="segment_indices")["words"][0]^=1
            elif defect=="device": m[2]["blocks"][1]["address"]=0x900000
            elif defect=="duplicate": m[2]["blocks"][2]=copy.deepcopy(m[2]["blocks"][1])
            elif defect=="rounding": m[3]["ac"]|=0x40000000
            elif defect=="before": m[2]["instructions"]=v[15]["instructions"]
            elif defect=="after": m[1]["instructions"]=v[1]["instructions"]
            else: m[2]["object_words"][1]+=4
            with self.subTest(defect=defect),self.assertRaises(ValueError): reader.motion_envelopes(m,v,c)

    def test_additive_reader_requires_its_own_sealed_stream_hash_and_rejects_redirects(self):
        motion,vehicle,marker=self.fixture()
        h=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(prefix="motion-reader-",dir=ROOT/"build") as temporary:
            parent=Path(temporary);capture=parent/"capture";capture.mkdir()
            (capture/"capture.json").write_text(json.dumps(marker))
            for name,events in (("main-vehicle",vehicle),("main-motion",motion)):
                (capture/(name+".jsonl")).write_text("".join(json.dumps(e)+"\n" for e in events))
            for name in ("baseline","captured"): (parent/(name+".csv")).write_text("header\ncheckpoint\n")
            report={"status":"passed","exact_checkpoint_match":True,"frames":1,"matched_frames":1,
                    "capture_summary":marker,"capture_files_sha256":{p.name:h(p) for p in capture.iterdir()},
                    "baseline_sha256":h(parent/"baseline.csv"),"captured_sha256":h(parent/"captured.csv")}
            validation=parent/"validation.json";validation.write_text(json.dumps(report))
            actual,parents,actual_marker,hashes=reader.read_motion_capture(capture)
            self.assertEqual((actual,parents,actual_marker),(motion,vehicle,marker));self.assertEqual(len(hashes),6)
            stream=capture/"main-motion.jsonl";original=stream.read_text()
            stream.write_text(original+"\n")
            with self.assertRaises(ValueError): reader.read_motion_capture(capture)
            stream.write_text(original)
            del report["capture_files_sha256"][stream.name];validation.write_text(json.dumps(report))
            with self.assertRaises(ValueError): reader.read_motion_capture(capture)
            stream.unlink();stream.symlink_to(capture/"main-vehicle.jsonl")
            with self.assertRaises(ValueError): reader.read_motion_capture(capture)


if __name__=="__main__": unittest.main()
