#!/usr/bin/env python3
"""Portable enclosure/rejection tests; actual VU execution is a separate EE run."""
from pathlib import Path
import os
import subprocess
import struct
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class TransformTests(unittest.TestCase):
    def run_success(self, command, **kwargs):
        result=subprocess.run(command,capture_output=True,text=True,**kwargs)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return result

    def test_enclosure_and_atomic_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/"vu1-transform"
            command=[os.environ.get("CXX","c++"),"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                     "-fno-fast-math","-ffp-contract=off","-Iplatform/ps2/include",
                     "platform/ps2/tests/vu1_transform_test.cpp","platform/ps2/src/vu1_transform_math.cpp",
                     "platform/ps2/src/vu1_transform.cpp","platform/ps2/src/vu0_pack.cpp","-o",str(binary)]
            self.run_success(command,cwd=ROOT)
            run=self.run_success([binary])
            self.assertIn("VU1 ENGINE PASS",run.stdout)
            self.assertIn("batches=0",run.stdout) # host must never claim VU execution

    def test_corpus_results_require_complete_input(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory)
            binary=work/"corpus"
            self.run_success([os.environ.get("CXX","c++"),"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                            "-fno-fast-math","-ffp-contract=off","-Iplatform/ps2/include",
                            "platform/ps2/tests/vu1_transform_corpus.cpp","platform/ps2/src/vu1_transform_math.cpp",
                            "platform/ps2/src/vu1_transform.cpp","platform/ps2/src/vu0_pack.cpp","-o",str(binary)],
                           cwd=ROOT)
            matrix=struct.pack("<16f",*(1.0 if i%5==0 else 0.0 for i in range(16)))
            vertices=struct.pack("<12f",0.0,0.0,0.5,1.0, 0.25,-0.5,0.75,1.0, 2.0,0.0,0.5,1.0)
            record=lambda frame:struct.pack("<II",frame,3)+matrix+matrix+vertices+vertices
            source=work/"input.bin"
            output=work/"output.bin"
            complete=b"RLYVUC1\0"+struct.pack("<II",2,2)+record(1)+record(2)
            source.write_bytes(complete)
            run=self.run_success([binary,source,output])
            self.assertIn("hardware_batches=0",run.stdout)
            expected=(b"RLYVUO1\0"+struct.pack("<II",2,2)+struct.pack("<II",1,3)+vertices+
                      struct.pack("<II",2,3)+vertices+b"COMPLETE"+struct.pack("<I",2))
            self.assertEqual(output.read_bytes(),expected)
            # A late malformed record may leave a diagnostic prefix, never a
            # success footer that a complete-frame consumer could accept.
            source.write_bytes(complete[:-1])
            run=subprocess.run([binary,source,output],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertNotIn(b"COMPLETE",output.read_bytes())
            source.write_bytes(complete)
            run=subprocess.run([binary,source,source],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertEqual(source.read_bytes(),complete)
if __name__=="__main__":unittest.main()
