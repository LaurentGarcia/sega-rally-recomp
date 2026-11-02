"""Bounded GS page ownership with no dependence on private texture assets."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class TextureCache(unittest.TestCase):
    def test_page_ownership_eviction_and_identity(self):
        compiler=shutil.which("clang++") or shutil.which("g++")
        if not compiler:self.skipTest("C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="texture-cache-") as directory:
            executable=Path(directory)/"test"
            subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
                "-I",str(ROOT/"platform/ps2/include"),str(ROOT/"platform/ps2/tests/native_texture_cache_test.cpp"),
                "-o",str(executable)],capture_output=True,text=True,check=True)
            result=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
            self.assertIn("TEXTURE_CACHE PASS",result.stdout)
            self.assertIn("page_ownership=1 eviction=1 generation=1 atomic_rejection=1 no_allocations=1",result.stdout)
if __name__=="__main__":unittest.main()
