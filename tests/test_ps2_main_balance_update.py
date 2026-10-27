#!/usr/bin/env python3
"""ROM-free composite balance safety checks and capture framing regressions."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import check_main_balance_update as checker


class BalanceUpdate(unittest.TestCase):
    def test_composite_controls_and_atomic_rejection(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="ps2-balance-") as temporary:
            executable=Path(temporary)/"test"
            command=[compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"platform/ps2/native"),
                     *(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in checker.NATIVE),
                     str(ROOT/"platform/ps2/tests/main_balance_update_test.cpp"),"-o",str(executable)]
            subprocess.run(command,check=True,capture_output=True,text=True)
            result=subprocess.run([str(executable)],check=True,capture_output=True,text=True)
            self.assertIn("branches=192 atomic_rejections=9",result.stdout)

    def test_record_builder_rejects_changed_block_identity(self):
        before={"frame":1,"ac":0,"registers":[0]*32,"fp_bits":[0]*4,
                "blocks":[{"name":"global","address":0x214120,"words":[1]}]}
        after={**before,"blocks":[{"name":"global","address":0x214124,"words":[1]}]}
        with self.assertRaisesRegex(ValueError,"identities"):
            checker.balance_records([[{},before,after,{}]])

    def test_record_builder_preserves_raw_payloads(self):
        before={"frame":1,"ac":0,"registers":[0]*32,"fp_bits":[0]*4,
                "blocks":[{"name":"global","address":0x214120,"words":[0x7fc12345,0x80000000]}]}
        after={**before,"blocks":[{"name":"global","address":0x214120,"words":[0xffffffff,1]}]}
        records=checker.balance_records([[{},before,after,{}]])
        self.assertEqual(records[:12],b"MBUPDT01\x01\0\0\0")
        self.assertEqual(records[-16:],bytes.fromhex("4523c17f00000080ffffffff01000000"))


if __name__=="__main__":unittest.main()
