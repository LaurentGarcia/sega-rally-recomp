"""Portable original input-wrapper branch and atomicity regression checks."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SOURCES=("main_update_entry","segment_selection","contact_projection","cabinet_steering_filter","cabinet_steering")


class MainEntryTests(unittest.TestCase):
    def test_input_filter_bypass_signed_miss_bitfield_and_atomic_failures(self):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: self.skipTest("C++20 compiler required")
        (ROOT/"build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="main-entry-test-",dir=ROOT/"build") as temporary:
            exe=Path(temporary)/"test"
            command=[compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-fno-fast-math","-ffp-contract=off",
                "-I",str(ROOT/"platform/ps2/native"),*[str(ROOT/f"platform/ps2/native/{s}.cpp") for s in SOURCES],
                str(ROOT/"platform/ps2/tests/main_update_entry_test.cpp"),"-o",str(exe)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=90)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(result.stdout,"MAIN_UPDATE_ENTRY_TEST PASS checks=14\n")


if __name__=="__main__": unittest.main()
