#!/usr/bin/env python3
"""Original authored value/observer guards; no private game data is required."""
import copy
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_render_bridge as checker


def records():
    proof={"pc":0x5bf380,"frame_pointer":0x210000,"source":"ordinary_ram","cache_index":-1,
           "words":[0x20ffc0,0,0x5bf380]+[0]*13}
    base={"schema":1,"call_id":1,"entry_pc":0x1ff60,"frame":10,"frame_pointer":0x210040,
          "return_frame":proof,"ac":0,"registers":[0]*32}
    before=copy.deepcopy(base);before.update(event="entry",pc=0x5bef60,instructions=100)
    before["registers"][0]=0x210000;before["registers"][31]=0x210040
    middle=copy.deepcopy(base);middle.update(event="view_matrix",pc=0x5bf1f0,instructions=110)
    middle["registers"][31]=0x210040
    after=copy.deepcopy(base);after.update(event="return",pc=0x5bf380,instructions=120)
    after["registers"][0]=0x20ffc0;after["registers"][31]=0x210000
    return [before,middle,after],{"schema":1,"complete":True,"calls":1,"completions":1,"events":3,"first_frame":10,"last_frame":10}


class RenderBridgeTests(unittest.TestCase):
    def test_portable_values(self):
        compiler=shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/"test"
            source=[f"platform/ps2/native/{name}.cpp"for name in checker.NATIVE]
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off",
                     "-I",str(ROOT/"platform/ps2/native"),str(ROOT/"platform/ps2/tests/render_bridge_test.cpp"),
                     *(str(ROOT/p)for p in source),"-o",str(exe)]
            build=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn("RENDER_BRIDGE PASS",run.stdout)

    def test_original_authored_call_proofs(self):
        events,marker=records();self.assertEqual(len(checker.camera_groups(events,marker)),1)
        for change in (lambda e:e[0]["return_frame"].update(pc=4),
                       lambda e:e[2]["registers"].__setitem__(0,0),
                       lambda e:e[1].update(pc=0x5bf1f4),
                       lambda e:e[1].update(ac=0x40000000),
                       lambda e:e[2].update(instructions=100),
                       lambda e:e[1].update(frame=11),
                       lambda e:e[0]["return_frame"].update(source="fabricated")):
            events,marker=records();change(events)
            with self.assertRaises(ValueError):checker.camera_groups(events,marker)
        events,marker=records();marker["completions"]=0
        with self.assertRaises(ValueError):checker.camera_groups(events,marker)

    def test_block_identity(self):
        e={"blocks":[{"name":"pose","address":64,"words":[1,2,3]}]}
        self.assertEqual(checker.block(e,"pose",64,3),[1,2,3])
        for address,count in ((68,3),(64,2)):
            with self.assertRaises(ValueError):checker.block(e,"pose",address,count)
        e["blocks"].append(copy.deepcopy(e["blocks"][0]))
        with self.assertRaises(ValueError):checker.block(e,"pose",64,3)


if __name__=="__main__":unittest.main()
