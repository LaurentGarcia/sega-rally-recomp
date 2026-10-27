"""Original composed-step fixture and strict cross-stream caller regressions."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_main_contact_step as reader
import test_ps2_main_vehicle_capture as vehicle_fixture
from test_ps2_candidate_contact_pass import fixture as contact_fixture, LOCAL, named


class MainContactStepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=vehicle_fixture.MainVehicleCaptureTests
        cls.harness.setUpClass()
        cls.vehicle,cls.marker=cls.harness().fixture()

    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()

    def fixture(self):
        vehicle,marker=copy.deepcopy((self.vehicle,self.marker));contact,main_marker=contact_fixture()
        marker.update(main_marker)
        fp=vehicle[0]["vehicle_frame_pointer"]
        for index,event in enumerate(vehicle): event["instructions"]=(index+1)*100 if index<3 else 10000+100*index
        before,after=vehicle[1:3]
        for event in contact: event["object_words"][1]=0x5d3f40
        before["object_words"]=contact[0]["object_words"][:64].copy()
        after["object_words"]=before["object_words"].copy()
        for name,address in (("candidate_position",fp+0x130),("candidate_orientation",fp+0x140)):
            named(after,name)["words"]=named(contact[0],name)["words"].copy()
            for event in contact:
                for data in event["blocks"]:
                    if data["name"]==name: data["address"]=address
        proof=copy.deepcopy(contact[0]["return_frame"])
        proof["frame_pointer"]=fp;proof["words"]=after["registers"][:16].copy();proof["words"][2]=0x5d421c
        for index,event in enumerate(contact):
            event["frame"]=after["frame"];event["instructions"]=after["instructions"]+1+10*index
            event["parent_return_frame"]=copy.deepcopy(proof)
            if not event["event"].startswith(("query_","contact_")):
                event["return_frame"]=copy.deepcopy(proof)
                event["registers"][0]=fp
            if event["event"]=="main_return": event["registers"][31]=fp
        contact[0]["registers"][16:31]=after["registers"][16:31]
        return vehicle,contact,marker

    def test_native_authored_controls_and_late_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler required")
        manifest=json.loads((ROOT/"platform/ps2/tests/main_contact_step_manifest.json").read_text())
        with tempfile.TemporaryDirectory(prefix="main-contact-step-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off"]
            for path in manifest["includes"]:command += ["-I",str(ROOT/path)]
            built=subprocess.run(command+[str(ROOT/name) for name in manifest["sources"]]+["-o",str(executable)],capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            tested=subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertEqual(tested.stdout.splitlines(),[
                "MAIN_CONTACT_STEP_PROBE PASS controls=16 hash=76ae45f11adbe279",
                "MAIN_CONTACT_STEP_ATOMIC PASS rejections=4 fourth_contact_control=1"])

    def test_all_110_words_join_input_and_actual_final_state_without_intermediate_inputs(self):
        vehicle,contact,marker=self.fixture()
        rows,provenance=reader.step_rows(vehicle,contact,marker,LOCAL)
        prefix,_=reader.prefix_rows(vehicle,marker);candidate,_=reader.candidate_rows(contact,marker,LOCAL)
        row=rows[0]
        self.assertEqual(len(row),110)
        self.assertEqual(row[:22],prefix[0][:22])
        self.assertEqual(row[22:26],(302,303,301,0))
        self.assertEqual(row[26:46],tuple(list(range(13,18))*4))
        self.assertEqual(row[46:61],tuple(contact[-1]["object_words"][5:20]))
        self.assertEqual(row[61],prefix[0][37])
        self.assertEqual(row[62:68],(401,202,403,204,405,206))
        self.assertEqual(row[68:72],(502,503,501,601))
        self.assertEqual(row[72:92],tuple(list(range(30,35))*4))
        self.assertEqual(row[92:],tuple(LOCAL)+(2,17,101,102,103,1))
        self.assertNotEqual(provenance[0]["vehicle_call_id"],provenance[0]["contact_call_id"])
        self.assertIn("not sampled at return",provenance[0]["final_marker_source"])
        self.assertIn("not a sampled input",provenance[0]["initial_selected_scalar_source"])

    def test_individually_valid_streams_with_wrong_call_transition_cannot_join(self):
        for defect in ("gap","frame","globals","savedlocal","object","candidate","extent"):
            vehicle,contact,marker=self.fixture()
            if defect=="gap":
                for e in contact:e["instructions"]+=1
            elif defect=="frame":
                for e in contact:e["frame"]+=1
            elif defect=="globals": contact[0]["registers"][20]^=1
            elif defect=="savedlocal":
                for e in contact:
                    e["parent_return_frame"]["words"][4]^=1
                    if not e["event"].startswith(("query_","contact_")): e["return_frame"]["words"][4]^=1
            elif defect=="object":
                for e in contact:e["object_words"][3]^=1
            elif defect=="candidate":
                for e in contact:
                    for b in e["blocks"]:
                        if b["name"]=="candidate_position":b["words"][1]+=1
            else: contact[-1]["instructions"]=vehicle[3]["instructions"]
            # These retain each component's own structural contract. Only the
            # cross-stream caller witness establishes this is the same call.
            reader.prefix_rows(vehicle,marker);reader.candidate_rows(contact,marker,LOCAL)
            with self.subTest(defect=defect),self.assertRaises(ValueError):reader.step_rows(vehicle,contact,marker,LOCAL)

    def test_incomplete_and_repointed_caller_population_rejects(self):
        for defect in ("missing","count","parent","output"):
            vehicle,contact,marker=self.fixture()
            if defect=="missing":contact.pop()
            elif defect=="count":marker["main_samples"]+=1
            elif defect=="parent":contact[0]["return_frame"]["words"][2]^=4
            else:named(contact[-1],"candidate_orientation")["address"]+=4
            with self.subTest(defect=defect),self.assertRaises(ValueError):reader.step_rows(vehicle,contact,marker,LOCAL)

    def test_both_stream_identities_and_observer_parity_are_required(self):
        vehicle,contact,marker=self.fixture();h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(prefix="joined-reader-",dir=ROOT/"build") as temporary:
            parent=Path(temporary);directory=parent/"capture";directory.mkdir()
            (directory/"capture.json").write_text(json.dumps(marker))
            for name,events in (("main-vehicle",vehicle),("main-contact",contact)):
                (directory/(name+".jsonl")).write_text("".join(json.dumps(e)+"\n" for e in events))
            for name in ("baseline","captured"):(parent/(name+".csv")).write_text("header\ncheckpoint\n")
            report={"status":"passed","exact_checkpoint_match":True,"frames":1,"matched_frames":1,
                "capture_summary":marker,"capture_files_sha256":{p.name:h(p) for p in directory.iterdir()},
                "baseline_sha256":h(parent/"baseline.csv"),"captured_sha256":h(parent/"captured.csv")}
            validation=parent/"validation.json";validation.write_text(json.dumps(report))
            rows,_,hashes=reader.read_capture(directory,LOCAL)
            self.assertEqual((len(rows),len(hashes)),(1,6))
            for name in ("main-vehicle.jsonl","main-contact.jsonl"):
                path=directory/name;original=path.read_text();path.write_text(original+"\n")
                with self.subTest(stream=name),self.assertRaises(ValueError):reader.read_capture(directory,LOCAL)
                path.write_text(original)
            report["status"]="failed";validation.write_text(json.dumps(report))
            with self.assertRaises(ValueError):reader.read_capture(directory,LOCAL)


if __name__=="__main__":unittest.main()
