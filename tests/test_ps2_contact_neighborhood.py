"""Check recovered contact indexing without any original game data/runtime."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("contact_checker", ROOT / "platform/ps2/tools/check_contact_neighborhood.py")
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class ContactNeighborhoodTests(unittest.TestCase):
    def test_private_output_rejects_checkout_and_external_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            with patch.object(CHECKER, "ROOT", root):
                self.assertEqual(CHECKER.private_output(root / "build" / "new"), root / "build" / "new")
                for path in (root, root / "build", root / "outside", root / "build" / ".." / "escaped"):
                    with self.assertRaises(ValueError):
                        CHECKER.private_output(path)

    def test_private_output_rejects_root_or_nested_symlink_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "outside").mkdir()
            (root / "build").symlink_to(root / "outside", target_is_directory=True)
            with patch.object(CHECKER, "ROOT", root), self.assertRaises(ValueError):
                CHECKER.private_output(root / "build" / "new")
            (root / "build").unlink()
            (root / "build").mkdir()
            (root / "build" / "link").symlink_to(root / "outside", target_is_directory=True)
            with patch.object(CHECKER, "ROOT", root), self.assertRaises(ValueError):
                CHECKER.private_output(root / "build" / "link" / "new")

    def test_wrong_private_image_is_rejected_before_output_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            program, output = root / "original-placeholder.bin", root / "output"
            program.write_bytes(bytes(32))
            result = subprocess.run([shutil.which("python3"), str(ROOT / "platform/ps2/tools/check_contact_neighborhood.py"),
                                     "--program", str(program), "--output", str(output)],
                                    capture_output=True, text=True, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not the verified Revision B", result.stderr)
            self.assertFalse(output.exists())

    def test_signed_index_domain_count_bounds_and_atomic_rejection(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="rally-contact-neighborhood-") as directory:
            executable = Path(directory) / "test"
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-I", str(ROOT / "platform/ps2/native"),
                       str(ROOT / "platform/ps2/native/contact_neighborhood.cpp"),
                       str(ROOT / "platform/ps2/tests/contact_neighborhood_test.cpp"),
                       "-o", str(executable)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            tested = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(tested.returncode, 0, tested.stdout + tested.stderr)
            self.assertIn("NATIVE_CONTACT_NEIGHBORHOOD PASS", tested.stdout)


if __name__ == "__main__":
    unittest.main()
