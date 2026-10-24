#!/usr/bin/env python3
"""ROM-free checks of the actual offline observer and its output boundary."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class DrivingCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: raise unittest.SkipTest("C++20 compiler required")
        (ROOT/"build").mkdir(exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix="driving-observer-test-",dir=ROOT/"build")
        cls.directory=Path(cls.temp.name)
        cls.exe=cls.directory/"observer"
        result=subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
            "-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/tools"),
            str(ROOT/"platform/ps2/tools/driving_capture.cpp"),
            str(ROOT/"platform/ps2/tests/driving_capture_test.cpp"),"-o",str(cls.exe)],
            capture_output=True,text=True)
        if result.returncode: raise AssertionError(result.stdout+result.stderr)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def run_case(self,mode,name=None):
        path=self.directory/(name or mode)
        return path,subprocess.run([str(self.exe),mode,str(path)],capture_output=True,text=True)

    def test_actual_records_raw_bits_pairing_and_optional_block_bounds(self):
        for mode in ("valid","rom","alternate"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                self.assertEqual((marker["calls"],marker["completed_calls"],marker["events"]),(1,1,10))
                self.assertEqual(marker["alternate_calls"],int(mode=="alternate"))
                self.assertEqual(marker["selector_observations"],1)
                events=[json.loads(line) for line in (path/"events.jsonl").read_text().splitlines()]
                self.assertEqual([e["event"] for e in events],["integrator_entry","orientation_pre",
                    "orientation_post","blend_pre","blend_store","blend_post","query_pre",
                    "query_count","query_post","integrator_return"])
                self.assertEqual(events[0]["fp_bits"][0],1<<63)
                self.assertEqual(events[0]["object_words"][0],0x210000^0x12345678)
                self.assertTrue(all(len(e["registers"])==32 and len(e["object_words"])==256 for e in events))
                self.assertEqual(events[0]["registers"][16],events[0]["object"])
                self.assertEqual(events[0]["branch_selector"],events[0]["object_words"][0xe8//4])
                selector=json.loads((path/"selectors.jsonl").read_text())
                self.assertEqual((selector["object"],selector["selector"]),
                                 (events[0]["object"],events[0]["branch_selector"]))
                self.assertEqual(events[-2]["pc"],events[-4]["registers"][2])
                blocks={b["name"]:b for b in events[3]["blocks"]}
                self.assertEqual(blocks["blend_output"]["address"],0x220000)
                self.assertEqual(len(blocks["blend_output"]["words"]),4)
                self.assertNotIn("selected_descriptor",blocks)
                self.assertNotIn("query_staging",blocks)
                self.assertEqual(len((path/"cadence.jsonl").read_text().splitlines()),2)

    def test_failures_never_seal_a_capture(self):
        for mode,message in (("forbidden","non-RAM"),("nested","nested"),
                             ("timeout","bounded instruction"),("incomplete","no complete"),
                             ("unselected","no complete"),("nested-query","unpaired table query"),
                             ("unfinished-query","unpaired query")):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertNotEqual(result.returncode,0)
                self.assertIn(message,result.stderr)
                self.assertFalse((path/"capture.json").exists())

    def test_new_real_build_directory_is_required(self):
        existing=self.directory/"existing";existing.mkdir()
        with tempfile.TemporaryDirectory(prefix="driving-outside-") as external:
            linked=self.directory/"alias";linked.symlink_to(external,target_is_directory=True)
            for path in (existing,ROOT/"build",Path(external)/"capture",linked/"capture"):
                result=subprocess.run([str(self.exe),"valid",str(path)],capture_output=True,text=True)
                self.assertNotEqual(result.returncode,0)
                self.assertIn("new directory below ignored build",result.stderr)
            self.assertFalse((Path(external)/"capture").exists())


if __name__=="__main__": unittest.main()
