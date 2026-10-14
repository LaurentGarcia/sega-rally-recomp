#!/usr/bin/env python3
"""Compile/run synthetic Machine old/new differential only after live-play release."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = ROOT / "build/validation/model2a-boundary-hook"
    out.mkdir(parents=True, exist_ok=True)
    sources = ["tests/test_model2a_boundary_hook.cpp", "src/runtime/model2a_machine.cpp",
        "src/runtime/model2a_bus.cpp", "src/runtime/model2a_clock.cpp", "src/runtime/model2a_control.cpp",
        "src/runtime/cpu.cpp", "src/runtime/lockstep.cpp", "src/runtime/tgp.cpp",
        "src/runtime/m2_tgp_board.cpp", "src/runtime/model2a_video.cpp", "src/runtime/video.cpp",
        "src/runtime/raster.cpp", "src/runtime/geo.cpp"]
    binary = out / ("differential-sanitized" if args.sanitize else "differential")
    command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20", "-O1", "-UNDEBUG",
        "-fno-fast-math", "-ffp-contract=off", "-Isrc", "-DM2_INSTRUCTION_BOUNDARY_HOOK",
        "-DM2_SRALLY_CONTROL", "-DM2_SRALLY_SCREEN", "-DM2_SRALLY_VIDEO", "-DM2_SRALLY_TGP"]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
    subprocess.run(command + sources + ["-o", str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True, timeout=30)

if __name__ == "__main__":
    main()
