#!/usr/bin/env python3
"""Compare actual old/new Model2AMachine scheduling with compatible native code.

Requires separately regenerated audited hook chunks. This compares shared board
implementations and generated CPU code, not independent MAME/PCB behavior.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_recompile import verify_boundary_contract, boundary_compile_flags


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_paths(root, build, audit, names):
    result = []
    for name in names:
        entry = audit["images"][name]
        path = (root / entry["path"]).resolve()
        if not path.is_relative_to(build) or sha(path) != entry["sha256"]:
            raise ValueError(f"{name} differs from the verified path-confined import")
        result.append(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-build", type=Path, required=True,
                        help="Compatible boundary-ABI generation beneath this worktree's build/")
    parser.add_argument("--assets-build", type=Path, required=True,
                        help="Frozen verified original v13 build; never modified")
    parser.add_argument("--asset-root", type=Path, default=ROOT,
                        help="Checkout root used by frozen image audit paths")
    parser.add_argument("--output", type=Path, default=ROOT / "build/validation/boundary-game")
    parser.add_argument("--instructions", type=int, default=2_000_000)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--input-replay", type=Path)
    parser.add_argument("--mixed-resume", action="store_true",
                        help="Include occasional run_for(1) among run_until_frame resumes")
    parser.add_argument("--compile-only", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.instructions <= 2_000_000_000 or not 1 <= args.timeout <= 3600:
        parser.error("instruction budget must be1..2000000000 and timeout1..3600")
    native, assets, asset_root, output = (p.resolve() for p in
        (args.native_build, args.assets_build, args.asset_root, args.output))
    if not output.is_relative_to(ROOT / "build") or output == ROOT / "build":
        parser.error("output must be beneath this worktree's ignored build/")
    if not assets.is_relative_to(asset_root / "build"):
        parser.error("assets build must be beneath its checkout's ignored build/")
    coverage = json.loads((native / "coverage.json").read_text())
    contract = verify_boundary_contract(native, coverage)
    if contract is None:
        raise ValueError("compatible regenerated boundary ABI required; ordinary objects are rejected")
    original = json.loads((assets / "coverage.json").read_text())
    if coverage["romset"] != original["romset"] or original["romset"] != "srallycb" or original["board"] != "model2a":
        raise ValueError("matching Sega Rally Revision B Model2A audits required")
    original_reach = json.loads((assets / "reachability.json").read_text())
    if (coverage["reachability"]["instruction_addresses"] != original_reach["instruction_addresses"] or
        coverage["reachability"]["instruction_count"] != original_reach["instruction_count"]):
        raise ValueError("hook generation changed the frozen native instruction coverage")
    images = image_paths(asset_root, assets, original, ("program", "main_data"))
    for name in ("program", "main_data"):
        if coverage["images"][name]["sha256"] != original["images"][name]["sha256"]:
            raise ValueError("hook code was generated from different ROM images")
    tgp = json.loads((assets / "tgp/audit.json").read_text())
    if tgp["romset"] != original["romset"] or not tgp["native_code_compiled"] or tgp["exit_code"] != 0:
        raise ValueError("matching compiled native TGP audit required")
    images += image_paths(asset_root, assets, tgp, ("copro_tgp_tables", "copro_data"))
    video = json.loads((assets / "video/audit.json").read_text())
    if video["romset"] != original["romset"] or video["board"] != "model2a" or video["manifest_sha256"] != sha(ROOT / "config/segarally_video.json"):
        raise ValueError("matching verified video manifest required")
    images += image_paths(asset_root, assets, video, ("polygons", "textures"))
    tgp_generated = (assets / "tgp/program.cpp").resolve()
    if not tgp_generated.is_relative_to(assets) or not tgp_generated.is_file():
        raise ValueError("TGP generated source escaped verified asset build")
    output.mkdir(parents=True, exist_ok=True)
    replay = []
    replay_metadata = None
    if args.input_replay:
        data = args.input_replay.read_bytes()
        if len(data) > 1024 * 1024:
            raise ValueError("input replay exceeds1MiB")
        path = output / "boundary-input.inputs"
        path.write_bytes(data)
        replay = [str(path)]
        replay_metadata = {"source": str(args.input_replay.resolve()), "sha256": sha(path)}
    compiler = shlex.split(args.compiler)
    sources = ["tests/test_segarally_boundary_game.cpp", "src/runtime/model2a_machine.cpp",
        "src/runtime/model2a_bus.cpp", "src/runtime/cpu.cpp", "src/runtime/lockstep.cpp",
        "src/runtime/model2a_clock.cpp", "src/runtime/model2a_control.cpp", "src/runtime/model2_io.cpp",
        "src/runtime/model2a_io.cpp", "src/runtime/model2_eeprom.cpp", "src/runtime/model2a_comm.cpp",
        "src/runtime/comm_board.cpp", "src/runtime/model2_uart.cpp", "src/runtime/model2a_uart.cpp",
        "src/runtime/tgp.cpp", "src/runtime/m2_tgp_board.cpp", "src/runtime/model2a_video.cpp",
        "src/runtime/video.cpp", "src/runtime/raster.cpp", "src/runtime/geo.cpp", str(tgp_generated)]
    binary = output / "boundary_game"
    command = compiler + ["-std=c++20", "-O2", "-UNDEBUG", "-fno-fast-math", "-ffp-contract=off", "-Isrc", "-I."]
    command += boundary_compile_flags(compiler)
    command += ["-DM2_SRALLY_" + name for name in ("TGP", "IO", "COMM", "CONTROL", "VIDEO", "SCREEN", "UART")]
    objects = sorted((native / "objects").glob("*.o"))
    command += sources + [str(p) for p in objects] + ["-o", str(binary)]
    metadata = {"schema": 1, "scope": "actual old/new Model2AMachine; generated CPU/devices shared, no independent hardware claim",
        "private_device_queues_compared": False, "native_build": str(native), "assets_build": str(assets),
        "coverage_sha256": sha(native / "coverage.json"), "boundary_contract": contract,
        "asset_coverage_sha256": sha(assets / "coverage.json"), "tgp_generated_sha256": sha(tgp_generated),
        "images_sha256": {p.name: sha(p) for p in images}, "input_replay": replay_metadata,
        "compiled_sources_sha256": {name: sha(ROOT / name) for name in sources if not Path(name).is_absolute()},
        "instructions": args.instructions, "mixed_resume": args.mixed_resume, "compile_command": command}
    with (output / "build.log").open("w") as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    metadata["binary_sha256"] = sha(binary)
    if not args.compile_only:
        run = [str(binary), *map(str, images), str(args.instructions), *replay]
        if args.mixed_resume:
            run.append("--mixed-resume")
        metadata["run_command"] = run
        result = subprocess.run(run, cwd=ROOT, text=True, capture_output=True, timeout=args.timeout)
        (output / "result.txt").write_text(result.stdout + result.stderr)
        metadata["exit_code"] = result.returncode
        print(result.stdout + result.stderr, end="")
    (output / "audit.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if not args.compile_only:
        result.check_returncode()


if __name__ == "__main__":
    main()
