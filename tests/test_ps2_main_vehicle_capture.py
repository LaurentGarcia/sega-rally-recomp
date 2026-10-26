#!/usr/bin/env python3
"""Source-only main-callback envelopes and sealed-reader failure regressions."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import main_vehicle_capture as reader
import test_ps2_driving_capture as observer_tests


class MainVehicleCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Exercise records emitted by the actual observer with an original
        # synthetic ordinary-RAM bus, never a copied live game snapshot.
        cls.harness=observer_tests.DrivingCaptureTests
        cls.harness.setUpClass()
        path,result=cls.harness().run_case("vehicle-valid","vehicle-reader-fixture")
        if result.returncode: raise AssertionError(result.stderr)
        cls.events=[json.loads(line) for line in (path/"main-vehicle.jsonl").read_text().splitlines()]
        cls.marker=json.loads((path/"capture.json").read_text())

    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()

    def fixture(self): return copy.deepcopy(self.events),copy.deepcopy(self.marker)

    def test_complete_actual_observer_records_group_without_numerical_claims(self):
        events,marker=self.fixture()
        groups=reader.vehicle_envelopes(events,marker)
        self.assertEqual(groups,[events])
        self.assertEqual(len(groups[0]),18)
        self.assertEqual([e["slot_index"] for e in groups[0] if e["event"]=="slot_pre"],list(range(4)))
        self.assertNotEqual(events[0]["registers"][2],events[0]["return_frame"]["pc"])

    def test_missing_reordered_duplicate_and_miscounted_envelopes_reject(self):
        variants=[]
        for index in (0,1,2,4,12,14,17):
            events,marker=self.fixture();del events[index];variants.append((events,marker))
        events,marker=self.fixture();events[4],events[5]=events[5],events[4];variants.append((events,marker))
        events,marker=self.fixture();events[3]["instructions"]=events[2]["instructions"];variants.append((events,marker))
        for key in ("vehicle_calls","vehicle_completions","vehicle_samples","vehicle_events",
                    "vehicle_prefix_entries","vehicle_slot_samples","vehicle_balance_returns","vehicle_selection_completions"):
            events,marker=self.fixture();marker[key]+=1;variants.append((events,marker))
        for index,(events,marker) in enumerate(variants):
            with self.subTest(index=index),self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)

    def test_proofs_parent_identity_raw_state_and_rounding_are_required(self):
        changes=[(0,"return_frame",None),(1,"parent_return_frame",{}),(3,"helper_frame_pointer",0x900000),
                 (4,"slot_index",4),(7,"object",0x210004),(12,"ac",0x40000000),(13,"object_words",[0]*63),
                 (14,"fp_bits",[0]*3),(15,"vehicle_frame_pointer",0x5ffff0),(17,"pc",0x5c9bb0)]
        for index,key,value in changes:
            events,marker=self.fixture();events[index][key]=value
            with self.subTest(index=index,key=key),self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)
        for mutation in ("words","source","cache","pfp","restored","slot-post-pfp","balance-post-pfp"):
            events,marker=self.fixture()
            if mutation=="words": events[3]["return_frame"]["words"][2]^=4
            if mutation=="source": events[3]["return_frame"]["source"]="unknown"
            if mutation=="cache": events[3]["return_frame"]["cache_index"]=4
            if mutation=="pfp": events[3]["registers"][0]+=64
            if mutation=="restored": events[4]["registers"][31]+=64
            if mutation=="slot-post-pfp": events[4]["registers"][0]+=64
            if mutation=="balance-post-pfp": events[16]["registers"][0]+=64
            with self.subTest(mutation=mutation),self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)

    def test_coherently_repointed_child_buffers_and_ownership_do_not_bypass_parent(self):
        events,marker=self.fixture()
        events[3]["registers"][17]+=4
        for event in events[3:5]:
            next(b for b in event["blocks"] if b["name"]=="slot_vector")["address"]+=4
        with self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)
        events,marker=self.fixture();events[11]["registers"][18]+=4
        for event in events[11:17]:
            for block in event["blocks"]:
                if block["name"]=="balance_output": block["address"]+=4
        with self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)
        for change in ("owner","callback","block","candidate","balance-frame","balance-proof"):
            events,marker=self.fixture()
            if change=="owner": events[0]["blocks"][0]["words"][0]+=256
            if change=="callback": events[0]["object_words"][1]+=4
            if change=="block": events[5]["blocks"].append(copy.deepcopy(events[5]["blocks"][0]))
            if change=="candidate": events[2]["registers"][17]+=4
            if change=="balance-frame": events[14]["registers"][31]+=64
            if change=="balance-proof": events[14]["return_frame"]["words"][3]^=1
            with self.subTest(change=change),self.assertRaises(ValueError): reader.vehicle_envelopes(events,marker)

    def test_sealed_reader_binds_stream_marker_checkpoint_hashes_and_private_path(self):
        events,marker=self.fixture()
        h=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(prefix="vehicle-reader-",dir=ROOT/"build") as temporary:
            parent=Path(temporary);capture=parent/"capture";capture.mkdir()
            marker_path=capture/"capture.json";stream=capture/"main-vehicle.jsonl";validation=parent/"validation.json"
            marker_path.write_text(json.dumps(marker));stream.write_text("".join(json.dumps(e)+"\n" for e in events))
            for name in ("baseline","captured"): (parent/(name+".csv")).write_text("header\ncheckpoint\n")
            report={"status":"passed","exact_checkpoint_match":True,"frames":1,"matched_frames":1,
                    "capture_summary":marker,"capture_files_sha256":{"capture.json":h(marker_path),"main-vehicle.jsonl":h(stream)},
                    "baseline_sha256":h(parent/"baseline.csv"),"captured_sha256":h(parent/"captured.csv")}
            validation.write_text(json.dumps(report))
            actual,actual_marker,hashes=reader.read_vehicle_capture(capture)
            self.assertEqual((actual,actual_marker),(events,marker));self.assertEqual(len(hashes),5)
            for key,value in (("status","failed"),("exact_checkpoint_match",False),("matched_frames",2),("capture_summary",{})):
                bad=copy.deepcopy(report);bad[key]=value;validation.write_text(json.dumps(bad))
                with self.subTest(key=key),self.assertRaises(ValueError): reader.read_vehicle_capture(capture)
            validation.write_text(json.dumps(report));stream.write_text(stream.read_text()+"\n")
            with self.assertRaises(ValueError): reader.read_vehicle_capture(capture)
            stream.write_text("".join(json.dumps(e)+"\n" for e in events));(parent/"captured.csv").write_text("changed\n")
            with self.assertRaises(ValueError): reader.read_vehicle_capture(capture)
            (parent/"captured.csv").unlink();(parent/"captured.csv").symlink_to(parent/"baseline.csv")
            with self.assertRaises(ValueError): reader.read_vehicle_capture(capture)
        with tempfile.TemporaryDirectory() as outside:
            with self.assertRaises(ValueError): reader.read_vehicle_capture(Path(outside))


if __name__=="__main__": unittest.main()
