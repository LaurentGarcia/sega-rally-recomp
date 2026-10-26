"""Original native full-pass fixture and synthetic private-record validation."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
try:
    import check_candidate_contact_pass as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_pass_response import fixture as response_fixture
from test_ps2_contact_query import block

LOCAL = [0x3f800000,0,0]*4


def fixture():
    events,marker=response_fixture()
    old=list(range(10,18))+[999]
    for event in events:
        event["object_words"][34]=17
        event["blocks"]=[b for b in event["blocks"] if b["name"]!="course_selector"]
        event["blocks"].append(block("course_selector",0x214354,[2]))
        kind=event["event"]
        if kind in ("main_entry","main_prequery","main_return"):
            slots=old*4
            if kind=="main_return":
                for i in range(4):slots[9*i+3:9*i+8]=range(30,35)
            event["blocks"].append(block("result_slots",0x2138d0,slots))
        if kind in ("main_entry","main_prequery"):
            event["blocks"].append(block("local_contact_points",0x5d2e30,list(LOCAL)))
        if kind=="main_prequery":
            event["registers"][18]=event["main_frame_pointer"]+0x40
            event["blocks"].append(block("query_points",event["main_frame_pointer"]+0x40,list(range(12))))
    return events,marker


def named(event,name):
    return next(b for b in event["blocks"] if b["name"]==name)


class CandidateContactPassTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): (ROOT/"build").mkdir(exist_ok=True)

    def test_native_fixture_hits_misses_composition_and_late_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler required")
        manifest=json.loads((ROOT/"platform/ps2/tests/candidate_contact_pass_manifest.json").read_text())
        with tempfile.TemporaryDirectory(prefix="native-candidate-pass-") as directory:
            executable=Path(directory)/"candidate"
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off",
                     *[arg for path in manifest["includes"] for arg in ("-I",str(ROOT/path))],
                     *(str(ROOT/name) for name in manifest["sources"]),"-o",str(executable)]
            built=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            tested=subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertIn("CANDIDATE_CONTACT_PASS_PROBE PASS cases=768 active=512 misses=256 hash=b8ed3cad65bf6be4",tested.stdout)
            self.assertIn("CANDIDATE_CONTACT_PASS_GUARDS PASS known_geometry=1 atomic_rejections=6",tested.stdout)

    def test_record_adds_local_selector_segment_and_original_slot_inputs(self):
        events,marker=fixture()
        rows,provenance=CHECKER.candidate_rows(events,marker,LOCAL)
        base,_=CHECKER.pass_rows(events,marker)
        self.assertEqual(rows,[base[0]+tuple(LOCAL)+(2,17)+tuple(list(range(13,18))*4)])
        self.assertEqual(len(rows[0]),99)
        self.assertEqual(provenance[0]["call_id"],7)

    def test_local_asset_and_preparation_parent_inputs_cannot_be_replaced(self):
        original,marker=fixture()
        changes=[lambda e:named(e[0],"local_contact_points")["words"].__setitem__(0,0),
                 lambda e:named(e[1],"local_contact_points")["words"].__setitem__(0,0),
                 lambda e:named(e[1],"candidate_orientation")["words"].__setitem__(0,99),
                 lambda e:e[1]["return_frame"]["words"].__setitem__(2,0x5d4218),
                 lambda e:e[1]["registers"].__setitem__(18,0x900000),
                 lambda e:named(e[1],"result_slots")["words"].__setitem__(3,99)]
        for i,change in enumerate(changes):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=i),self.assertRaises(ValueError):CHECKER.candidate_rows(events,marker,LOCAL)
        with self.assertRaises(ValueError):CHECKER.candidate_rows(original,marker,LOCAL[:-1])

    def test_course_and_signed_segment_must_remain_unchanged_and_bounded(self):
        original,marker=fixture()
        for value in (8,0xffffffff):
            events=copy.deepcopy(original)
            for e in events:named(e,"course_selector")["words"]=[value]
            with self.subTest(selector=value),self.assertRaises(ValueError):CHECKER.candidate_rows(events,marker,LOCAL)
        for value in (0x8000,0xffff):
            events=copy.deepcopy(original)
            for e in events:e["object_words"][34]=value
            with self.subTest(segment=value),self.assertRaises(ValueError):CHECKER.candidate_rows(events,marker,LOCAL)
        for index in (1,len(original)-1):
            for field in ("selector","segment"):
                events=copy.deepcopy(original)
                if field=="selector":named(events[index],"course_selector")["words"]=[1]
                else:events[index]["object_words"][34]=18
                with self.subTest(event=index,field=field),self.assertRaises(ValueError):CHECKER.candidate_rows(events,marker,LOCAL)

    def test_prior_adapter_state_and_final_preserved_slot_words_are_bound(self):
        original,marker=fixture()
        pre=next(i for i,e in enumerate(original) if e["event"]=="contact_pre")
        events=copy.deepcopy(original)
        named(events[pre],"contact_output")["words"][3]=999
        named(events[pre+1],"contact_output")["words"][3]=999
        with self.assertRaisesRegex(ValueError,"prior state"):CHECKER.candidate_rows(events,marker,LOCAL)
        for index in (0,3,7,8,12,17,35):
            events=copy.deepcopy(original)
            named(events[-1],"result_slots")["words"][index]^=1
            with self.subTest(word=index),self.assertRaises(ValueError):CHECKER.candidate_rows(events,marker,LOCAL)
        events=copy.deepcopy(original);events.pop()
        with self.assertRaises(ValueError):CHECKER.candidate_rows(events,dict(marker,main_events=len(events)),LOCAL)

    def test_reader_rejects_changed_private_files_and_unverified_assets(self):
        with tempfile.TemporaryDirectory(prefix="candidate-proof-",dir=ROOT/"build") as directory:
            parent=Path(directory);capture=parent/"capture";capture.mkdir()
            events,marker=fixture();stream=capture/"main-contact.jsonl";metadata=capture/"capture.json"
            stream.write_text("".join(json.dumps(e)+"\n" for e in events));metadata.write_text(json.dumps(marker))
            validation={"status":"passed","exact_checkpoint_match":True,
                        "capture_files_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (stream,metadata)}}
            report=parent/"validation.json";report.write_text(json.dumps(validation))
            rows,_,hashes=CHECKER.read_capture(capture,LOCAL)
            self.assertEqual((len(rows),len(hashes)),(1,3))
            report.write_text(json.dumps(dict(validation,status="failed")))
            with self.assertRaises(ValueError):CHECKER.read_capture(capture,LOCAL)
            report.write_text(json.dumps(validation));stream.write_text(stream.read_text()+"\n")
            with self.assertRaisesRegex(ValueError,"identity differs"):CHECKER.read_capture(capture,LOCAL)
            fake=parent/"fake.bin";fake.write_bytes(b"original synthetic fixture")
            with self.assertRaisesRegex(ValueError,"verified private identity"):
                CHECKER.verify_inputs({"program":fake,"tgp_tables":fake,"copro_data":fake})
        with tempfile.TemporaryDirectory(prefix="candidate-external-") as directory:
            with self.assertRaisesRegex(ValueError,"real build directory"):CHECKER.read_capture(Path(directory),LOCAL)


if __name__=="__main__":unittest.main()
