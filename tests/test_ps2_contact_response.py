"""Original bounded per-contact response arithmetic and observation checks."""
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
    import check_contact_response as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_result import main_pass
from test_ps2_contact_query import block


def fixture():
    original, marker = main_pass()
    slots = []
    for i in range(4): slots += [10,11,12,0x3f800000,0,0,0x3f800000,128 if i==0 else 0,0]
    events, responses = [], [0]*12
    for event in original:
        event["object_words"][20:22] = [0x447a0000,0x448f4000]
        if event["event"] == "contact_post":
            index = event["contact_index"]
            next(b for b in event["blocks"] if b["name"]=="contact_output")["words"] = slots[9*index:9*index+8]
        if event["event"] == "main_return": event["registers"][16] = 0x3f800000
        if event["event"] != "response_post":
            events.append(event)
            continue
        index,fp = event["contact_index"],event["main_frame_pointer"]
        for kind,pc in (("response_pre",0x5d372c),("response_post",0x5d3a30)):
            value = copy.deepcopy(event);value.update(event=kind,pc=pc)
            regs = value["registers"]
            for reg,word in ((3,0x2138ec+36*index),(9,0x2138d0+36*index),(10,0x2138e8+36*index),
                             (6,12*index),(15,36*index),(7,fp+0xd0),(14,fp+0xe0),(25,fp+0xd8),
                             (20,128 if index==0 else 0),(12,0 if index==0 and kind=="response_pre" else 0x3f800000)):
                regs[reg]=word
            if kind=="response_post" and index==0: responses[:3]=[0x3f000000,0,0]
            value["blocks"] = [block("result_slots",0x2138d0,list(slots)),
                               block("transformed_offsets",fp+0x70,[0,0,0x3f800000]*4),
                               block("response_vectors",fp+0xa0,list(responses)),
                               block("feedback",0x213960,[0 if index==0 and kind=="response_pre" else 1,2,3]),
                               block("feedback_enable",0x2142c8,[0]),
                               block("candidate_position",0x220000,[1,2,3]),
                               block("candidate_orientation",0x220020,[4,5,6])]
            events.append(value)
    for i,e in enumerate(events):e["instructions"]=10*(i+1)
    marker.update(main_events=len(events),main_response_entries=4,main_response_completions=4,main_response_sampled_pairs=4)
    return events,marker


class ContactResponseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): (ROOT/"build").mkdir(exist_ok=True)

    def test_actual_native_response_rounding_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-response-") as directory:
            executable = Path(directory)/"response"
            sources = ["platform/ps2/native/contact_response.cpp", "platform/ps2/native/contact_normalization.cpp",
                       "platform/ps2/native/rounded_cross_product.cpp", "platform/ps2/tests/contact_response_test.cpp"]
            command = [compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                       "-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native"),
                       *(str(ROOT/name) for name in sources),"-o",str(executable)]
            built = subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            tested = subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertIn("CONTACT_RESPONSE_PROBE PASS cases=1200 accepted=1128 hash=cd1b0a7946e70d88",tested.stdout)
            self.assertIn("CONTACT_RESPONSE_HOST PASS cases=18432 accepted=18360 hash=3684bd3cf078fbc4",tested.stdout)
            self.assertIn("CONTACT_RESPONSE_REJECTION PASS checks=33",tested.stdout)

    def test_active_and_inactive_pairs_preserve_word_order_and_scalar_chain(self):
        events,marker=fixture()
        rows,provenance=CHECKER.response_rows(events,marker)
        first=(0x3f800000,0,0,0,0,0x3f800000,0x3f800000,0,0x447a0000,0x448f4000,128,
               0x3f000000,0,0,0x3f800000,1)
        rest=(0x3f800000,0,0,0,0,0x3f800000,0x3f800000,0x3f800000,0x447a0000,0x448f4000,0,
              0,0,0,0x3f800000,0)
        self.assertEqual(rows,[first,rest,rest,rest])
        self.assertEqual([p["active"] for p in provenance],[True,False,False,False])
        self.assertEqual([p["index"] for p in provenance],[0,1,2,3])

    def test_pointer_parent_gate_rounding_domain_and_chain_reject(self):
        original,marker=fixture()
        pre=next(i for i,e in enumerate(original) if e["event"]=="response_pre")
        changes=[lambda e:e[pre].__setitem__("pc",0x5d3728),
                 lambda e:e[pre].__setitem__("contact_index",1),
                 lambda e:e[pre]["return_frame"]["words"].__setitem__(2,0x5d4218),
                 lambda e:e[pre].__setitem__("ac",0x7f001000),
                 lambda e:e[pre]["registers"].__setitem__(20,0),
                 lambda e:e[pre]["registers"].__setitem__(12,1),
                 lambda e:e[pre]["blocks"][3]["words"].__setitem__(0,1),
                 lambda e:e[pre]["blocks"][0]["words"].__setitem__(3,0x41800001),
                 lambda e:e[pre]["blocks"][0]["words"].__setitem__(6,0x44800001),
                 lambda e:e[pre]["object_words"].__setitem__(20,0),
                 lambda e:e[pre]["object_words"].__setitem__(21,0x47800001),
                 lambda e:e[-1]["registers"].__setitem__(16,0)]
        changes += [lambda e,r=r:e[pre]["registers"].__setitem__(r,e[pre]["registers"][r]+4)
                    for r in (3,5,6,7,9,10,13,14,15,25,30,31)]
        for number,change in enumerate(changes):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=number),self.assertRaises(ValueError):CHECKER.response_rows(events,marker)

    def test_output_preserves_other_slots_candidates_and_mask_arithmetic(self):
        original,marker=fixture()
        post=next(i for i,e in enumerate(original) if e["event"]=="response_post")
        changes=[lambda e:e[post]["object_words"].__setitem__(0,1),
                 lambda e:e[post]["blocks"][0]["words"].__setitem__(3,0),
                 lambda e:e[post]["blocks"][1]["words"].__setitem__(0,1),
                 lambda e:e[post]["blocks"][2]["words"].__setitem__(3,1),
                 lambda e:e[post]["blocks"][3]["words"].__setitem__(0,2),
                 lambda e:e[post]["blocks"][3]["words"].__setitem__(1,4),
                 lambda e:e[post]["blocks"][4]["words"].__setitem__(0,1),
                 lambda e:e[post]["blocks"][5]["words"].__setitem__(0,9),
                 lambda e:e[post]["blocks"][6]["words"].__setitem__(0,9)]
        inactive=next(i for i,e in enumerate(original) if e["event"]=="response_post" and e["contact_index"]==1)
        changes += [lambda e:e[inactive]["blocks"][2]["words"].__setitem__(3,0x80000000),
                    lambda e:e[inactive]["registers"].__setitem__(12,0)]
        for number,change in enumerate(changes):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=number),self.assertRaises(ValueError):CHECKER.response_rows(events,marker)

    def test_missing_pairs_duplicates_and_wrong_counts_reject(self):
        events,marker=fixture()
        pre=next(i for i,e in enumerate(events) if e["event"]=="response_pre")
        for bad in ([],events[:-1],events[:pre]+events[pre+1:],events[:pre+1]+events[pre+2:],
                    events[:pre+1]+events[pre:pre+1]+events[pre+1:]):
            bad=copy.deepcopy(bad)
            for i,e in enumerate(bad):e["instructions"]=10*(i+1)
            with self.subTest(length=len(bad)),self.assertRaises(ValueError):
                CHECKER.response_rows(bad,dict(marker,main_events=len(bad)))
        for name in ("main_response_entries","main_response_completions","main_response_sampled_pairs"):
            with self.subTest(field=name),self.assertRaises(ValueError):
                CHECKER.response_rows(events,dict(marker,**{name:marker[name]+1}))

    def test_reader_requires_complete_hash_bound_private_capture(self):
        with tempfile.TemporaryDirectory(prefix="response-proof-",dir=ROOT/"build") as directory:
            parent=Path(directory);capture=parent/"capture";capture.mkdir()
            events,marker=fixture();stream=capture/"main-contact.jsonl";metadata=capture/"capture.json"
            stream.write_text("".join(json.dumps(e)+"\n" for e in events));metadata.write_text(json.dumps(marker))
            validation={"status":"passed","exact_checkpoint_match":True,
                        "capture_files_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (stream,metadata)}}
            report=parent/"validation.json";report.write_text(json.dumps(validation))
            rows,_,hashes=CHECKER.read_capture(capture)
            self.assertEqual((len(rows),len(hashes)),(4,3))
            for fields in ({"status":"failed"},{"exact_checkpoint_match":False}):
                report.write_text(json.dumps(dict(validation,**fields)))
                with self.assertRaises(ValueError):CHECKER.read_capture(capture)
            report.write_text(json.dumps(validation));stream.write_text(stream.read_text()+"\n")
            with self.assertRaisesRegex(ValueError,"identity differs"):CHECKER.read_capture(capture)
            metadata.write_bytes(b" "*16385)
            with self.assertRaisesRegex(ValueError,"bounded size"):CHECKER.read_capture(capture)
        with tempfile.TemporaryDirectory(prefix="response-outside-") as directory:
            with self.assertRaisesRegex(ValueError,"real build directory"):CHECKER.read_capture(Path(directory))


if __name__=="__main__": unittest.main()
