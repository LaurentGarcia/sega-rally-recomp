#!/usr/bin/env python3
"""Original paired arithmetic/parameter fixture and strict capture parsing."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_paired_balance as reader
import test_ps2_main_vehicle_capture as vehicle_fixture


class PairedBalancePortableTests(unittest.TestCase):
    def test_exact_pair_rounding_and_exhaustive_parameter_flag_selection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="paired-balance-test-",dir=ROOT/"build") as temporary:
            executable=Path(temporary)/"fixture"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-Wall","-Wextra","-Werror",
                     "-I",str(ROOT/"platform/ps2/native"),"-I",str(ROOT/"platform/ps2/tests")]
            sources=["platform/ps2/tests/paired_balance_test.cpp","platform/ps2/native/paired_balance.cpp",
                     "platform/ps2/native/vehicle_parameter_selection.cpp"]
            result=subprocess.run(command+[str(ROOT/name) for name in sources]+["-o",str(executable)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(executable)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result.stdout.splitlines(),[
                "PAIRED_BALANCE_PROBE PASS cases=2154 hash=1fd698b609c8f960",
                "PAIRED_BALANCE_REJECTION PASS checks=24",
                "VEHICLE_PARAMETER_SELECTION PASS cases=3072 hash=cf0278eb2ab1a5a1",
                "PAIRED_BALANCE_HOST PASS cases=101130 hash=205c18818a38f128"])


class PairedBalanceParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.harness=vehicle_fixture.MainVehicleCaptureTests
        cls.harness.setUpClass()
        cls.events,cls.marker=cls.harness().fixture()
        # Complete the original synthetic observer's intentionally arbitrary
        # output registers with the inspected storage-to-register identities.
        selection=next(e for e in cls.events if e["event"]=="selection_post")
        selection["registers"][17]=0x214254
        selection["registers"][4:8]=[508,509,510,511]
        selection["registers"][20]=512
        post=next(e for e in cls.events if e["event"]=="balance_post")
        post["registers"][12]=next(b for b in post["blocks"] if b["name"]=="balance_prior")["words"][0]

    @classmethod
    def tearDownClass(cls): cls.harness.tearDownClass()

    def fixture(self): return copy.deepcopy(self.events),copy.deepcopy(self.marker)

    @staticmethod
    def event(events,kind): return next(e for e in events if e["event"]==kind)

    @staticmethod
    def block(event,name): return next(b for b in event["blocks"] if b["name"]==name)

    def test_exact_raw_row_order_with_independent_parent_envelope(self):
        events,marker=self.fixture();balances,selections,provenance=reader.paired_rows(events,marker)
        pre=self.event(events,"balance_pre")
        self.assertEqual(balances,[tuple(pre["object_words"][25:29]+[0x3f000001,0x3f000002,0xbe000001,0xbe000002])])
        self.assertEqual(selections,[(64,6,*range(500,513))])
        self.assertEqual(provenance[0]["call_id"],1)
        self.assertEqual(provenance[0]["selection_index"],6)

    def test_zero_bit_zero_and_high_flags_do_not_invent_parameter_entries(self):
        for flags,index in ((0,0),(1,0),(0xfffffc01,0),(2,1),(0x202,9),(0x80000100,8)):
            events,marker=self.fixture()
            for kind in ("selection_pre","selection_post"):
                self.block(self.event(events,kind),"selection_flags")["words"]=[flags]
            self.event(events,"selection_post")["registers"][10]=index
            with self.subTest(flags=flags):
                self.assertEqual(reader.paired_rows(events,marker)[1][0][:2],(flags,index))

    def test_balance_rejects_unbound_inputs_domain_and_unrelated_mutations(self):
        for change in ("object","pair","vector","output","prior","prior-register","domain","proof","missing"):
            events,marker=self.fixture();pre=self.event(events,"balance_pre");post=self.event(events,"balance_post")
            if change=="object": post["object_words"][0]^=1
            if change=="pair": self.block(post,"balance_pairs")["words"][0]^=1
            if change in ("vector","output","prior"):
                self.block(post,"balance_"+change)["words"][0]^=1
            if change=="prior-register": post["registers"][12]^=1
            if change=="domain":
                for event in (pre,post):
                    self.block(event,"balance_pairs")["words"][0]=0x49800001
                    event["object_words"][25]=0x49800001
            if change=="proof": pre["return_frame"]["words"][2]^=4
            if change=="missing": events.remove(post)
            with self.subTest(change=change),self.assertRaises(ValueError): reader.paired_rows(events,marker)
        events,marker=self.fixture()
        for kind in ("balance_pre","balance_post"):
            event=self.event(events,kind);event["object_words"][25]=1
            self.block(event,"balance_pairs")["words"][0]=1
        self.assertEqual(reader.paired_rows(events,marker)[0][0][0],1)  # Subnormal inputs stay within the declared domain.

    def test_selection_rejects_changed_flags_index_destination_or_copy_witness(self):
        for change in ("flags","index","destination","last","quartet","object","count"):
            events,marker=self.fixture();post=self.event(events,"selection_post")
            if change=="flags": self.block(post,"selection_flags")["words"][0]^=1
            if change=="index": post["registers"][10]=5
            if change=="destination": post["registers"][17]+=4
            if change=="last": post["registers"][20]^=1
            if change=="quartet": post["registers"][4]^=1
            if change=="object": post["object_words"][0]^=1
            if change=="count": marker["vehicle_selection_samples"]+=1
            with self.subTest(change=change),self.assertRaises(ValueError): reader.paired_rows(events,marker)


if __name__=="__main__": unittest.main()
