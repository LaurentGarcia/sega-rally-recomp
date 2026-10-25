"""ROM-free ordered native course search and payload preservation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ContactSearchTests(unittest.TestCase):
    def test_selection_order_payload_and_bounded_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="native-contact-search-") as directory:
            executable = Path(directory) / "search"
            sources = ["platform/ps2/native/contact_search.cpp", "platform/ps2/native/contact_neighborhood.cpp",
                       "platform/ps2/native/contact_projection.cpp", "platform/ps2/tests/contact_search_test.cpp"]
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT / "platform/ps2/native"),
                       *(str(ROOT / source) for source in sources), "-o", str(executable)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("CONTACT_SEARCH_HOST PASS cases=12288 hash=4513b3de87618b42", result.stdout)
            self.assertIn("CONTACT_SEARCH_BOUNDARIES PASS checks=12", result.stdout)


if __name__ == "__main__":
    unittest.main()
