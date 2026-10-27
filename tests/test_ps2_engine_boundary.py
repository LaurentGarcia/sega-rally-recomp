#!/usr/bin/env python3
"""Enforce project dependency direction and independent engine reuse without ROMs."""
import contextlib
import importlib.util
import io
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ps2_engine_build_driver", ROOT / "platform/ps2/build_ps2.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class EngineDependencyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ps2-engine-boundary-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.patched_root = mock.patch.object(driver, "ROOT", self.root)
        self.patched_root.start()
        self.addCleanup(self.patched_root.stop)
        self.engine = self.put("platform/ps2/src/native_geometry.cpp")
        self.engine_header = self.put("platform/ps2/include/native_geometry.h")
        self.game = self.put("platform/ps2/native/car_pose.cpp")
        self.game_header = self.put("platform/ps2/native/car_pose.h")
        self.study = self.put("platform/ps2/src/native_scene.cpp")
        self.study_header = self.put("platform/ps2/include/native_scene.h")
        self.native = self.put("platform/ps2/native/main.cpp")
        self.demo = self.put("platform/ps2/samples/engine_demo/main.cpp")

    def put(self, name, text="// original fixture\n"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def include(self, source, dependency):
        source.write_text('#include "' + str(dependency) + '"\n')

    def test_permitted_roles_keep_game_and_study_out_of_engine(self):
        for source, dependency, target in (
                (self.engine, self.engine_header, "native-port"),
                (self.game, self.engine_header, "native-port"),
                (self.game, self.game_header, "native-port"),
                (self.study, self.engine_header, "native-port"),
                (self.study, self.study_header, "native-port"),
                (self.native, self.study_header, "native-port"),
                (self.native, self.game_header, "native-port"),
                (self.demo, self.engine_header, "engine-demo")):
            with self.subTest(source=source, dependency=dependency):
                self.include(source, dependency)
                result = driver.validate_native_boundary([source], target)
                self.assertTrue(result["dependency_directions_checked"])
                self.assertIn(driver.relative(dependency), result["checked_project_files"])

    def test_reverse_dependencies_are_rejected_even_inside_native_target(self):
        for source, dependency in ((self.engine, self.game_header), (self.engine, self.study_header),
                                   (self.engine, self.native), (self.game, self.study_header),
                                   (self.study, self.game_header), (self.game, self.native),
                                   (self.study, self.native)):
            with self.subTest(source=source, dependency=dependency):
                self.include(source, dependency)
                with self.assertRaisesRegex(ValueError, "dependency direction forbidden"):
                    driver.validate_native_boundary([source])
                source.write_text("// restored fixture\n")

    def test_allowed_app_cannot_launder_an_indirect_engine_dependency(self):
        self.include(self.native, self.engine_header)
        self.include(self.engine_header, self.game_header)
        with self.assertRaisesRegex(ValueError, "native_geometry.h.*engine.*car_pose.h.*game"):
            driver.validate_native_boundary([self.native])

    def test_unused_translation_unit_is_checked_before_linker_gc(self):
        self.include(self.engine, self.game_header)
        with self.assertRaisesRegex(ValueError, "dependency direction forbidden"):
            driver.validate_native_boundary([self.native, self.engine])

    def test_demo_rejects_game_study_and_other_app_sources(self):
        for dependency in (self.game, self.game_header, self.study, self.study_header, self.native):
            with self.subTest(dependency=dependency):
                self.include(self.demo, dependency)
                with self.assertRaisesRegex(ValueError, "forbids"):
                    driver.validate_native_boundary([self.demo], "engine-demo")
        self.demo.write_text("// restored fixture\n")
        self.include(self.native, self.demo)
        with self.assertRaisesRegex(ValueError, "forbids"):
            driver.validate_native_boundary([self.native])

    def test_offline_converter_and_unreviewed_native_files_are_not_trusted(self):
        for name in ("platform/ps2/tools/mesh_decode.h", "platform/ps2/native/new_machine.h",
                     "platform/ps2/include/secret_runtime.h"):
            dependency = self.put(name)
            self.include(self.native, dependency)
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "forbids"):
                driver.validate_native_boundary([self.native])

    def test_symlink_cannot_relabel_game_header_as_engine_header(self):
        self.engine_header.unlink()
        self.engine_header.symlink_to(self.game_header)
        self.include(self.engine, self.engine_header)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.validate_native_boundary([self.engine])
        self.include(self.demo, self.engine_header)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.validate_native_boundary([self.demo], "engine-demo")

    def test_declared_engine_source_cannot_redirect_to_allowed_game_source(self):
        frame = self.put("platform/ps2/src/native_frame.cpp")
        progress = self.put("platform/ps2/native/race_progress.cpp")
        frame.unlink()
        frame.symlink_to(progress)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.validate_native_boundary([self.native, frame, progress])

    def test_symlink_parent_traversal_cannot_hide_the_actual_include_identity(self):
        hidden = self.put("platform/ps2/native/native_geometry.h")
        child = hidden.parent / "child"
        child.mkdir()
        (self.engine_header.parent / "alias").symlink_to(child, target_is_directory=True)
        self.engine.write_text('#include "../include/alias/../native_geometry.h"\n')
        actual = self.engine.parent / "../include/alias/../native_geometry.h"
        self.assertEqual(actual.resolve(), hidden)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path.*native/native_geometry.h"):
            driver.validate_native_boundary([self.engine])

    def test_smoke_must_check_declared_source_before_resolving_it(self):
        self.engine.unlink()
        self.engine.symlink_to(self.game)
        args = SimpleNamespace(target="native-port", core_manifest=None)
        with mock.patch.object(driver, "NATIVE_SOURCES", [driver.relative(self.native), "platform/ps2/src/native_geometry.cpp"]), \
             mock.patch.object(driver, "NATIVE_IOP_SOURCES", ()), \
             mock.patch.object(driver, "ENGINE_HEADERS", set()), \
             self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.smoke(args, {})

    def test_build_rejects_redirected_engine_source_before_starting_toolchain(self):
        self.engine.unlink()
        self.engine.symlink_to(self.game)
        args = SimpleNamespace(target="native-port", core_manifest=None)
        with mock.patch.object(driver, "NATIVE_SOURCES", [driver.relative(self.native), "platform/ps2/src/native_geometry.cpp"]), \
             mock.patch.object(driver, "Runner", side_effect=AssertionError("must reject before invoking toolchain")), \
             self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.build(args, {})

    def test_opaque_includes_remain_forbidden_in_each_role(self):
        for path, target in ((self.engine, "native-port"), (self.study, "native-port"),
                             (self.game, "native-port"), (self.demo, "engine-demo")):
            path.write_text('#define SECRET "secret.h"\n#include SECRET\n')
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "opaque include"):
                driver.validate_native_boundary([path], target)

    def test_assembler_include_cannot_import_game_or_binary_payload(self):
        source = self.put("platform/ps2/src/vu1_transform.vsm")
        source.write_text('.include "' + str(self.game_header) + '"\n')
        with self.assertRaisesRegex(ValueError, "dependency direction forbidden"):
            driver.validate_native_boundary([source])
        source.write_text('payload: .incbin "private.bin"\n')
        with self.assertRaisesRegex(ValueError, "binary or opaque includes"):
            driver.validate_native_boundary([source])

    def test_assembler_cannot_redirect_engine_source_or_transitive_header(self):
        source = self.put("platform/ps2/src/vu1_transform.vsm")
        source.unlink()
        source.symlink_to(self.game)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.validate_native_boundary([source])
        source.unlink()
        source.write_text('.include "' + str(self.engine_header) + '"\n')
        self.engine_header.unlink()
        self.engine_header.symlink_to(self.game_header)
        with self.assertRaisesRegex(ValueError, "redirected reviewed path"):
            driver.validate_native_boundary([source])


class ActualEngineInventoryTests(unittest.TestCase):
    def smoke(self, target):
        report = {}
        args = SimpleNamespace(target=target, core_manifest=None)
        with contextlib.redirect_stdout(io.StringIO()), \
             mock.patch.object(driver.subprocess, "run", side_effect=AssertionError("smoke must not compile or execute")):
            driver.smoke(args, report)
        return report

    def test_both_apps_share_the_complete_engine_identity_without_game_assets(self):
        native, demo = self.smoke("native-port"), self.smoke("engine-demo")
        self.assertEqual(native["shared_engine"], demo["shared_engine"])
        expected = set(driver.ENGINE_SOURCES) | set(driver.NATIVE_IOP_SOURCES)
        expected |= {"platform/ps2/include/" + name for name in driver.ENGINE_HEADERS}
        self.assertEqual(set(demo["shared_engine"]["source_sha256"]), expected)
        self.assertEqual(set(demo["native_source_boundary"]["project_file_roles"].values()), {"engine", "demo_app"})
        for name in demo["source_sha256"]:
            self.assertFalse(name.startswith(("platform/ps2/native/", "platform/ps2/tools/", "build/", "src/")), name)
            self.assertNotIn("native_mesh", name)
            self.assertNotIn("native_scene", name)
        self.assertFalse(demo["executable_run"])
        self.assertEqual(driver.elf_name("engine-demo"), "ps2-engine-demo.elf")
        self.assertEqual(driver.elf_name("native-port"), "segarally-ps2-native-port.elf")

    def test_engine_manifest_changes_for_engine_input_but_not_game_input(self):
        fingerprints = self.smoke("native-port")["source_sha256"]
        original = driver.shared_engine_manifest(fingerprints)
        fingerprints["platform/ps2/native/car_pose.cpp"] = "different game code"
        self.assertEqual(original, driver.shared_engine_manifest(fingerprints))
        fingerprints["platform/ps2/include/native_display.h"] = "different engine contract"
        self.assertNotEqual(original["manifest_sha256"], driver.shared_engine_manifest(fingerprints)["manifest_sha256"])

    def test_demo_inventory_cannot_be_replaced_by_core_manifest(self):
        args = SimpleNamespace(target="engine-demo", core_manifest=ROOT / "build/arbitrary.json")
        with self.assertRaisesRegex(ValueError, "core-manifest is forbidden"):
            driver.build_spec(args)
        args.core_manifest = None
        actual = driver.build_spec(args)
        self.assertEqual(actual["sources"], ["platform/ps2/samples/engine_demo/main.cpp", *driver.ENGINE_SOURCES])
        self.assertEqual(actual["includes"], ["platform/ps2/include"])
        self.assertTrue(actual["pcm_transport"])

if __name__ == "__main__":
    unittest.main()
