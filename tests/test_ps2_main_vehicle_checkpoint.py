#!/usr/bin/env python3
"""Portable typed checkpoint checks with independently authored LE/CRC data."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[1]

class MainVehicleCheckpointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler:raise unittest.SkipTest("C++20 compiler unavailable")
        cls.directory=tempfile.TemporaryDirectory(prefix="rally-checkpoint-")
        cls.work=Path(cls.directory.name);cls.probe=cls.work/"checkpoint"
        subprocess.run([compiler,"-std=c++20","-O1","-Wall","-Wextra","-Werror",
                        "-I",str(ROOT/"platform/ps2/native"),
                        str(ROOT/"platform/ps2/native/main_vehicle_checkpoint.cpp"),
                        str(ROOT/"platform/ps2/tests/main_vehicle_checkpoint_test.cpp"),"-o",str(cls.probe)],check=True)

    @classmethod
    def tearDownClass(cls):cls.directory.cleanup()

    def run_file(self,data,mode):
        path=self.work/"original-fixture.bin";path.write_bytes(data)
        result=subprocess.run([str(self.probe),str(path),mode],text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result.stdout

    @staticmethod
    def wire(words=None):
        if words is None:
            words=[0]*180;words[0]=0x80000000;words[17]=1;words[-1]=0xabcdef12
        payload=struct.pack("<180I",*words)
        return b"RLYCAR1\0"+struct.pack("<6I",1,32,180,zlib.crc32(payload),3,0x23456789)+payload

    def test_native_roundtrip_and_atomic_failure(self):
        result=subprocess.run([str(self.probe)],text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn("bytes=752 words=180",result.stdout)

    def test_independent_format(self):
        self.assertIn("FILE PASS",self.run_file(self.wire(),"valid"))

    def test_wrong_lengths_headers_and_crc(self):
        original=self.wire()
        for data in (b"",original[:-1],original+b"\0",b"BADCAR1\0"+original[8:],original[:8]+struct.pack("<I",2)+original[12:],original[:32]+bytes(720)):
            with self.subTest(length=len(data)):self.assertIn("REJECT",self.run_file(data,"invalid"))

    def test_noncanonical_values_with_valid_crc(self):
        # Input starts atword5: seven32-bit fields, five bytes, then booleans.
        for index,value in ((12,256),(16,0x100),(17,2),(18,0xffffffff)):
            words=[0]*180;words[index]=value
            with self.subTest(index=index):self.assertIn("REJECT 4",self.run_file(self.wire(words),"invalid"))

if __name__=="__main__":unittest.main()
