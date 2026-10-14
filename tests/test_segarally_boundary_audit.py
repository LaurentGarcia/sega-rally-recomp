"""Synthetic provenance and pipeline wiring checks; no game ROM or execution."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("boundary_pipeline", ROOT / "scripts/segarally_recompile.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class BoundaryAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.build = self.root / "build/native"
        (self.build / "gen").mkdir(parents=True)
        (self.build / "objects").mkdir()
        for name in pipeline.BOUNDARY_SOURCE_ROOTS:
            source = self.root / name
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("// synthetic generation dependency\n")
        (self.root / "src/runtime/gen_support.h").write_text('#include "runtime/synthetic.h"\n')
        (self.root / "src/runtime/synthetic.h").write_text("// ABI fixture\n")
        for name in ("chunk_000", "gen_table"):
            (self.build / f"gen/{name}.cpp").write_text("// synthetic instruction_boundary_hook_compatible\n")
            (self.build / f"objects/{name}.o").write_bytes(b"synthetic object hash fixture")
        self.addCleanup(patch.stopall)
        patch.object(pipeline, "ROOT", self.root).start()
        patch.object(pipeline, "BUILD", self.build).start()
        self.reach = {"board": "model2a", "instruction_boundary_hook": True,
            "instruction_boundary_hook_version": 1, "reference_cycle_accounting": True,
            "reference_cycle_accounting_version": 1, "missing_template_count": 0,
            "chunk_count": 1, "instruction_count": 1, "native_template_count": 1,
            "indirect_sites": [], "invalid_stops": [], "unmapped_targets": []}
        (self.build / "reachability.json").write_text(json.dumps(self.reach))
        self.coverage = {"board": "model2a", "native_objects_compiled": True,
            "native_object_count": 2, "reachability": self.reach,
            "boundary_contract": pipeline.make_boundary_contract(self.build, self.reach)}

    def verify(self, objects=True):
        return pipeline.verify_boundary_contract(self.build, self.coverage, objects)

    def test_valid_source_and_object_contract(self):
        self.assertEqual(self.verify(), self.coverage["boundary_contract"])
        self.assertIn("src/runtime/synthetic.h", self.verify()["sources_sha256"])
        self.assertEqual(pipeline.boundary_compile_flags(["c++"]), ["-DM2_INSTRUCTION_BOUNDARY_HOOK"])

    def test_ordinary_generation_keeps_no_new_contract(self):
        (self.build / "gen/gen_table.cpp").write_text("// ordinary generated table\n")
        self.assertIsNone(pipeline.verify_boundary_contract(self.build, {"reachability": {}}))

    def test_stripped_or_wrong_contract_is_rejected(self):
        baseline = copy.deepcopy(self.coverage)
        for mutate in (
            lambda c: c.pop("boundary_contract"),
            lambda c: c["reachability"].pop("instruction_boundary_hook"),
            lambda c: (c.pop("boundary_contract"), c["reachability"].pop("instruction_boundary_hook")),
            lambda c: c["boundary_contract"].update(compile_definitions=[]),
            lambda c: c["boundary_contract"].update(version=2),
            lambda c: c.update(native_object_count=3),
            lambda c: c["reachability"].update(missing_template_count=1),
            lambda c: c["reachability"].update(native_template_count=0),
        ):
            self.coverage = copy.deepcopy(baseline)
            mutate(self.coverage)
            with self.assertRaises(ValueError):
                self.verify()

    def test_changed_transitive_header_or_missing_hash_is_rejected(self):
        (self.root / "src/runtime/synthetic.h").write_text("// changed ABI\n")
        with self.assertRaisesRegex(ValueError, "header ABI changed"):
            self.verify()
        self.coverage["boundary_contract"]["sources_sha256"].pop("src/runtime/synthetic.h")
        with self.assertRaises(ValueError):
            self.verify()

    def test_changed_source_and_native_object_are_rejected(self):
        obj = self.build / "objects/chunk_000.o"
        obj.write_bytes(b"stale object")
        with self.assertRaisesRegex(ValueError, "native objects differ"):
            self.verify()
        # Dedicated hosts freshly compile sources, so they do not link these
        # objects; their source/header check still must reject changed code.
        self.assertIsNotNone(self.verify(objects=False))
        (self.build / "gen/chunk_000.cpp").write_text("// changed generated source\n")
        with self.assertRaisesRegex(ValueError, "generated sources differ"):
            self.verify(objects=False)

    def test_added_artifact_and_symlink_escape_are_rejected(self):
        extra = self.build / "objects/unexpected.o"
        extra.write_bytes(b"extra")
        with self.assertRaisesRegex(ValueError, "exact chunk inventory"):
            self.verify()
        extra.unlink()
        original = self.build / "gen/chunk_000.cpp"
        outside = self.root / "outside.cpp"
        outside.write_bytes(original.read_bytes())
        original.unlink()
        original.symlink_to(outside)
        with self.assertRaisesRegex(ValueError, "escaped"):
            self.verify()

    def test_changed_report_or_object_inventory_is_rejected(self):
        self.coverage["boundary_contract"]["objects_sha256"].pop("chunk_000.o")
        with self.assertRaisesRegex(ValueError, "object audit inventory"):
            self.verify()
        self.coverage["boundary_contract"] = pipeline.make_boundary_contract(self.build, self.reach)
        (self.build / "reachability.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "reachability report differs"):
            self.verify()

    def test_inexact_compiler_definitions_are_rejected(self):
        for flags in (("-DM2_FAST_GEN",), ("-DM2_FAST_CPU=0",), ("-D", "M2_DC_SPEED"), ("-DM2_DC_MEMORY=1",)):
            with self.subTest(flags=flags), self.assertRaisesRegex(ValueError, "inexact compiler"):
                pipeline.boundary_compile_flags(["c++", *flags])

    def test_every_generated_compile_gets_matching_macro(self):
        commands = []
        with patch.object(pipeline, "run_logged", side_effect=lambda command, log: commands.append(command)):
            count = pipeline.compile_generated(["c++"], self.build / "gen", self.build / "objects", 1, True)
        self.assertEqual(count, 2)
        self.assertTrue(all(command.count("-DM2_INSTRUCTION_BOUNDARY_HOOK") == 1 for command in commands))

    def test_hook_flag_survives_fixed_point_regeneration(self):
        config = self.root / "config"
        config.mkdir()
        for name, value in (
            ("segarally_seeds.json", {"seeds": [{"address": "0x00000040", "name": "synthetic", "evidence": "fixture"}]}),
            ("segarally_observed_seeds.json", {"seeds": []}),
            ("segarally_relocations.json", {}), ("segarally_table_seeds.json", {})):
            (config / name).write_text(json.dumps(value))
        commands = []
        def extract(*args):
            images = self.build / "rom_cache"
            images.mkdir()
            (images / "program.bin").write_bytes(bytes(16))
            (images / "main_data.bin").write_bytes(bytes(16))
            return {"board": "model2a", "romset": "synthetic", "verified_files": [], "reset_ip": "0x40"}
        def generate(command, log):
            commands.append(command)
            for name in ("chunk_000", "gen_table"):
                (self.build / f"gen/{name}.cpp").write_text("// synthetic instruction_boundary_hook_compatible\n")
            (self.build / "reachability.json").write_text(json.dumps(self.reach))
        def compile_objects(compiler, generated, output, jobs, enabled):
            self.assertTrue(enabled)
            for name in ("chunk_000", "gen_table"):
                (output / f"{name}.o").write_bytes(b"synthetic compiled fixture")
            return 2
        continuation = {"address": "0x00000080", "source_address": "0x00000040"}
        with patch.object(sys, "argv", ["segarally_recompile.py", "--build-dir", str(self.build),
                "--instruction-boundary-hook", "--indexed-dispatch"]), \
             patch.object(pipeline, "load_manifest", return_value={"regions": []}), \
             patch.object(pipeline, "extract", side_effect=extract), \
             patch.object(pipeline, "build_generator", return_value=Path("synthetic-generator")), \
             patch.object(pipeline, "build_continuation_tool", return_value=Path("synthetic-proof")), \
             patch.object(pipeline, "table_seeds", return_value=[]), \
             patch.object(pipeline, "relocation_seeds", return_value=([], [])), \
             patch.object(pipeline, "continuation_seeds", side_effect=[[continuation], []]), \
             patch.object(pipeline, "object_update_seeds", return_value=[]), \
             patch.object(pipeline, "compile_generated", side_effect=compile_objects), \
             patch.object(pipeline, "run_logged", side_effect=generate):
            self.assertEqual(pipeline.main(), 0)
        self.assertEqual(len(commands), 2)
        self.assertTrue(all(command.count("--instruction-boundary-hook") == 1 for command in commands))
        self.assertTrue(all(command.count("--indexed-dispatch") == 1 for command in commands))
        coverage = json.loads((self.build / "coverage.json").read_text())
        self.assertIsNotNone(pipeline.verify_boundary_contract(self.build, coverage))

    def prepare_consumer_audit(self, compatible=True):
        self.coverage["romset"] = "srallycb"
        self.coverage["images"] = {}
        for name in ("program", "main_data"):
            path = self.build / (name + ".bin")
            path.write_bytes(b"synthetic image")
            self.coverage["images"][name] = {"path": str(path.relative_to(self.root)),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        if not compatible:
            self.coverage.pop("boundary_contract", None)
            self.coverage["reachability"].pop("instruction_boundary_hook", None)
            (self.build / "gen/gen_table.cpp").write_text("// ordinary table\n")
        (self.build / "coverage.json").write_text(json.dumps(self.coverage))

    def boot_fixture(self, enabled, compatible=True):
        spec = importlib.util.spec_from_file_location("boundary_boot", ROOT / "scripts/segarally_boot.py")
        boot = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {"segarally_recompile": pipeline}):
            spec.loader.exec_module(boot)
        self.prepare_consumer_audit(compatible)
        commands = []
        def run(command, **kwargs):
            commands.append(command)
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
        argv = ["segarally_boot.py", "--build-dir", str(self.build), "--with-control"]
        if enabled:
            argv.append("--instruction-boundary-hook")
        with patch.object(boot, "ROOT", self.root), patch.object(sys, "argv", argv), \
             patch.object(boot.subprocess, "run", side_effect=run):
            result = boot.main()
        return result, commands

    def test_boot_old_and_new_modes_link_the_same_compatible_abi(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled):
                result, commands = self.boot_fixture(enabled)
                self.assertEqual(result, 0)
                self.assertEqual(len(commands), 2)
                self.assertIn("-DM2_INSTRUCTION_BOUNDARY_HOOK", commands[0])
                self.assertEqual("--instruction-boundary-hook" in commands[1], enabled)
                report = json.loads((self.build / "boot.json").read_text())
                self.assertTrue(report["instruction_boundary_hook_build"])
                self.assertEqual(report["instruction_boundary_hook"], enabled)

    def test_boot_ordinary_mode_keeps_default_link_contract(self):
        result, commands = self.boot_fixture(False, compatible=False)
        self.assertEqual(result, 0)
        self.assertNotIn("-DM2_INSTRUCTION_BOUNDARY_HOOK", commands[0])
        self.assertNotIn("--instruction-boundary-hook", commands[1])
        self.assertNotIn("instruction_boundary_hook_build", json.loads((self.build / "boot.json").read_text()))

    def test_boot_rejects_runtime_hook_before_linking_ordinary_objects(self):
        result, commands = self.boot_fixture(True, compatible=False)
        self.assertEqual(result, 2)
        self.assertFalse(commands)

    def parity_fixture(self, compatible=True, forbid_processes=False):
        spec = importlib.util.spec_from_file_location("boundary_parity", ROOT / "tests/test_segarally_boot_parity.py")
        parity = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {"segarally_recompile": pipeline}):
            spec.loader.exec_module(parity)
        self.prepare_consumer_audit(compatible)
        commands = []
        def run(command, **kwargs):
            self.assertFalse(forbid_processes, "invalid object audit reached an external process")
            commands.append(command)
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
        argv = ["test_segarally_boot_parity.py", "--build-dir", str(self.build), "--control"]
        with patch.object(parity, "ROOT", self.root), patch.object(sys, "argv", argv), \
             patch.object(parity.subprocess, "run", side_effect=run):
            parity.main()
        return commands

    def test_independent_cpu_parity_links_hook_abi_with_callback_disabled(self):
        commands = self.parity_fixture()
        self.assertEqual(len(commands), 2)
        self.assertIn("-DM2_INSTRUCTION_BOUNDARY_HOOK", commands[0])
        self.assertNotIn("--instruction-boundary-hook", commands[1])
        report = json.loads((self.build / "boot-parity-build.json").read_text())
        self.assertTrue(report["instruction_boundary_hook_build"])
        self.assertFalse(report["instruction_boundary_callback_enabled"])
        self.assertFalse(report["model2a_machine_scheduling_tested"])

    def test_independent_cpu_parity_preserves_ordinary_default(self):
        commands = self.parity_fixture(compatible=False)
        self.assertNotIn("-DM2_INSTRUCTION_BOUNDARY_HOOK", commands[0])
        self.assertFalse((self.build / "boot-parity-build.json").exists())

    def test_independent_cpu_parity_rejects_stale_objects_before_link(self):
        (self.build / "objects/chunk_000.o").write_bytes(b"stale object")
        with self.assertRaisesRegex(ValueError, "native objects differ"):
            self.parity_fixture(forbid_processes=True)


if __name__ == "__main__":
    unittest.main()
