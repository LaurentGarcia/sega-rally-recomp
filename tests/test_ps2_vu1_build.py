#!/usr/bin/env python3
"""Output/source boundaries for the independent original VU1 fixture builder."""
import importlib.util
import io
from contextlib import redirect_stdout
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "vu1_fixture_builder", ROOT / "platform/ps2/tools/build_vu1_fixture.py")
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)


class Vu1BuildBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rally-vu1-boundary-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "repo"
        self.root.mkdir()

    def test_new_or_empty_build_subdirectory(self):
        path = self.root / "build/fixture"
        self.assertEqual(BUILDER.resolve_output(Path("build/fixture"), self.root), path)
        self.assertFalse(path.exists())
        path.mkdir(parents=True)
        self.assertEqual(BUILDER.resolve_output(path, self.root), path)

    def test_rejects_source_root_and_path_traversal(self):
        for name in (".", "build", "platform/ps2", "build/../../elsewhere"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                BUILDER.resolve_output(Path(name), self.root)

    def test_rejects_nonempty_directory_and_regular_file(self):
        path = self.root / "build/prior"
        path.mkdir(parents=True)
        evidence = path / "build-report.json"
        evidence.write_text("preserve me")
        with self.assertRaises(ValueError):
            BUILDER.resolve_output(path, self.root)
        with self.assertRaises(ValueError):
            BUILDER.resolve_output(evidence, self.root)
        self.assertEqual(evidence.read_text(), "preserve me")

    def test_rejects_symlink_escape(self):
        build = self.root / "build"
        build.mkdir()
        outside = self.root.parent / "outside"
        outside.mkdir()
        (build / "redirect").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError):
            BUILDER.resolve_output(Path("build/redirect/fixture"), self.root)

    def test_rejects_redirected_build_root(self):
        outside = self.root.parent / "outside"
        outside.mkdir()
        (self.root / "build").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError):
            BUILDER.resolve_output(Path("build/fixture"), self.root)

    def test_fixed_original_source_inventory_and_pinned_helpers(self):
        self.assertEqual(tuple(path.relative_to(ROOT).as_posix() for path in BUILDER.SOURCES), (
            "platform/ps2/tests/vu1/fixture.cpp", "platform/ps2/tests/vu1/transform.vsm",
            "platform/ps2/src/vu0_pack.cpp"))
        hashes = BUILDER.fingerprints()
        self.assertEqual(len(hashes), 7)
        self.assertIn("platform/ps2/include/vu0_pack.h", hashes)
        self.assertIn("platform/ps2/toolchain.lock.json", hashes)
        self.assertIn("platform/ps2/build_ps2.py", hashes)
        self.assertTrue(all(len(value) == 64 for value in hashes.values()))

    def test_dual_unit_diagnostic_requires_explicit_opt_in(self):
        output = self.root / "build/fixture"
        for arguments, enabled in (([], False), (["--dual-unit"], True)):
            with self.subTest(arguments=arguments):
                with patch.object(BUILDER, "resolve_output", return_value=output), \
                        patch.object(BUILDER, "build", return_value={"elf": {}}) as build, \
                        redirect_stdout(io.StringIO()):
                    self.assertEqual(BUILDER.main(arguments), 0)
                build.assert_called_once_with(output, enabled)


if __name__ == "__main__":
    unittest.main()
