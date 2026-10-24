#!/usr/bin/env python3
"""Build the original ROM-free VU1 numerical fixture with the pinned toolchain.

This independent experiment does not enable VU1 in the native game renderer.
The fixed source inventory contains no game data, transformed game programs,
board devices, XGKICK or drawing. Compilation is not runtime validation.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
FIXTURE = HERE.parent / "tests/vu1"
BUILD_HELPER = HERE.parent / "build_ps2.py"
LOCK = HERE.parent / "toolchain.lock.json"
SOURCES = (FIXTURE / "fixture.cpp", FIXTURE / "transform.vsm", HERE.parent / "src/vu0_pack.cpp")
INPUTS = (*SOURCES, HERE.parent / "include/vu0_pack.h", Path(__file__).resolve(), BUILD_HELPER, LOCK)


def resolve_output(value: Path, root: Path = ROOT) -> Path:
    """Only empty output directories beneath the real repository build tree."""
    root = root.resolve()
    build = root / "build"
    if build.is_symlink():
        raise ValueError("The repository build directory must not be a symlink")
    candidate = value if value.is_absolute() else root / value
    output = candidate.resolve()
    if output == build or not output.is_relative_to(build):
        raise ValueError("Output must be a directory beneath build/, not its root")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError("Output must be empty; preserve previous evidence and choose a new directory")
    return output


def fingerprints() -> dict[str, str]:
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in INPUTS}


def load_build_helpers():
    spec = importlib.util.spec_from_file_location("rally_vu1_build_helpers", BUILD_HELPER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(output: Path, dual_unit: bool = False, trailing_nop_qword: bool = False) -> dict:
    # Revalidate immediately before creating anything or starting Docker.
    output = resolve_output(output)
    initial = fingerprints()
    helpers = load_build_helpers()
    runner = helpers.Runner(SimpleNamespace(
        output=output, backend="docker", container_platform=None))
    report = {"schema": 1, "scope": "original ROM-free VIF1/VU1 transform/readback fixture",
              "status": "build_failed", "source_sha256": initial,
              "mode": "dual_unit_diagnostic" if dual_unit else "baseline_numerical",
              "compiler_defines": ["_EE"] + (["RALLY_DUAL_VU_TEST=1"] if dual_unit else [])
                                  + (["RALLY_VIF_TRAILING_NOP=1"] if trailing_nop_qword else []),
              "dual_unit_enabled": dual_unit,
              "trailing_nop_qword": trailing_nop_qword,
              "dual_unit_scope": "Optional VU0 final XYZ conversion before the outstanding VIF1/VU1 completion fence",
              "limitations": ["No game or drawing", "No XGKICK", "No physical console validation",
                              "Cross-compilation does not prove execution or numerical results"]}
    try:
        report["toolchain"] = runner.identity()
        report["assembler_version"] = runner.run(
            [runner.ps2dev + "/dvp/bin/dvp-as", "--version"], "assembler-version").strip()
        runner.run([runner.ps2dev + "/dvp/bin/dvp-as", "-al=/out/transform.lst",
                    runner.path(SOURCES[1]), "-o", "/out/transform.o"], "assemble")
        compile_flags = [runner.tool("g++"), "-std=c++20", "-D_EE", "-G0", "-O2", "-Wall", "-Wextra",
                    "-fno-fast-math", "-ffp-contract=off", "-I" + runner.sdk + "/ee/include",
                    "-I" + runner.sdk + "/common/include", "-I" + runner.path(HERE.parent / "include")]
        if dual_unit:
            compile_flags.append("-DRALLY_DUAL_VU_TEST=1")
        if trailing_nop_qword:
            compile_flags.append("-DRALLY_VIF_TRAILING_NOP=1")
        runner.run([*compile_flags, "-c", runner.path(SOURCES[0]),
                    "-o", "/out/fixture.o"], "compile")
        runner.run([*compile_flags, "-c", runner.path(SOURCES[2]),
                    "-o", "/out/vu0_pack.o"], "compile-vu0")
        runner.run([runner.tool("g++"), "-G0", "-O2", "-T" + runner.sdk + "/ee/startup/linkfile",
                    "-L" + runner.sdk + "/ee/lib", "-Wl,-zmax-page-size=128", "-Wl,-Map,/out/fixture.map",
                    "/out/fixture.o", "/out/transform.o", "/out/vu0_pack.o", "-ldma", "-o", "/out/vu1-transform.elf"], "link")
        # This DVP binutils build emits generic ELF objects which its -d mode
        # cannot disassemble. Preserve the assembler's instruction/byte listing.
        runner.run([runner.ps2dev + "/dvp/bin/dvp-objdump", "-s", "/out/transform.o"], "micro-object-bytes")
        if fingerprints() != initial:
            raise RuntimeError("Build inputs changed while compiling; artifact evidence is invalid")
        report["elf"] = helpers.inspect_elf(output / "vu1-transform.elf")
        report["artifacts_sha256"] = {
            name: hashlib.sha256((output / name).read_bytes()).hexdigest()
            for name in ("fixture.o", "transform.o", "transform.lst", "vu0_pack.o")}
        report["status"] = "cross_compiled_runtime_unverified"
    finally:
        report["commands"] = runner.commands
        (output / "build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/ps2-vu1-fixture"),
                        help="New or empty directory beneath build/; previous evidence is never overwritten")
    parser.add_argument("--dual-unit", action="store_true",
                        help="Enable the separate, artificial-load VU0/VU1 cooperation diagnostic")
    parser.add_argument("--trailing-nop-qword", action="store_true",
                        help="Append one diagnostic VIF NOP qword after every unchanged fence/MARK")
    args = parser.parse_args(argv)
    try:
        report = build(resolve_output(args.output), args.dual_unit, args.trailing_nop_qword)
    except (OSError, ValueError, RuntimeError) as error:
        print("VU1 fixture build failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(report["elf"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
