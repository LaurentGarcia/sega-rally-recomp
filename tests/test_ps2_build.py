#!/usr/bin/env python3
"""Reject invalid executable targets and unaudited PS2 core manifests."""

import ast
import contextlib
import fnmatch
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


driver = load("ps2_build_test_driver", "platform/ps2/build_ps2.py")
prepare = load("ps2_prepare_test_driver", "platform/ps2/prepare_core.py")
checkpoints = load("ps2_checkpoint_test_driver", "platform/ps2/tools/compare_checkpoints.py")


def executable(*, machine=8, kind=2, version=1, entry=0x100000,
               address=0x100000, file_offset=84, file_size=16, memory_size=64,
               flags=5, phoff=52, phsize=32, phnum=1, segment_type=1):
    # Independently construct the ELF32 header and one PT_LOAD program header.
    data = bytearray(128)
    data[:7] = b"\x7fELF\x01\x01\x01"
    struct.pack_into("<HHIIIIIHHHHHH", data, 16, kind, machine, version, entry,
                     phoff, 0, 0x20924001, 52, phsize, phnum, 0, 0, 0)
    struct.pack_into("<IIIIIIII", data, 52, segment_type, file_offset, address,
                     address, file_size, memory_size, flags, 128)
    return data


class ElfTargetTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rally-ps2-elf-")
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "candidate.elf"

    def inspect(self, data):
        self.path.write_bytes(data)
        return driver.inspect_elf(self.path)

    def test_reports_resident_bytes_separately_from_file_and_dynamic_memory(self):
        report = self.inspect(executable())
        self.assertEqual(report["file_bytes"], 128)
        self.assertEqual(report["load_segment_memory_bytes"], 64)
        self.assertEqual(report["ram_high_water_bytes"], 0x100040)
        self.assertEqual(report["remaining_before_stack_heap_bytes"], 0x2000000 - 0x100040)
        self.assertFalse(report["runtime_allocations_included"])
        self.assertEqual(report["sha256"], hashlib.sha256(self.path.read_bytes()).hexdigest())

    def test_rejects_wrong_architecture_and_nonexecutable_objects(self):
        for options in ({"machine": 62}, {"machine": 183}, {"kind": 1}, {"version": 2}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.inspect(executable(**options))

    def test_rejects_magic_wordsize_endianness_and_truncated_header(self):
        candidates = [b"", b"not an ELF", executable()[:51]]
        for offset, value in ((0, 0), (4, 2), (5, 2), (6, 0)):
            data = executable()
            data[offset] = value
            candidates.append(data)
        for candidate in candidates:
            with self.subTest(length=len(candidate)), self.assertRaises(ValueError):
                self.inspect(candidate)

    def test_rejects_invalid_program_header_and_payload_bounds(self):
        for options in ({"phoff": 0xffffffff}, {"phsize": 16}, {"phnum": 0},
                        {"phnum": 65535}, {"file_offset": 0xffffffff},
                        {"file_size": 65}, {"file_offset": 120, "file_size": 16},
                        {"segment_type": 0}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.inspect(executable(**options))

    def test_rejects_outside_ee_user_ram_and_unsigned_address_wrap(self):
        for address, count in ((0x0fffff, 64), (0x1fffff0, 17),
                               (0x2000000, 64), (0xfffffff0, 64)):
            with self.subTest(address=address, memory=count), self.assertRaises(ValueError):
                self.inspect(executable(address=address, entry=address,
                                        file_size=16, memory_size=count))

    def test_accepts_exact_static_ram_limit_without_claiming_heap_space(self):
        report = self.inspect(executable(address=0x1fffff0, entry=0x1fffff0, memory_size=16))
        self.assertEqual(report["remaining_before_stack_heap_bytes"], 0)
        self.assertFalse(report["runtime_allocations_included"])

    def test_entry_must_belong_to_executable_load_segment(self):
        for options in ({"entry": 0x100040}, {"entry": 0x0fffff}, {"flags": 6}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.inspect(executable(**options))


class DvpAssemblyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ps2-dvp-build-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.output = self.root / "build/output"
        self.source = self.root / "transform.vsm"
        self.header = self.root / "layout.inc"
        self.source.write_text('.include "layout.inc"\n.vu\nnop[e] nop\nnop nop\n')
        self.header.write_text('.equ COUNT, 1\n')
        self.cpp = self.root / "driver.cpp"
        self.cpp.write_text('int main() { return 0; }\n')
        self.args = SimpleNamespace(target="core-probe", output=self.output, jobs=1,
                                    optimization="O2", backend="native", ps2dev=Path("/pinned/ps2dev"),
                                    container_platform=None)
        self.commands = []
        self.version = "GNU assembler (DVP) 2.45.1"
        owner = self
        class FakeRunner(driver.Runner):
            def identity(self):
                return {"compiler_version": "test-EE", "compiler_target": "mips64r5900el-ps2-elf"}

            def run(self, command, name, container=True):
                owner.commands.append((name, command))
                if name == "dvp-assembler-version":
                    return owner.version
                if name == "size":
                    return "synthetic test ELF"
                output = Path(command[command.index("-o") + 1])
                if name == "link":
                    output.write_bytes(executable())
                else:
                    output.write_bytes((name + owner.version).encode() + owner.header.read_bytes())
                    for arg in command:
                        if arg.startswith("-al="):
                            Path(arg[4:]).write_text("original fixture listing\n")
                return ""
        self.patches = [mock.patch.object(driver, "ROOT", self.root),
                        mock.patch.object(driver, "Runner", FakeRunner),
                        mock.patch.object(driver, "build_spec", return_value={
                            "sources": [str(self.source), str(self.cpp)], "includes": [],
                            "defines": [], "compile_flags": [], "libraries": [], "embed_iop_modules": False})]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def build(self):
        report = {}
        with contextlib.redirect_stdout(io.StringIO()):
            driver.build(self.args, report)
        return report

    def test_dvp_tool_listing_and_transitive_input_are_bound(self):
        report = self.build()
        commands = [command for name, command in self.commands if name.startswith("assemble-")]
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][0], "/pinned/ps2dev/dvp/bin/dvp-as")
        self.assertFalse(any(arg.startswith(("-std=", "-O", "-D", "-ff")) for arg in commands[0]))
        self.assertEqual(report["toolchain"]["dvp_assembler_version"], self.version)
        self.assertEqual(report["compile_dependencies"]["transform.vsm"], ["layout.inc"])
        self.assertEqual(report["source_sha256"]["layout.inc"], driver.digest(self.header))
        obj = report["dvp_objects"]["transform.vsm"]
        self.assertEqual(obj["sha256"], driver.digest(self.root / obj["object"]))
        self.assertEqual(obj["listing_sha256"], driver.digest(self.root / obj["listing"]))

    def test_cache_invalidates_changed_include_assembler_or_listing_only(self):
        report = self.build()
        self.commands.clear()
        self.build()
        self.assertFalse(any(name.startswith(("compile-", "assemble-")) for name, _ in self.commands))
        for mutation in (lambda: self.header.write_text('.equ COUNT, 2\n'),
                         lambda: setattr(self, "version", "GNU assembler (DVP) changed"),
                         lambda: (self.root / report["dvp_objects"]["transform.vsm"]["listing"]).write_text("tampered")):
            self.commands.clear()
            mutation()
            self.build()
            self.assertEqual(sum(name.startswith("assemble-") for name, _ in self.commands), 1)
            self.assertFalse(any(name.startswith("compile-") for name, _ in self.commands))

    def test_binary_opaque_and_missing_assembly_inputs_are_rejected(self):
        for text in ('.incbin "private.bin"\n', 'label: .incbin "private.bin"\n',
                     '.include INPUT\n', '.include "absent.inc"\n'):
            self.source.write_text(text)
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.project_headers([self.source], [])

    def test_nested_assembler_inputs_use_the_same_search_path_as_as(self):
        nested = self.root / "sub"
        nested.mkdir()
        (nested / "first.inc").write_text('.include "layout.inc"\n')
        (nested / "layout.inc").write_text('unrelated file must not shadow assembler search path\n')
        self.source.write_text('.include "sub/first.inc"\n')
        self.assertEqual(driver.project_headers([self.source], []), [self.header, nested / "first.inc"])


class CoreProvenanceTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="ps2-prepare-test-", dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.native = self.directory / "native"
        self.native.mkdir()
        for child in ("gen", "tgp", "video"):
            (self.native / child).mkdir()
        self.output = self.directory / "core.json"
        self.output.write_text('{"existing": true}\n')

    def audits(self, boundary=False):
        reach = {"board": "model2a", "reference_cycle_accounting": True,
                 "reference_cycle_accounting_version": 1, "chunk_count": 1,
                 "instruction_count": 1, "native_template_count": 1,
                 "missing_template_count": 0}
        if boundary:
            reach.update(instruction_boundary_hook=True, instruction_boundary_hook_version=1)
        coverage = {"board": "model2a", "romset": "srallycb", "native_objects_compiled": True,
                    "native_object_count": 2, "reachability": reach}
        (self.native / "reachability.json").write_text(json.dumps(reach))
        if boundary:
            coverage["boundary_contract"] = {
                "version": 1, "compile_definitions": ["M2_INSTRUCTION_BOUNDARY_HOOK"],
                "reachability_sha256": hashlib.sha256((self.native / "reachability.json").read_bytes()).hexdigest(),
                "sources_sha256": {},
            }
        values = {"coverage.json": coverage,
                  "tgp/audit.json": {"romset": "srallycb", "native_code_compiled": True},
                  "video/audit.json": {"romset": "srallycb", "manifest_sha256":
                                       hashlib.sha256((ROOT / "config/segarally_video.json").read_bytes()).hexdigest()}}
        for name, data in values.items():
            (self.native / name).write_text(json.dumps(data))
        return values

    def test_refuses_generated_code_without_boundary_compatibility(self):
        self.audits()
        with self.assertRaisesRegex(ValueError, "instruction-boundary-hook"):
            prepare.prepare(self.native, self.output)
        self.assertEqual(self.output.read_text(), '{"existing": true}\n')

    def test_refuses_changed_source_or_header_contract_without_replacing_manifest(self):
        self.audits(boundary=True)
        with self.assertRaisesRegex(ValueError, "source or header ABI changed"):
            prepare.prepare(self.native, self.output)
        self.assertEqual(self.output.read_text(), '{"existing": true}\n')

    def test_rejects_wrong_board_revision_or_uncompiled_tgp(self):
        for file, key, value in (("coverage.json", "board", "model2b"),
                                 ("coverage.json", "romset", "srallyc"),
                                 ("tgp/audit.json", "native_code_compiled", False)):
            with self.subTest(file=file, key=key):
                audits = self.audits()
                audits[file][key] = value
                (self.native / file).write_text(json.dumps(audits[file]))
                with self.assertRaisesRegex(ValueError, "Revision B Model2A"):
                    prepare.prepare(self.native, self.output)

    def test_preparation_cannot_export_or_overwrite_audited_input_directory(self):
        for output in (ROOT / "platform/ps2/core.json", ROOT / "build/../core.json",
                       self.native / "core.json", self.directory / "core.cpp"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                prepare.prepare(self.native, output)
        escaped = self.directory / "outside"
        escaped.symlink_to(ROOT / "platform/ps2", target_is_directory=True)
        with self.assertRaises(ValueError):
            prepare.prepare(self.native, escaped / "core.json")


class CheckpointComparisonTests(unittest.TestCase):
    HEADER = "frame,instructions,cycles,board_ns,pc,register_hash,screen_hash\n"
    FIRST = "1,10,20,30,00000040,0000000000000050,0000000000000060\n"
    SECOND = "2,11,21,31,00000041,0000000000000051,0000000000000061\n"
    THIRD = "3,12,22,32,00000042,0000000000000052,0000000000000062\n"

    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="ps2-checkpoint-test-", dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.reference = self.directory / "reference.csv"
        self.guest = self.directory / "guest.log"
        self.reference.write_text(self.HEADER + self.FIRST + self.SECOND + self.THIRD)
        self.guest.write_text("GS ready\n" + self.HEADER + "controller state=1\n" + self.FIRST +
                              "heap stage=ready arena=123\n" + self.THIRD + "still running\n")

    def test_sparse_mixed_log_records_exact_input_identity_and_partial_scope(self):
        report = checkpoints.evidence(self.reference, self.guest)
        self.assertEqual(report["matched_frames"], [1, 3])
        self.assertEqual(report["matched_checkpoint_count"], 2)
        self.assertTrue(report["partial_run_evidence"])
        self.assertFalse(report["complete_reference_coverage"])
        self.assertFalse(report["physical_ps2_execution_verified"])
        self.assertFalse(report["arcade_fidelity_certified"])
        self.assertFalse(report["performance_measured"])
        for name, path in (("reference", self.reference), ("guest", self.guest)):
            self.assertEqual(report["inputs"][name]["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(report["inputs"][name]["bytes"], path.stat().st_size)

    def test_rejects_a_difference_in_every_nonframe_field(self):
        for index, field in enumerate(checkpoints.FIELDS[1:], 1):
            fields = self.FIRST.strip().split(",")
            fields[index] = "9" if index < 4 else "f" * len(fields[index])
            self.guest.write_text(",".join(fields) + "\n")
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field + " mismatch"):
                checkpoints.evidence(self.reference, self.guest)

    def test_rejects_missing_reference_frame(self):
        self.reference.write_text(self.HEADER + self.FIRST + self.SECOND)
        with self.assertRaisesRegex(ValueError, "frame 3: missing reference"):
            checkpoints.evidence(self.reference, self.guest)

    def test_rejects_duplicate_and_out_of_order_frames_in_either_input(self):
        for mixed in (False, True):
            for rows in (self.FIRST + self.FIRST, self.SECOND + self.FIRST):
                with self.subTest(mixed=mixed, rows=rows), self.assertRaisesRegex(ValueError, "duplicate or out-of-order"):
                    checkpoints.parse_checkpoints((self.HEADER + rows).encode(), mixed_log=mixed)

    def test_rejects_malformed_rows_counters_headers_and_empty_runs(self):
        bad_rows = (self.FIRST.rsplit(",", 1)[0], self.FIRST.strip() + ",extra",
                    self.FIRST.replace("00000040", "0000004z"),
                    self.FIRST.replace("0000000000000060", "60"),
                    self.FIRST.replace("1,", "-1,", 1), self.FIRST.replace("1,", "0,", 1),
                    self.FIRST.replace("1,", "4294967296,", 1),
                    self.FIRST.replace(",10,", ",18446744073709551616,"))
        for row in bad_rows:
            with self.subTest(row=row), self.assertRaises(ValueError):
                checkpoints.parse_checkpoints((self.HEADER + self.FIRST + row).encode(), mixed_log=True)
        for text, mixed in (("GS ready\n", True), (self.HEADER, False),
                            (self.FIRST, False), (self.HEADER + self.FIRST + self.HEADER, True),
                            ("frame,bad,header\n" + self.FIRST, True)):
            with self.subTest(text=text, mixed=mixed), self.assertRaises(ValueError):
                checkpoints.parse_checkpoints(text.encode(), mixed_log=mixed)

    def test_report_is_exclusive_and_confined_even_through_symlinks(self):
        report = checkpoints.evidence(self.reference, self.guest)
        output = self.directory / "report.json"
        checkpoints.write_report(output, report)
        original = output.read_bytes()
        with self.assertRaisesRegex(ValueError, "overwrite"):
            checkpoints.write_report(output, {"replacement": True})
        self.assertEqual(output.read_bytes(), original)
        with self.assertRaises(ValueError):
            checkpoints.write_report(ROOT / "ps2-test-report.json", report)
        with self.assertRaises(ValueError):
            checkpoints.write_report(self.directory / "report.txt", report)
        (self.directory / "outside").symlink_to(ROOT / "platform/ps2", target_is_directory=True)
        with self.assertRaises(ValueError):
            checkpoints.write_report(self.directory / "outside/report.json", report)
        dangling = self.directory / "dangling.json"
        dangling.symlink_to(self.directory / "absent.json")
        with self.assertRaises(ValueError):
            checkpoints.write_report(dangling, report)

    def test_cli_failure_does_not_publish_a_success_report(self):
        self.guest.write_text(self.FIRST.replace(",10,", ",11,"))
        output = self.directory / "must-not-exist.json"
        result = subprocess.run([sys.executable, str(ROOT / "platform/ps2/tools/compare_checkpoints.py"),
                                 "--reference", str(self.reference), "--guest-log", str(self.guest),
                                 "--output", str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("instructions mismatch", result.stderr)
        self.assertFalse(output.exists())


class BuildBoundaryTests(unittest.TestCase):
    def test_build_cannot_report_success_when_sources_change_during_compilation(self):
        with tempfile.TemporaryDirectory(prefix="ps2-build-snapshot-") as temporary:
            root = Path(temporary)
            source = root / "source.cpp"
            source.write_text("original input\n")
            fingerprints = {"source.cpp": driver.digest(source)}
            with mock.patch.object(driver, "ROOT", root):
                driver.verify_source_snapshot(fingerprints)
                source.write_text("changed input\n")
                with self.assertRaisesRegex(RuntimeError, "rerun incrementally"):
                    driver.verify_source_snapshot(fingerprints)
                source.unlink()
                with self.assertRaisesRegex(RuntimeError, "source.cpp"):
                    driver.verify_source_snapshot(fingerprints)

    def test_actual_native_inventory_and_transitive_headers_are_reference_free(self):
        args = type("Args", (), {"target": "native-port", "core_manifest": None})()
        spec = driver.build_spec(args)
        sources = [ROOT / name for name in spec["sources"] + list(driver.NATIVE_IOP_SOURCES)]
        headers = driver.project_headers(sources, [ROOT / name for name in spec["includes"]])
        boundary = driver.validate_native_boundary(sources + headers)
        self.assertFalse(boundary["arcade_machine_runtime_linked"])
        self.assertFalse(boundary["translated_processors_linked"])
        self.assertIn("platform/ps2/native/race_progress.cpp", boundary["checked_project_files"])

    def test_native_smoke_checks_sources_without_running_reference_raster_or_compiler(self):
        with tempfile.TemporaryDirectory(prefix="ps2-native-smoke-") as temporary:
            root = Path(temporary).resolve()
            main = root / "platform/ps2/native/main.cpp"
            main.parent.mkdir(parents=True)
            main.write_text("int main() { return 0; }\n")
            args = type("Args", (), {"target": "native-port", "core_manifest": None})()
            report = {}
            with mock.patch.object(driver, "ROOT", root), \
                 mock.patch.object(driver, "NATIVE_SOURCES", ["platform/ps2/native/main.cpp"]), \
                 mock.patch.object(driver, "NATIVE_IOP_SOURCES", ()), \
                 mock.patch.object(driver, "ENGINE_HEADERS", set()), \
                 mock.patch.object(driver.subprocess, "run", side_effect=AssertionError("unexpected compiler/reference execution")), \
                 contextlib.redirect_stdout(io.StringIO()):
                driver.smoke(args, report)
            self.assertEqual(report["status"], "native_source_boundary_passed")
            self.assertFalse(report["executable_run"])
            self.assertEqual(list(report["source_sha256"]), ["platform/ps2/native/main.cpp"])

    def test_native_defined_symbols_cannot_link_known_processor_or_machine_implementations(self):
        driver.validate_native_symbols("00100000 T main\n00101000 T ps2::AudioOutput::initialize()\n00102000 T native::RaceProgress::update()\n")
        for symbol in ("rt::Model2AMachine::run_until_frame(unsigned long)", "rt::Cpu::reset()",
                       "rt::Model2UART::clock()", "rt::Model2AClock::advance(unsigned long)", "rt::Bus::read()",
                       "rt::TGP::run()", "rt::Geo::step()", "rt::Video::render()", "rt::Raster::poly()",
                       "app::Model2Frame::build()",
                       "snd::Cpu68k::reset()", "snd::Model2ASound::Model2ASound()", "snd::Model2Scsp::Impl::init()",
                       "gen::run(gen::Env&)", "sndgen::run(sndgen::Env&)", "tgpgen::run()"):
            with self.subTest(symbol=symbol), self.assertRaisesRegex(ValueError, "forbidden reference-machine symbols"):
                driver.validate_native_symbols("00101000 T " + symbol + "\n")

    def test_native_target_cannot_accept_a_reference_core_manifest(self):
        args = type("Args", (), {"target": "native-port", "core_manifest": ROOT / "build/reference.json"})()
        with self.assertRaisesRegex(ValueError, "core-manifest is forbidden"):
            driver.build_spec(args)
        args.core_manifest = None
        spec = driver.build_spec(args)
        self.assertEqual(spec["defines"], ["RALLY_NATIVE_PS2=1"])
        self.assertNotIn("src", spec["includes"])
        self.assertNotIn(".", spec["includes"])

    def test_native_boundary_rejects_every_reference_subsystem_and_generated_code(self):
        forbidden = ["src/runtime/cpu.cpp", "src/runtime/model2a_machine.cpp",
                     "src/runtime/model2a_bus.cpp", "src/runtime/model2a_clock.cpp",
                     "src/runtime/model2_scsp.cpp", "src/runtime/model2a_sound.cpp",
                     "src/runtime/snd_cpu.cpp", "src/runtime/tgp.h", "src/runtime/raster.h",
                     "src/app/gpu/model2_frame.h", "platform/ps2/game/rom_cache.h",
                     "build/ps2/native/gen/chunk_000.cpp", "build/ps2/native/tgp/program.cpp",
                     "build/ps2/native/sound/native-timed/snd_gen.cpp"]
        for name in forbidden:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "forbids"):
                driver.validate_native_boundary([ROOT / "platform/ps2/native/main.cpp", ROOT / name])

    def test_native_boundary_checks_transitive_headers_and_resolves_symlinks(self):
        with tempfile.TemporaryDirectory(prefix="ps2-native-boundary-") as temporary:
            root = Path(temporary).resolve()
            native = root / "platform/ps2/native"
            native.mkdir(parents=True)
            runtime = root / "src/runtime"
            runtime.mkdir(parents=True)
            forbidden = runtime / "model2a_machine.h"
            forbidden.write_text("struct ReferenceMachine {};\n")
            main = native / "main.cpp"
            bridge = native / "bridge.h"
            main.write_text('#include "bridge.h"\n')
            bridge.write_text('#include "../../../src/runtime/model2a_machine.h"\n')
            with mock.patch.object(driver, "ROOT", root):
                dependencies = driver.project_headers([main], [native])
                with self.assertRaisesRegex(ValueError, "model2a_machine.h"):
                    driver.validate_native_boundary([main, *dependencies])
                bridge.unlink()
                bridge.symlink_to(forbidden)
                with self.assertRaisesRegex(ValueError, "model2a_machine.h"):
                    driver.validate_native_boundary([main, bridge])
                with self.assertRaisesRegex(ValueError, "model2a_machine.h"):
                    driver.validate_native_boundary([main])
                main.write_text("int main() { return 0; }\n")
                allowed = driver.validate_native_boundary([main])
                self.assertFalse(allowed["translated_processors_linked"])
                self.assertFalse(allowed["arcade_machine_runtime_linked"])
                main.write_text('#define SOURCE "hidden.cpp"\n#include SOURCE\n')
                with self.assertRaisesRegex(ValueError, "opaque include"):
                    driver.validate_native_boundary([main])

    def test_sound_manifest_requires_measured_compact_configuration_and_dependencies(self):
        audit = {"runtime_flags": ["-DSEGARALLY_SOUND_TIMED_PROBE", "-DM2_COMPACT_SOUND=1"],
                 "sources_sha256": dict.fromkeys(prepare.SOUND_AUDIT_DEPENDENCIES, "verified elsewhere")}
        prepare.validate_sound_configuration(audit)
        for flags in ([], ["-DSEGARALLY_SOUND_TIMED_PROBE"], "-DM2_COMPACT_SOUND=1"):
            with self.subTest(flags=flags), self.assertRaisesRegex(ValueError, "compact-scsp"):
                prepare.validate_sound_configuration(dict(audit, runtime_flags=flags))
        for omitted in prepare.SOUND_AUDIT_DEPENDENCIES:
            sources = dict(audit["sources_sha256"])
            del sources[omitted]
            with self.subTest(omitted=omitted), self.assertRaisesRegex(ValueError, "dependencies"):
                prepare.validate_sound_configuration(dict(audit, sources_sha256=sources))

    def test_audio_fixture_uses_only_original_sources_and_open_iop_modules(self):
        spec = driver.build_spec(type("Args", (), {"target": "audio-fixture"})())
        self.assertEqual(spec["iop_modules"], ["libsd", "rally_pcm"])
        self.assertTrue(spec["pcm_transport"])
        self.assertNotIn("audsrv", spec["libraries"])
        self.assertTrue(all(name.startswith("platform/ps2/") for name in spec["sources"]))
        workflow = (ROOT / ".github/workflows/ps2.yml").read_text()
        self.assertIn("--target audio-fixture", workflow)

    def test_default_output_cannot_follow_symlink_outside_build(self):
        with tempfile.TemporaryDirectory(prefix="ps2-output-confinement-") as temporary:
            directory = Path(temporary).resolve()
            root = directory / "checkout"
            (root / "build").mkdir(parents=True)
            outside = directory / "outside"
            outside.mkdir()
            (root / "build/ps2-bringup").symlink_to(outside, target_is_directory=True)
            error = io.StringIO()
            with mock.patch.object(driver, "ROOT", root), contextlib.redirect_stderr(error):
                with self.assertRaises(SystemExit) as stopped:
                    driver.main(["report"])
            self.assertEqual(stopped.exception.code, 2)
            self.assertIn("ignored build directory", error.getvalue())
            self.assertEqual(list(outside.iterdir()), [])

    def test_ci_triggers_cover_bringup_sources_and_transitive_project_headers(self):
        sources = [ROOT / name for name in driver.BRINGUP_SOURCES + driver.NATIVE_SOURCES + driver.ENGINE_DEMO_SOURCES + list(driver.NATIVE_IOP_SOURCES)]
        dependencies = sources + driver.project_headers(sources, [ROOT / "platform/ps2/include", ROOT / "platform/ps2/native", ROOT / "src"])
        workflow = (ROOT / ".github/workflows/ps2.yml").read_text()
        for event in ("push", "pull_request"):
            # The workflow uses a quoted inline list, also a Python literal;
            # no YAML package is needed by the portable boundary tests.
            match = re.search(r"^  " + event + r":\n    paths: (\[.*\])$", workflow, re.MULTILINE)
            self.assertIsNotNone(match, f"missing inline path filters for {event}")
            patterns = ast.literal_eval(match.group(1))
            uncovered = [path.relative_to(ROOT).as_posix() for path in dependencies
                         if not any(fnmatch.fnmatchcase(path.relative_to(ROOT).as_posix(), pattern)
                                    for pattern in patterns)]
            self.assertEqual(uncovered, [], f"{event} does not trigger for PS2 dependencies")


class HeaderDependencyTests(unittest.TestCase):
    def test_tracks_transitive_local_and_angle_includes_without_scanning_worktrees(self):
        with tempfile.TemporaryDirectory(prefix="ps2-header-closure-") as directory:
            root = Path(directory)
            (root / "src").mkdir()
            (root / "build/old-worktree").mkdir(parents=True)
            main = root / "src/main.cpp"
            main.write_text('#include "a.h"\n#include <cstdint>\n#include <shared.h>\n')
            a = root / "src/a.h"
            a.write_text('#include "b.inc"\n#include "unavailable-platform.h"\n')
            b = root / "src/b.inc"
            b.write_text('#include "a.h"\n')
            shared = root / "shared.h"
            shared.write_text("#pragma once\n")
            (root / "build/old-worktree/unrelated.h").write_text("unrelated dirty source\n")
            self.assertEqual(set(driver.project_headers([main], [root])), {p.resolve() for p in (a, b, shared)})

    def test_frontend_header_change_preserves_unrelated_generated_source_cache_key(self):
        with tempfile.TemporaryDirectory(prefix="ps2-per-source-cache-") as directory:
            root = Path(directory).resolve()
            generated, frontend = root / "chunk.cpp", root / "main.cpp"
            cpu, storage = root / "cpu.h", root / "storage.h"
            generated.write_text('#include "cpu.h"\n')
            frontend.write_text('#include "storage.h"\n')
            cpu.write_text("struct Cpu {};\n")
            storage.write_text("void open_pack();\n")
            sources = [generated, frontend]
            dependencies = {source: driver.project_headers([source], [root]) for source in sources}
            paths = [generated, frontend, cpu, storage]

            def keys():
                hashes = {driver.relative(path): driver.digest(path) for path in paths}
                return [driver.compilation_key(source, dependencies[source], hashes, "same compiler and flags")
                        for source in sources]

            before = keys()
            storage.write_text("void open_pack_with_progress();\n")
            after_storage = keys()
            self.assertEqual(before[0], after_storage[0])
            self.assertNotEqual(before[1], after_storage[1])
            cpu.write_text("struct Cpu { int pc; };\n")
            after_cpu = keys()
            self.assertNotEqual(after_storage[0], after_cpu[0])
            self.assertEqual(after_storage[1], after_cpu[1])


if __name__ == "__main__":
    unittest.main()
