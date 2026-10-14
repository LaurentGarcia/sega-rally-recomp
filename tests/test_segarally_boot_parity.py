#!/usr/bin/env python3
"""Validate every native boot instruction against the test-only reference CPU.

Requires an audited generation from scripts/segarally_recompile.py. This checks
CPU state, reference cycles and cycle-stamped ordered writes through the same
bus implementation, not independent MAME scheduler or PCB behavior.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    parser.add_argument("--instructions", type=int, default=2_000_000)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--tgp", action="store_true", help="Attach independently instantiated native TGP boards")
    parser.add_argument("--io", action="store_true", help="Attach independent I/O+EEPROM devices using each CPU's reference cycle clock")
    parser.add_argument("--comm", action="store_true", help="Attach independent disconnected communication adapters (no firmware or vblank)")
    parser.add_argument("--control", action="store_true", help="Attach independent IRQ/countdown timers with explicit boundary dispatch")
    parser.add_argument("--video", action="store_true", help="Attach independent video RAM and actual Geo/TGP bindings (frame clock frozen)")
    parser.add_argument("--screen", action="store_true", help="Connect shared queue screen phase, geometry, rendering and vblank IRQ")
    args = parser.parse_args()
    if args.video and not args.tgp:
        parser.error("--video requires --tgp for each independent geometry buffer")
    if args.screen and not (args.control and args.video):
        parser.error("--screen requires --control and --video")
    directory = args.build_dir.resolve()
    if not directory.is_relative_to(ROOT / "build") or directory == ROOT / "build":
        parser.error("--build-dir must be below ignored build/")
    if not 1 <= args.instructions <= 100_000_000:
        parser.error("--instructions must be 1..100000000")
    audit = json.loads((directory / "coverage.json").read_text())
    assert audit["board"] == "model2a" and audit["native_objects_compiled"]
    reachability = json.loads((directory / "reachability.json").read_text())
    if not reachability.get("reference_cycle_accounting") or reachability.get("reference_cycle_accounting_version") != 1:
        parser.error("regenerate native objects with reference cycle accounting before timed parity")
    objects = sorted((directory / "objects").glob("*.o"))
    assert len(objects) == audit["native_object_count"]
    images = []
    for name in ("program", "main_data"):
        entry = audit["images"][name]
        path = (ROOT / entry["path"]).resolve()
        assert path.is_relative_to(directory)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
        images.append(path)
    executable = directory / "boot_parity"
    command = shlex.split(args.compiler) + ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc",
        "tests/test_segarally_boot_parity.cpp", "src/runtime/model2a_bus.cpp", "src/runtime/cpu.cpp",
        "src/runtime/lockstep.cpp", "src/refcore/i960_ref.cpp", "src/runtime/model2a_clock.cpp"]
    tgp_images = []
    if args.tgp:
        tgp_audit = json.loads((directory / "tgp/audit.json").read_text())
        assert tgp_audit["native_code_compiled"] and tgp_audit["exit_code"] == 0
        for name in ("copro_tgp_tables", "copro_data"):
            entry = tgp_audit["images"][name]
            path = (ROOT / entry["path"]).resolve()
            assert path.is_relative_to(directory)
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
            tgp_images.append(path)
        command += ["-DM2_SRALLY_TGP", "src/runtime/tgp.cpp", "src/runtime/m2_tgp_board.cpp",
                    str(directory / "tgp/program.cpp")]
    if args.io:
        command += ["-DM2_SRALLY_IO", "src/runtime/model2_io.cpp", "src/runtime/model2a_io.cpp",
                    "src/runtime/model2_eeprom.cpp"]
    if args.comm:
        command += ["-DM2_SRALLY_COMM", "src/runtime/model2a_comm.cpp", "src/runtime/comm_board.cpp"]
    if args.control:
        command += ["-DM2_SRALLY_CONTROL", "src/runtime/model2a_control.cpp"]
    video_images = []
    if args.video:
        video_audit = json.loads((directory / "video/audit.json").read_text())
        assert video_audit["romset"] == "srallycb" and video_audit["board"] == "model2a"
        assert video_audit["manifest_sha256"] == hashlib.sha256((ROOT / "config/segarally_video.json").read_bytes()).hexdigest()
        for name in ("polygons", "textures"):
            entry = video_audit["images"][name]
            path = (ROOT / entry["path"]).resolve()
            assert path.is_relative_to(directory)
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
            video_images.append(path)
        command += ["-DM2_SRALLY_VIDEO", "src/runtime/model2a_video.cpp", "src/runtime/geo.cpp",
                    "src/runtime/video.cpp", "src/runtime/raster.cpp"]
    subprocess.run(command + list(map(str, objects)) + ["-o", str(executable)], cwd=ROOT, check=True)
    arguments = [str(executable), *map(str, images), str(args.instructions), *map(str, tgp_images), *map(str, video_images)]
    if args.io:
        arguments.append("--io")
    if args.comm:
        arguments.append("--comm")
    if args.control:
        arguments.append("--control")
    if args.video:
        arguments.append("--video")
    if args.screen:
        arguments.append("--screen")
    result = subprocess.run(arguments,
                            cwd=ROOT, capture_output=True, text=True, timeout=60)
    transcript = result.stdout + result.stderr
    (directory / "boot-parity.txt").write_text(transcript)
    print(transcript, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
