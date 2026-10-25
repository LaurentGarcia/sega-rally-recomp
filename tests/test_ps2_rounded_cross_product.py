"""Native rounded cross products, without game assets or firmware."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "platform/ps2/tools"))
try:
    import check_rounded_cross_product as CHECKER
finally:
    sys.path.pop(0)


def capture():
    fp,caller,obj=0x5ff000,0x5fef00,0x500000
    proof={"pc":0x5d421c,"frame_pointer":caller,"source":"ordinary_ram","cache_index":-1,"words":[0]*16}
    proof["words"][2]=proof["pc"]
    events=[]
    def event(kind,pc,index=0):
        regs=[0]*32
        regs[0]=caller;regs[31]=fp;regs[13]=obj;regs[16]=obj
        record={"schema":1,"event":kind,"call_id":1,"object":obj,"main_frame_pointer":fp,
                "frame":2500,"instructions":10*(len(events)+1),"pc":pc,"ac":0,
                "registers":regs,"fp_bits":[0]*4,"object_words":[0]*256,
                "return_frame":copy.deepcopy(proof),"parent_return_frame":copy.deepcopy(proof),"blocks":[]}
        if kind.startswith("cross_"):
            record["cross_index"]=index
            regs[16:19]=[fp+0xd8,fp+0xd4,fp+0xd0]
            regs[21]=fp+0x40+12*index;regs[23]=12*index
            record["blocks"]=[{"name":name,"address":address,"words":[0,0,0]}
                for name,address in (("cross_left",fp+0x70+12*index),
                                     ("cross_right",fp+0xa0+12*index),("cross_result",fp+0xd0))]
        if kind=="main_return":
            regs[31]=caller
        events.append(record)
    event("main_entry",0x5d3380)
    for i in range(4):
        event("cross_pre",0x5d3a88,i)
        event("cross_post",0x5d3b04,i)
    event("main_return",0x5d421c)
    marker={"main_calls":1,"main_completions":1,"main_samples":1,"main_events":len(events),
            "main_cross_pairs":4,"main_sampled_cross_pairs":4,"main_queries":4,"main_query_completions":4,
            "main_contacts":4,"main_contact_completions":4}
    return events,marker


class RoundedCrossProductTests(unittest.TestCase):
    def test_separate_rounding_aliases_signed_zeros_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rounded-cross-product-") as directory:
            executable = Path(directory) / "test"
            command = [compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off",
                "-Wall","-Wextra","-Werror","-I",str(ROOT / "platform/ps2/native"),
                str(ROOT / "platform/ps2/native/rounded_cross_product.cpp"),
                str(ROOT / "platform/ps2/tests/rounded_cross_product_test.cpp"),"-o",str(executable)]
            built = subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stderr)
            tested = subprocess.run([str(executable)],capture_output=True,text=True,timeout=30)
            self.assertEqual(tested.returncode,0,tested.stderr)
            self.assertIn("PROBE PASS cases=6208 hash=408f01a4e8d06ea5 aliases=3",tested.stdout)
            self.assertIn("HOST PASS cases=110048 hash=428bbf5526cd241b aliases=3",tested.stdout)
            self.assertIn("REJECTION PASS cases=180 fused_guard=1",tested.stdout)

    def test_unverified_firmware_is_rejected(self):
        for image in (b"",bytes(96),bytes(6660)):
            with self.assertRaises(ValueError):
                CHECKER.verify_firmware(image)

    def test_complete_live_cross_groups_require_parent_return(self):
        events,marker=capture()
        rows,pairs=CHECKER.cross_rows(events,marker)
        self.assertEqual(rows,[(0,)*9]*4)
        self.assertEqual([p["index"] for p in pairs],[0,1,2,3])
        for bad in ([],events[1:],events[:-1],events[:2]+events[3:],events+events,list(reversed(events))):
            with self.assertRaises(ValueError):
                CHECKER.cross_rows(bad,marker)

    def test_malformed_live_proof_pointers_inputs_and_counts_are_rejected(self):
        events,marker=capture()
        changes=[
            lambda e:e[1].__setitem__("pc",0x5d3b04),
            lambda e:e[2].__setitem__("call_id",2),
            lambda e:e[1].__setitem__("cross_index",1),
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
            lambda e:e[1]["blocks"][1].__setitem__("address",0x5ff0a4),
            lambda e:e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][0])),
            lambda e:e[1]["blocks"][2]["words"].pop(),
            lambda e:e[2]["object_words"].__setitem__(3,1),
            lambda e:e[-1]["registers"].__setitem__(31,0x5ff000),
        ]
        for change in changes:
            bad=copy.deepcopy(events)
            change(bad)
            with self.assertRaises(ValueError):
                CHECKER.cross_rows(bad,marker)
        for key,value in (("main_calls",0),("main_completions",2),("main_samples",2),
                          ("main_events",9),("main_cross_pairs",3),("main_sampled_cross_pairs",3),
                          ("main_queries",3),("main_query_completions",3),
                          ("main_contacts",3),("main_contact_completions",3)):
            bad=dict(marker);bad[key]=value
            with self.assertRaises(ValueError):
                CHECKER.cross_rows(events,bad)


if __name__ == "__main__":
    unittest.main()
