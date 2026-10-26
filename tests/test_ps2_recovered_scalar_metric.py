"""Recovered native scalar metric and strict private-capture adapter."""
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
try:
    import check_recovered_scalar_metric as CHECKER
finally:
    sys.path.pop(0)


def capture():
    fp,caller,obj=0x5ff000,0x5fef00,0x500000
    proof={"pc":0x5d421c,"frame_pointer":caller,"source":"ordinary_ram","cache_index":-1,"words":[0]*16}
    proof["words"][2]=proof["pc"]
    events=[]
    def event(kind,pc,index=0):
        regs=[0]*32;regs[0]=caller;regs[31]=fp;regs[13]=obj;regs[16]=obj
        e={"schema":1,"event":kind,"call_id":1,"object":obj,"main_frame_pointer":fp,
           "frame":2571,"instructions":10*(len(events)+1),"pc":pc,"ac":0,
           "registers":regs,"fp_bits":[0]*4,"object_words":[0]*256,
           "return_frame":copy.deepcopy(proof),"parent_return_frame":copy.deepcopy(proof),"blocks":[]}
        if kind in ("cross_post","metric_post"):
            e["metric_index"]=index;regs[16:19]=[fp+0xd8,fp+0xd4,fp+0xd0];regs[23]=12*index
            e["blocks"]=[{"name":"metric_cross","address":fp+0xd0,"words":[0,0x3f800000,0]}]
            if kind=="cross_post":
                e["cross_index"]=index;regs[21]=fp+0x40+12*index
            else:
                regs[20:23]=[0,0x3f800000,0];regs[28]=0x3f825985;regs[29]=0x2c005858
        if kind=="main_return":
            regs[31]=caller
        events.append(e)
    event("main_entry",0x5d3380)
    for i in range(4):
        event("cross_post",0x5d3b04,i);event("metric_post",0x5d3b5c,i)
    event("main_return",0x5d421c)
    marker={"schema":1,"complete":True,"main_calls":1,"main_completions":1,"main_samples":1,"main_events":len(events),
        "main_cross_pairs":4,"main_sampled_cross_pairs":4,"main_queries":4,"main_query_completions":4,
        "main_contacts":4,"main_contact_completions":4,"main_metric_entries":4,"main_metric_completions":4,
        "main_metric_sampled_pairs":4}
    return events,marker


class RecoveredScalarMetricTests(unittest.TestCase):
    def test_rounding_exponents_domain_aliases_and_unit_discriminator(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="recovered-scalar-metric-") as directory:
            exe=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/native/recovered_scalar_metric.cpp"),
                str(ROOT/"platform/ps2/tests/recovered_scalar_metric_test.cpp"),"-o",str(exe)]
            built=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stderr)
            tested=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertIn("PROBE PASS cases=11972 accepted=11234 hash=46407e33dcf00f1b aliases=6",tested.stdout)
            self.assertIn("HOST PASS cases=156644 accepted=146736 hash=807252722fa67a0c aliases=6",tested.stdout)
            self.assertIn("REJECTION PASS cases=96 unit_discriminator=1 identical_alias=1",tested.stdout)

    def test_unverified_firmware_is_rejected(self):
        for image in (b"",bytes(128),bytes(6660)):
            with self.assertRaises(ValueError):
                CHECKER.verify_firmware(image)

    def test_shared_cross_boundary_forms_four_complete_metric_pairs(self):
        events,marker=capture();rows,pairs=CHECKER.metric_rows(events,marker)
        self.assertEqual(rows,[(0,0,0x3f800000,0,0,0,0x3f825985)]*4)
        self.assertEqual([p["index"] for p in pairs],[0,1,2,3])
        for bad in ([],events[1:],events[:-1],events[:2]+events[3:],events+events,list(reversed(events))):
            with self.assertRaises(ValueError):
                CHECKER.metric_rows(bad,marker)

    def test_malformed_metric_proof_transport_domain_and_counts_rejected(self):
        events,marker=capture()
        changes=[
            lambda e:e[1].__setitem__("pc",0x5d3b0c),
            lambda e:e[2].__setitem__("pc",0x5d3b54),
            lambda e:e[1].__setitem__("cross_index",1),
            lambda e:e[1].__setitem__("metric_index",1),
            lambda e:e[2].__setitem__("call_id",2),
            lambda e:e[1]["registers"].__setitem__(30,0x80000000),
            lambda e:e[2]["registers"].__setitem__(29,0x2d805b5b),
            lambda e:e[2]["registers"].__setitem__(21,0),
            lambda e:e[1]["registers"].__setitem__(23,12),
            lambda e:e[1]["registers"].__setitem__(16,0x5ff0d0),
            lambda e:e[1]["registers"].__setitem__(21,0x5ff044),
            lambda e:e[1]["registers"].__setitem__(13,0x500100),
            lambda e:e[1]["registers"].__setitem__(0,0x5fee00),
            lambda e:e[0]["return_frame"]["words"].__setitem__(2,0x5d4218),
            lambda e:e[0]["return_frame"].__setitem__("source","unproven"),
            lambda e:e[1]["parent_return_frame"]["words"].__setitem__(4,1),
            lambda e:e[2]["blocks"][0]["words"].__setitem__(0,1),
            lambda e:e[1]["blocks"][0]["words"].__setitem__(1,0x49800001),
            lambda e:e[1]["blocks"][0].__setitem__("address",0x5ff0d4),
            lambda e:e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][0])),
            lambda e:e[1]["blocks"][0]["words"].pop(),
            lambda e:e[2]["object_words"].__setitem__(3,1),
            lambda e:e[-1]["registers"].__setitem__(31,0x5ff000),
        ]
        for change in changes:
            bad=copy.deepcopy(events);change(bad)
            with self.assertRaises(ValueError):
                CHECKER.metric_rows(bad,marker)
        for key,value in (("main_calls",0),("main_completions",2),("main_samples",2),("main_events",9),
            ("main_metric_entries",3),("main_metric_completions",3),("main_metric_sampled_pairs",3),
            ("main_cross_pairs",3),("main_sampled_cross_pairs",3),("main_queries",3),
            ("main_query_completions",3),("main_contacts",3),("main_contact_completions",3)):
            bad=dict(marker);bad[key]=value
            with self.assertRaises(ValueError):
                CHECKER.metric_rows(events,bad)

    def test_response_boundary_is_allowed_only_outside_metric_pair(self):
        events,marker=capture()
        extra=copy.deepcopy(events[1]);extra["event"]="response_pre";extra["pc"]=0x5d372c
        for offset,accepted in ((1,True),(2,False)):
            extended=copy.deepcopy(events[:offset])+[extra]+copy.deepcopy(events[offset:])
            for i,event in enumerate(extended):event["instructions"]=10*(i+1)
            counts=dict(marker);counts["main_events"]=len(extended)
            if accepted:self.assertEqual(len(CHECKER.metric_rows(extended,counts)[0]),4)
            else:
                with self.assertRaises(ValueError):CHECKER.metric_rows(extended,counts)

    def test_capture_files_must_match_successful_observer_validation(self):
        events,marker=capture()
        with tempfile.TemporaryDirectory(prefix="metric-capture-",dir=ROOT/"build") as directory:
            base=Path(directory);folder=base/"capture";folder.mkdir()
            (folder/"capture.json").write_text(json.dumps(marker))
            (folder/"main-contact.jsonl").write_text("\n".join(json.dumps(e) for e in events)+"\n")
            hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir()}
            validation={"status":"passed","exact_checkpoint_match":True,"capture_files_sha256":hashes}
            (base/"validation.json").write_text(json.dumps(validation))
            self.assertEqual(len(CHECKER.capture_rows(folder)[0]),4)
            validation["exact_checkpoint_match"]=False
            (base/"validation.json").write_text(json.dumps(validation))
            with self.assertRaises(ValueError):
                CHECKER.capture_rows(folder)
            validation["exact_checkpoint_match"]=True
            (base/"validation.json").write_text(json.dumps(validation))
            (folder/"main-contact.jsonl").write_text("{}\n")
            with self.assertRaises(ValueError):
                CHECKER.capture_rows(folder)


if __name__=="__main__":
    unittest.main()
