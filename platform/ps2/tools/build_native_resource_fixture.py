#!/usr/bin/env python3
"""Build the original GS resource-lifetime fixture with SDK link wrappers.

The fixed GNU ld wrappers inject setup failures and count allocations in this
fixture only. Production Display and production builds have no test hooks.
No emulator is launched and no assets are imported or packaged.
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
PS2 = ROOT / "platform/ps2"
BUILD_HELPER = PS2 / "build_ps2.py"
PATH_HELPER = PS2 / "tools/build_native_depth_fixture.py"
LOCK = PS2 / "toolchain.lock.json"
WRAPPED_SYMBOLS = ("packet_init", "packet_free", "memalign", "free",
                   "dma_channel_initialize", "graph_vram_allocate",
                   "graph_vram_clear", "graph_initialize")


def module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def fixture_spec() -> dict:
    return {"sources": ["platform/ps2/" + path for path in (
                "tests/native_resources_fixture.cpp", "src/native_display.cpp",
                "src/native_frame.cpp", "src/vu0_pack.cpp")],
            "includes": ["platform/ps2/include", "platform/ps2/tests"],
            "libraries": ["draw", "graph", "dma", "packet", "m"],
            "defines": [], "compile_flags": [], "embed_iop_modules": False}


def fixture_command(command: list[str], label: str) -> list[str]:
    # The original SDK implementations remain available through __real_*.
    # No flag is added to compilation, toolchain inspection or production links.
    return [*command, *("-Wl,--wrap=" + symbol for symbol in WRAPPED_SYMBOLS)] if label == "link" else command


def build(output: Path, container_platform: str | None = None) -> dict:
    path_helper = module(PATH_HELPER, "resource_fixture_paths")
    helpers = module(BUILD_HELPER, "resource_fixture_build_helpers")
    output = path_helper.resolve_output(output)
    manifest = fixture_spec()
    sources = [ROOT / name for name in manifest["sources"]]
    headers = helpers.project_headers(sources, [ROOT / name for name in manifest["includes"]])
    for header in headers:
        if not header.is_relative_to(PS2 / "include") and not header.is_relative_to(PS2 / "tests"):
            raise ValueError("Non-native fixture dependency: " + str(header.relative_to(ROOT)))
    inputs = sorted(set(sources + headers + [Path(__file__).resolve(), BUILD_HELPER, PATH_HELPER, LOCK]))
    initial = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    output = path_helper.resolve_output(output)
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "manifest.json"
    with manifest_path.open("x") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")

    class FixtureRunner(helpers.Runner):
        def run(self, command, label, container=True):
            return super().run(fixture_command(command, label), label, container=container)

    helpers.Runner = FixtureRunner
    report = {"schema": 1, "status": "build_failed", "mode": "resource_fixture",
              "scope": "original GS cache invalidation, ownership and controlled setup failures",
              "build_inputs_sha256": initial, "wrapped_symbols": list(WRAPPED_SYMBOLS),
              "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "roms_packaged": False, "playable": False, "ps2_hardware_tested": False,
              "limitations": ["Cross-compilation only; execution and pixel checks not established",
                              "Failure injection is confined to this fixture executable",
                              "Fatal readback quarantine is not deliberately triggered"]}
    args = SimpleNamespace(output=output, target="core-probe", core_manifest=manifest_path,
                           backend="docker", container_platform=container_platform, jobs=2, optimization="Os")
    try:
        helpers.build(args, report)
        helpers.verify_source_snapshot(initial)
        if hashlib.sha256(manifest_path.read_bytes()).hexdigest() != report["manifest_sha256"]:
            raise RuntimeError("Fixture manifest changed while compiling")
        report["status"] = "cross_compiled_runtime_unverified"
    except Exception:
        report["status"] = "build_failed"
        raise
    finally:
        with (output / "build-report.json").open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/ps2-native-resource-fixture"),
                        help="New or empty output directory beneath build/")
    parser.add_argument("--container-platform", choices=("linux/arm64", "linux/amd64"))
    args = parser.parse_args(argv)
    try:
        report = build(args.output, args.container_platform)
    except (OSError, ValueError, RuntimeError) as error:
        print("Native resource fixture build failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(report["elf"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
