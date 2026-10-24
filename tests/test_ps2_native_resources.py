"""Portable checks of the fixed original resource fixture build boundary.

These tests do not run the GS, simulate SDK calls or prove pixel correctness;
the separately built EE fixture performs its controlled runtime checks.
"""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "resource_fixture", ROOT / "platform/ps2/tools/build_native_resource_fixture.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)


class ResourceFixtureBuildTests(unittest.TestCase):
    def test_fixed_original_sources_only(self):
        manifest = FIXTURE.fixture_spec()
        self.assertEqual(set(manifest["sources"]), {
            "platform/ps2/tests/native_resources_fixture.cpp",
            "platform/ps2/src/native_display.cpp", "platform/ps2/src/native_frame.cpp",
            "platform/ps2/src/vu0_pack.cpp"})
        self.assertEqual(manifest["defines"], [])
        self.assertEqual(manifest["compile_flags"], [])
        self.assertFalse(manifest["embed_iop_modules"])
        for source in manifest["sources"]:
            self.assertTrue((ROOT / source).is_file())

    def test_wrappers_affect_only_the_fixture_link(self):
        command = ["ee-g++", "input.o", "-o", "fixture.elf"]
        linked = FIXTURE.fixture_command(command, "link")
        expected = {"packet_init", "packet_free", "memalign", "free",
                    "dma_channel_initialize", "graph_vram_allocate",
                    "graph_vram_clear", "graph_initialize"}
        self.assertEqual(set(FIXTURE.WRAPPED_SYMBOLS), expected)
        self.assertEqual(linked[:len(command)], command)
        self.assertEqual(set(linked[len(command):]), {"-Wl,--wrap=" + name for name in expected})
        self.assertEqual(command, ["ee-g++", "input.o", "-o", "fixture.elf"])
        for label in ("compile-native_display", "compiler-version", "size", "native-symbol-boundary"):
            self.assertEqual(FIXTURE.fixture_command(command, label), command)

    def check_post_link_report(self, drift):
        # Run the real fixture orchestration/output writer with compilation
        # replaced at its boundary. Its helper first reports a successful link,
        # then the actual post-link checks must override that status on drift.
        paths = FIXTURE.module(FIXTURE.PATH_HELPER, "resource_test_paths")
        helpers = FIXTURE.module(FIXTURE.BUILD_HELPER, "resource_test_helpers")

        def linked(args, report):
            report["status"] = "cross_compiled"
            if drift == "manifest":
                args.core_manifest.write_text("{}\n")

        verifier_error = RuntimeError("late source drift") if drift == "source" else None
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ps2-resource-report-", dir=ROOT / "build") as directory:
            with mock.patch.object(FIXTURE, "module", side_effect=[paths, helpers]), \
                 mock.patch.object(helpers, "build", side_effect=linked) as compile_mock, \
                 mock.patch.object(helpers, "verify_source_snapshot", side_effect=verifier_error) as verify_mock:
                if drift:
                    expected = "late source drift" if drift == "source" else "manifest changed"
                    with self.assertRaisesRegex(RuntimeError, expected):
                        FIXTURE.build(Path(directory))
                else:
                    FIXTURE.build(Path(directory))
                compile_mock.assert_called_once()
                verify_mock.assert_called_once()
            report = json.loads((Path(directory) / "build-report.json").read_text())
            self.assertEqual(report["status"], "build_failed" if drift else "cross_compiled_runtime_unverified")
            self.assertFalse(report["ps2_hardware_tested"])

    def test_late_source_drift_records_failure(self):
        self.check_post_link_report("source")

    def test_late_manifest_drift_records_failure(self):
        self.check_post_link_report("manifest")

    def test_unchanged_post_link_inputs_record_only_build_success(self):
        self.check_post_link_report(None)


if __name__ == "__main__":
    unittest.main()
