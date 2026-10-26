"""Synthetic field mapping and provenance checks for composed response passes."""
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
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_contact_pass_response as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_result import main_pass
from test_ps2_contact_query import block


def fixture():
    events, marker = main_pass()
    for event in events:
        for index, value in zip((20,21,22,5,7,15,11,13,18), range(101,110)):
            event["object_words"][index] = value
        if event["event"] in ("main_entry","main_prequery","main_return"):
            event["blocks"] = [block("candidate_position",0x220000,[201,202,203]),
                               block("candidate_orientation",0x220020,[204,205,206]),
                               block("feedback_enable",0x2142c8,[0]),
                               block("feedback",0x213960,[301,302,303]),
                               block("transformed_offsets",event["main_frame_pointer"]+0x70,list(range(12)))]
        if event["event"] == "main_return":
            for index,value in zip((11,13,18),(407,408,409)): event["object_words"][index] = value
            event["blocks"][0]["words"] = [401,202,403]
            event["blocks"][1]["words"] = [204,405,206]
            event["blocks"][3]["words"] = [501,502,503]
            event["registers"][16] = 601
    return events,marker


class ContactPassResponseReaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/"build").mkdir(exist_ok=True)

    def test_composed_native_pass_and_atomic_guards(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-response-pass-") as directory:
            executable = Path(directory)/"response-pass"
            sources = [ROOT/f"platform/ps2/native/{name}.cpp" for name in CHECKER.NATIVE]
            command = [compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                       "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native"),
                       str(ROOT/"platform/ps2/tests/contact_pass_response_test.cpp"),
                       *(str(name) for name in sources),"-o",str(executable)]
            built = subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            tested = subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertIn("CONTACT_PASS_RESPONSE_PROBE PASS cases=640 hash=3a490cc705f0b6f2",tested.stdout)
            self.assertIn("CONTACT_PASS_RESPONSE_HOST PASS cases=4224 hash=f99dcd3347d0f255",tested.stdout)
            self.assertIn("CONTACT_PASS_RESPONSE_GUARDS PASS inactive_preservation=1 late_atomic_rejection=1",tested.stdout)

    def test_all_65_words_follow_explicit_parameter_and_state_order(self):
        events,marker=fixture()
        rows,provenance=CHECKER.pass_rows(events,marker)
        expected = list(range(30,35))*4 + list(range(12)) + list(range(101,107)) + [1]
        expected += [107,108,109,201,202,203,204,205,206,302,303,301,0]
        expected += [407,408,409,401,202,403,204,405,206,502,503,501,601]
        self.assertEqual(rows,[tuple(expected)])
        self.assertEqual(len(rows[0]),65)
        self.assertEqual(provenance,[{"call_id":7,"entry_frame":100,"exit_frame":100,
                                      "selected_scalar":601,"contact_mask":501}])

    def test_preparation_frame_proof_and_operand_addresses_are_required(self):
        original,marker=fixture()
        changes=[
            lambda e:e[1].pop("return_frame"),
            lambda e:e[1]["return_frame"]["words"].__setitem__(2,0x5d4218),
            lambda e:e[1]["return_frame"].__setitem__("source","device"),
            lambda e:e[1]["registers"].__setitem__(0,0x200000),
            lambda e:e[1]["blocks"][-1].__setitem__("address",e[1]["main_frame_pointer"]+0x74),
            lambda e:e[1]["blocks"][-1]["words"].pop(),
            lambda e:e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][-1])),
            lambda e:e[0].__setitem__("ac",0x7f001000),
        ]
        for number,change in enumerate(changes):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=number),self.assertRaises(ValueError):
                CHECKER.pass_rows(events,marker)
        for slot,name in ((17,"candidate_position"),(18,"candidate_orientation")):
            events=copy.deepcopy(original);events[0]["registers"][slot]=0x900000
            for event in (events[0],events[-1]):
                next(b for b in event["blocks"] if b["name"]==name)["address"]=0x900000
            with self.subTest(block=name),self.assertRaises(ValueError): CHECKER.pass_rows(events,marker)

    def test_unrelated_memory_and_disabled_feedback_must_be_preserved(self):
        original,marker=fixture()
        changes=[lambda e:e[-1]["object_words"].__setitem__(0,1),
                 lambda e:e[-1]["object_words"].__setitem__(20,999),
                 lambda e:e[-1]["blocks"][0]["words"].__setitem__(1,999),
                 lambda e:e[-1]["blocks"][1]["words"].__setitem__(0,999),
                 lambda e:e[-1]["blocks"][1]["words"].__setitem__(2,999),
                 lambda e:e[-1]["blocks"][2]["words"].__setitem__(0,1)]
        for number,change in enumerate(changes):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=number),self.assertRaises(ValueError): CHECKER.pass_rows(events,marker)
        events=copy.deepcopy(original)
        for event in (events[0],events[-1]): event["blocks"][2]["words"]=[1]
        with self.assertRaisesRegex(ValueError,"disabled feedback"): CHECKER.pass_rows(events,marker)
        events[-1]["blocks"][3]["words"][1:]=[302,303]
        rows,_=CHECKER.pass_rows(events,marker)
        self.assertEqual(rows[0][38],0)
        self.assertEqual(rows[0][-4:-2],(302,303))

    def test_missing_reordered_and_incorrect_completion_records_reject(self):
        events,marker=fixture()
        contact=next(i for i,e in enumerate(events) if e["event"]=="contact_post")
        for bad in ([],events[:-1],events[1:],events[:1]+events[2:],
                    events[:contact]+events[contact+1:],events[:2]+events[1:2]+events[2:]):
            bad=copy.deepcopy(bad)
            for i,event in enumerate(bad):event["instructions"]=10*(i+1)
            with self.subTest(length=len(bad)),self.assertRaises(ValueError):
                CHECKER.pass_rows(bad,dict(marker,main_events=len(bad)))
        for name in ("main_samples","main_completions","main_contacts","main_query_completions"):
            with self.subTest(field=name),self.assertRaises(ValueError):
                CHECKER.pass_rows(events,dict(marker,**{name:marker[name]+1}))

    def test_reader_binds_schema_completion_hashes_and_private_location(self):
        with tempfile.TemporaryDirectory(prefix="pass-response-proof-",dir=ROOT/"build") as directory:
            parent=Path(directory);capture=parent/"capture";capture.mkdir()
            events,marker=fixture()
            stream,metadata=capture/"main-contact.jsonl",capture/"capture.json"
            stream.write_text("".join(json.dumps(e)+"\n" for e in events))
            def publish(meta=None,fields=None):
                metadata.write_text(json.dumps(meta or marker))
                validation={"status":"passed","exact_checkpoint_match":True,
                            "capture_files_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                                     for p in (stream,metadata)}}
                validation.update(fields or {})
                (parent/"validation.json").write_text(json.dumps(validation))
            publish()
            rows,_,hashes=CHECKER.read_capture(capture)
            self.assertEqual((len(rows),len(hashes)),(1,3))
            for fields in ({"status":"failed"},{"exact_checkpoint_match":False}):
                publish(fields=fields)
                with self.assertRaises(ValueError):CHECKER.read_capture(capture)
            for fields in ({"schema":2},{"complete":False}):
                publish(meta=dict(marker,**fields))
                with self.assertRaises(ValueError):CHECKER.read_capture(capture)
            publish();stream.write_text(stream.read_text()+"\n")
            with self.assertRaisesRegex(ValueError,"identity differs"):CHECKER.read_capture(capture)
            metadata.write_bytes(b" "*16385)
            with self.assertRaisesRegex(ValueError,"bounded size"):CHECKER.read_capture(capture)
        with tempfile.TemporaryDirectory(prefix="pass-response-external-") as directory:
            with self.assertRaisesRegex(ValueError,"real build directory"):CHECKER.read_capture(Path(directory))


if __name__=="__main__": unittest.main()
