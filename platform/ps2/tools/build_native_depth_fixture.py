#!/usr/bin/env python3
"""Cross-build original native GS depth or private scene-readback fixtures.

Uses the existing immutable PS2dev toolchain and ELF inspection. No emulator
is launched and no game assets are packaged. Compilation does not establish
depth correctness, framebuffer contents or physical-console execution.
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
LOCK = PS2 / "toolchain.lock.json"
COMMON_SOURCES = ("src/native_display.cpp", "src/native_frame.cpp", "src/vu0_pack.cpp")
SCENE_SOURCES = ("src/native_mesh.cpp", "src/native_scene.cpp", "src/native_geometry.cpp")


def resolve_output(value: Path, root: Path = ROOT) -> Path:
    root = root.resolve()
    build = root / "build"
    if build.is_symlink():
        raise ValueError("The repository build directory must not be a symlink")
    output = (value if value.is_absolute() else root / value).resolve()
    if output == build or not output.is_relative_to(build):
        raise ValueError("Output must be a directory beneath build/, not its root")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError("Output must be empty; preserve earlier evidence in a different directory")
    return output


def fixture_spec(scene_capture: bool) -> dict:
    entry = "tests/native_scene_fixture.cpp" if scene_capture else "tests/native_depth_fixture.cpp"
    sources = (entry, *COMMON_SOURCES, *(SCENE_SOURCES if scene_capture else ()))
    return {"sources": ["platform/ps2/" + name for name in sources],
            "includes": ["platform/ps2/include", "platform/ps2/tests"],
            "libraries": ["draw", "graph", "dma", "packet", "m"],
            "defines": [], "compile_flags": [], "embed_iop_modules": False}


def build(output: Path, scene_capture: bool = False, container_platform: str | None = None) -> dict:
    output = resolve_output(output)
    spec = importlib.util.spec_from_file_location("rally_native_fixture_build_helpers", BUILD_HELPER)
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    manifest = fixture_spec(scene_capture)
    sources = [ROOT / name for name in manifest["sources"]]
    headers = helpers.project_headers(sources, [ROOT / name for name in manifest["includes"]])
    # A future include must not pull a board/runtime dependency into a fixture.
    for header in headers:
        if not header.is_relative_to(PS2 / "include") and not header.is_relative_to(PS2 / "tests"):
            raise ValueError("Non-native fixture header dependency: " + str(header.relative_to(ROOT)))
    inputs = sorted(set(sources + headers + [Path(__file__).resolve(), BUILD_HELPER, LOCK]))
    initial = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    # Recheck confinement immediately before creating the manifest or Docker.
    output = resolve_output(output)
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "manifest.json"
    with manifest_path.open("x") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    report = {"schema": 1, "status": "build_failed",
              "mode": "scene_capture" if scene_capture else "depth_fixture",
              "scope": "original native GS scene readback fixture" if scene_capture else "original ROM-free GS depth fixture",
              "build_inputs_sha256": initial,
              "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "roms_packaged": False, "playable": False, "ps2_hardware_tested": False,
              "limitations": ["Cross-compilation only; no emulator or console execution",
                              "No captured framebuffer or depth correctness claim",
                              "Scene mode requires separately supplied private mesh data"]}
    args = SimpleNamespace(output=output, target="core-probe", core_manifest=manifest_path,
                           backend="docker", container_platform=container_platform, jobs=2,
                           optimization="Os")
    try:
        helpers.build(args, report)
        helpers.verify_source_snapshot(initial)
        if hashlib.sha256(manifest_path.read_bytes()).hexdigest() != report["manifest_sha256"]:
            raise RuntimeError("Fixture manifest changed while compiling")
        report["artifacts_sha256"] = {
            str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted((output / "obj").glob("*.o"))}
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
    parser.add_argument("--output", type=Path,
                        help="New or empty directory beneath build/; earlier evidence is never overwritten")
    parser.add_argument("--scene-capture", action="store_true",
                        help="Build the static owner-supplied mesh framebuffer capture fixture")
    parser.add_argument("--container-platform", choices=("linux/arm64", "linux/amd64"))
    args = parser.parse_args(argv)
    output = args.output or Path("build/ps2-native-scene-fixture" if args.scene_capture else "build/ps2-native-depth-fixture")
    try:
        report = build(output, args.scene_capture, args.container_platform)
    except (OSError, ValueError, RuntimeError) as error:
        print("Native GS fixture build failed: " + str(error), file=sys.stderr)
        return 1
    print(json.dumps(report["elf"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
