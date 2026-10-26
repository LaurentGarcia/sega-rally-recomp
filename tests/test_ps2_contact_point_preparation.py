"""Native orientation/point preparation and sealed capture proof validation."""
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
    import check_contact_point_preparation as CHECKER
finally:
    sys.path.pop(0)


def capture():
    fp,caller,obj=0x5ff000,0x5fef00,0x500000
    proof={"pc":0x5d421c,"frame_pointer":caller,"source":"ordinary_ram","cache_index":-1,"words":[0]*16}
    proof["words"][2]=proof["pc"]
    events=[]
    for kind,pc in (("main_entry",0x5d3380),("main_prequery",0x5d36d8),("main_return",0x5d421c)):
        regs=[0]*32;regs[0]=caller;regs[31]=fp;regs[13]=obj;regs[16]=obj;regs[17]=0x200100;regs[18]=0x200110
        blocks=[{"name":"candidate_position","address":0x200100,"words":[0]*3},
                {"name":"candidate_orientation","address":0x200110,"words":[0]*3},
                {"name":"local_contact_points","address":0x5d2e30,"words":[0]*12}]
        if kind=="main_prequery":
            regs[18]=fp+0x40
            blocks.extend([{"name":"transformed_offsets","address":fp+0x70,"words":[0]*12},
                           {"name":"query_points","address":fp+0x40,"words":[0]*12}])
        if kind=="main_return":regs[31]=caller
        events.append({"schema":1,"event":kind,"call_id":1,"object":obj,"main_frame_pointer":fp,
            "frame":2220,"instructions":10*(len(events)+1),"pc":pc,"ac":0,"registers":regs,
            "fp_bits":[0]*4,"object_words":[0]*256,"return_frame":copy.deepcopy(proof),
            "parent_return_frame":copy.deepcopy(proof),"blocks":blocks})
    marker={"main_calls":1,"main_completions":1,"main_samples":1,"main_events":3,
            "main_queries":4,"main_query_completions":4,"main_contacts":4,"main_contact_completions":4}
    return events,marker


class ContactPointPreparationTests(unittest.TestCase):
    def test_polynomial_rotation_rounding_subnormals_aliases_and_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="contact-point-") as directory:
            exe=Path(directory)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/native/contact_point_preparation.cpp"),
                str(ROOT/"platform/ps2/native/rounded_affine_transform.cpp"),str(ROOT/"platform/ps2/tests/contact_point_preparation_test.cpp"),"-o",str(exe)]
            built=subprocess.run(command,capture_output=True,text=True,timeout=60);self.assertEqual(built.returncode,0,built.stderr)
            tested=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stdout+tested.stderr)
            self.assertIn("PROBE PASS cases=3279 hash=ef519d24a3f602fa aliases=3",tested.stdout)
            self.assertIn("HOST PASS cases=49295 hash=f05d0d03253121d8 aliases=3",tested.stdout)
            self.assertIn("REJECTION PASS cases=114",tested.stdout)

    def test_unverified_original_images_are_rejected(self):
        for program,firmware in ((b"",b""),(bytes(2097152),bytes(6660)),(bytes(64),bytes(32))):
            with self.assertRaises(ValueError):CHECKER.verify_images(program,firmware)

    def test_complete_preparation_requires_saved_parent_and_return(self):
        events,marker=capture();rows,pairs=CHECKER.point_rows(events,marker,[0]*12)
        self.assertEqual(rows,[(0,)*42]);self.assertEqual(pairs[0]["instructions"],10)
        for bad in ([],events[1:],events[:-1],events[:1]+events[2:],events+events,list(reversed(events))):
            with self.assertRaises(ValueError):CHECKER.point_rows(bad,marker,[0]*12)

    def test_malformed_proof_inputs_output_and_counts_are_rejected(self):
        events,marker=capture()
        changes=[lambda e:e[0].__setitem__("pc",0x5d3384),lambda e:e[1].__setitem__("pc",0x5d36d4),
            lambda e:e[0].__setitem__("ac",0x40000000),lambda e:e[1].__setitem__("ac",0x80000000),
            lambda e:e[0]["registers"].__setitem__(30,1),lambda e:e[1]["registers"].__setitem__(18,0x5ff044),
            lambda e:e[1]["registers"].__setitem__(13,0x500100),lambda e:e[0]["registers"].__setitem__(17,0x884000),
            lambda e:e[0]["return_frame"]["words"].__setitem__(2,0x5d4218),
            lambda e:e[0]["return_frame"].__setitem__("source","unproven"),
            lambda e:e[1]["parent_return_frame"]["words"].__setitem__(4,1),
            lambda e:e[0]["blocks"][1]["words"].__setitem__(0,0x41000001),
            lambda e:e[0]["blocks"][0]["words"].__setitem__(0,0x49800001),
            lambda e:e[0]["blocks"][2]["words"].__setitem__(0,1),
            lambda e:e[1]["blocks"][0]["words"].__setitem__(0,1),
            lambda e:e[1]["blocks"][1]["words"].__setitem__(0,1),
            lambda e:e[1]["blocks"][2]["words"].__setitem__(0,1),
            lambda e:e[1]["blocks"][3].__setitem__("address",0x5ff074),
            lambda e:e[1]["blocks"][4]["words"].__setitem__(0,0x7f800000),
            lambda e:e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][3])),
            lambda e:e[1]["blocks"][3]["words"].pop(),
            lambda e:e[1]["object_words"].__setitem__(3,1),lambda e:e[-1]["registers"].__setitem__(31,0x5ff000)]
        for change in changes:
            bad=copy.deepcopy(events);change(bad)
            with self.assertRaises(ValueError):CHECKER.point_rows(bad,marker,[0]*12)
        for key,value in (("main_calls",0),("main_completions",2),("main_samples",2),("main_events",4),
                ("main_queries",3),("main_query_completions",3),("main_contacts",3),("main_contact_completions",3)):
            bad=dict(marker);bad[key]=value
            with self.assertRaises(ValueError):CHECKER.point_rows(events,bad,[0]*12)

    def test_child_event_cannot_interrupt_preparation(self):
        events,marker=capture();extra=copy.deepcopy(events[1]);extra["event"]="response_pre";extra["pc"]=0x5d372c
        for offset,wanted in ((1,False),(2,True)):
            extended=events[:offset]+[extra]+events[offset:]
            for i,event in enumerate(extended):event["instructions"]=10*(i+1)
            counts=dict(marker);counts["main_events"]=4
            if wanted:self.assertEqual(len(CHECKER.point_rows(extended,counts,[0]*12)[0]),1)
            else:
                with self.assertRaises(ValueError):CHECKER.point_rows(extended,counts,[0]*12)

    def test_sealed_capture_hashes_and_parity_are_required(self):
        events,marker=capture();marker.update(schema=1,complete=True)
        with tempfile.TemporaryDirectory(prefix="point-capture-",dir=ROOT/"build") as directory:
            base=Path(directory);folder=base/"capture";folder.mkdir()
            (folder/"capture.json").write_text(json.dumps(marker));(folder/"main-contact.jsonl").write_text("\n".join(json.dumps(e) for e in events)+"\n")
            hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir()}
            proof={"status":"passed","exact_checkpoint_match":True,"capture_files_sha256":hashes}
            (base/"validation.json").write_text(json.dumps(proof));self.assertEqual(len(CHECKER.capture_rows(folder,[0]*12)[0]),1)
            proof["exact_checkpoint_match"]=False;(base/"validation.json").write_text(json.dumps(proof))
            with self.assertRaises(ValueError):CHECKER.capture_rows(folder,[0]*12)
            proof["exact_checkpoint_match"]=True;(base/"validation.json").write_text(json.dumps(proof))
            (folder/"main-contact.jsonl").write_text("{}\n")
            with self.assertRaises(ValueError):CHECKER.capture_rows(folder,[0]*12)

if __name__=="__main__":unittest.main()
