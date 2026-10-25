"""Synthetic main-response normalization capture boundaries and provenance."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import contact_normalization_live as CHECKER
finally:
    sys.path.pop(0)
from test_ps2_contact_result import main_pass
from test_ps2_contact_query import block


def fixture():
    original, marker = main_pass()
    events = []
    for event in original:
        if event["event"] == "response_post":
            for kind,pc,words in (("normalization_pre",0x5d38ec,[0x40000000,0,0]),
                                  ("normalization_post",0x5d3968,[0x3f800000,0,0])):
                value = copy.deepcopy(event)
                fp = value["main_frame_pointer"]
                value.update(event=kind,pc=pc)
                value["registers"][7],value["registers"][25] = fp+0xd0,fp+0xd8
                value["blocks"] = [block("normalization_vector",fp+0xd0,words),
                                   block("candidate_position",0x220000,[1,2,3]),
                                   block("candidate_orientation",0x220020,[4,5,6])]
                events.append(value)
        events.append(event)
    for i,event in enumerate(events): event["instructions"] = 10*(i+1)
    marker.update(main_events=len(events),main_normalization_entries=4,
                  main_normalization_completions=4,main_normalization_sampled_pairs=4)
    return events,marker


class NormalizationLiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/"build").mkdir(exist_ok=True)

    def test_complete_parent_pipelined_pairs_preserve_six_word_order(self):
        events,marker=fixture()
        rows,provenance=CHECKER.normalization_rows(events,marker)
        self.assertEqual(rows,[(0x40000000,0,0,0x3f800000,0,0)]*4)
        self.assertEqual([p["contact_index"] for p in provenance],[0,1,2,3])
        self.assertTrue(all(p["instructions"]==10 and p["return_pc"]==0x5d421c for p in provenance))

    def test_pair_identity_pointers_rounding_domain_and_unrelated_state_reject(self):
        original,marker=fixture()
        pre=next(i for i,e in enumerate(original) if e["event"]=="normalization_pre")
        post=pre+1
        mutations=[
            lambda e:e[pre].__setitem__("pc",0x5d38e8),
            lambda e:e[post].__setitem__("pc",0x5d396c),
            lambda e:e[pre].__setitem__("contact_index",1),
            lambda e:e[pre]["registers"].__setitem__(5,1),
            lambda e:e[pre]["registers"].__setitem__(7,0x900000),
            lambda e:e[pre]["registers"].__setitem__(25,e[pre]["registers"][25]+4),
            lambda e:e[post]["registers"].__setitem__(31,e[post]["registers"][31]+64),
            lambda e:e[post]["registers"].__setitem__(13,0x210100),
            lambda e:e[pre]["registers"].__setitem__(30,1),
            lambda e:e[post].__setitem__("ac",0x7f001000),
            lambda e:e[pre]["blocks"][0]["words"].__setitem__(0,0x49800001),
            lambda e:e[pre]["blocks"][0]["words"].__setitem__(0,0x7f800000),
            lambda e:e[pre]["blocks"][0]["words"].__setitem__(0,True),
            lambda e:e[post]["blocks"][0]["words"].pop(),
            lambda e:e[post]["blocks"].append(copy.deepcopy(e[post]["blocks"][0])),
            lambda e:e[post]["object_words"].__setitem__(0,1),
            lambda e:e[post]["blocks"][1]["words"].__setitem__(0,99),
            lambda e:e[post]["blocks"][2]["words"].__setitem__(0,99),
            lambda e:e[pre]["return_frame"]["words"].__setitem__(2,0x5d4218),
            lambda e:e[post]["return_frame"].__setitem__("source","device"),
        ]
        for index,change in enumerate(mutations):
            events=copy.deepcopy(original);change(events)
            with self.subTest(mutation=index),self.assertRaises(ValueError):
                CHECKER.normalization_rows(events,marker)
        for name in ("candidate_position","candidate_orientation"):
            events=copy.deepcopy(original)
            for event in events[pre:post+1]:
                next(b for b in event["blocks"] if b["name"]==name)["address"]+=4
            with self.assertRaisesRegex(ValueError,"parent entry arguments"):
                CHECKER.normalization_rows(events,marker)

    def test_incomplete_duplicate_and_incorrect_population_reject(self):
        events,marker=fixture()
        pre=next(i for i,e in enumerate(events) if e["event"]=="normalization_pre")
        variants=[[],events[1:],events[:-1],events[:pre]+events[pre+1:],
                  events[:pre+1]+events[pre+2:],events[:pre+2]+events[pre:pre+2]+events[pre+2:]]
        for bad in variants:
            bad=copy.deepcopy(bad)
            for i,event in enumerate(bad): event["instructions"]=10*(i+1)
            with self.subTest(events=len(bad)),self.assertRaises(ValueError):
                CHECKER.normalization_rows(bad,dict(marker,main_events=len(bad)))
        for field,value in (("main_normalization_entries",0),("main_normalization_completions",3),
                            ("main_normalization_sampled_pairs",3),("main_normalization_entries",5)):
            with self.subTest(field=field),self.assertRaises(ValueError):
                CHECKER.normalization_rows(events,dict(marker,**{field:value}))

    def test_reader_binds_complete_capture_to_passing_observer_hashes(self):
        with tempfile.TemporaryDirectory(prefix="normalization-proof-",dir=ROOT/"build") as directory:
            parent=Path(directory);capture=parent/"capture";capture.mkdir()
            events,marker=fixture()
            stream=capture/"main-contact.jsonl";metadata=capture/"capture.json"
            stream.write_text("".join(json.dumps(e)+"\n" for e in events))
            metadata.write_text(json.dumps(marker))
            validation={"status":"passed","exact_checkpoint_match":True,
                        "capture_files_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (stream,metadata)}}
            report=parent/"validation.json";report.write_text(json.dumps(validation))
            rows,_,hashes=CHECKER.read_capture(capture)
            self.assertEqual(len(rows),4);self.assertEqual(len(hashes),3)
            report.write_text(json.dumps(dict(validation,status="failed")))
            with self.assertRaisesRegex(ValueError,"observer parity"): CHECKER.read_capture(capture)
            report.write_text(json.dumps(validation));stream.write_text(stream.read_text()+"\n")
            with self.assertRaisesRegex(ValueError,"validated identity"): CHECKER.read_capture(capture)
            metadata.write_bytes(b" "*16385)
            with self.assertRaisesRegex(ValueError,"reader bound"): CHECKER.read_capture(capture)
        with tempfile.TemporaryDirectory(prefix="normalization-external-") as directory:
            with self.assertRaisesRegex(ValueError,"real build directory"): CHECKER.read_capture(Path(directory))


if __name__=="__main__": unittest.main()
