#!/usr/bin/env python3
"""Validate every native boot instruction against the test-only reference CPU.

Requires an audited generation from scripts/segarally_recompile.py. This checks
CPU state and ordered writes through the same bus, not independent PCB behavior.
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
    parser.add_argument("--io", action="store_true", help="Attach independent I/O+EEPROM devices with explicit frozen clocks")
    args = parser.parse_args()
    directory = args.build_dir.resolve()
    if not directory.is_relative_to(ROOT / "build") or directory == ROOT / "build":
        parser.error("--build-dir must be below ignored build/")
    if not 1 <= args.instructions <= 100_000_000:
        parser.error("--instructions must be 1..100000000")
    audit = json.loads((directory / "coverage.json").read_text())
    assert audit["board"] == "model2a" and audit["native_objects_compiled"]
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
        "src/runtime/lockstep.cpp", "src/refcore/i960_ref.cpp"]
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
    subprocess.run(command + list(map(str, objects)) + ["-o", str(executable)], cwd=ROOT, check=True)
    arguments = [str(executable), *map(str, images), str(args.instructions), *map(str, tgp_images)]
    if args.io:
        arguments.append("--io")
    result = subprocess.run(arguments,
                            cwd=ROOT, capture_output=True, text=True, timeout=60)
    transcript = result.stdout + result.stderr
    (directory / "boot-parity.txt").write_text(transcript)
    print(transcript, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
