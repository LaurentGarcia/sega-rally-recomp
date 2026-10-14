#!/usr/bin/env python3
"""Generate synthetic instructions and verify their native/reference timestamps."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import segarally_recompile as pipeline


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    compiler = shlex.split(args.compiler)
    output = ROOT / "build/validation/runtime-timing"
    output.mkdir(parents=True, exist_ok=True)
    tool = pipeline.build_generator(compiler, output / "tools")
    program = bytearray(0x10000)
    struct.pack_into("<I", program, 12, 0x40)
    # Self-authored absolute MEMB fixtures, then call/return/IAC services.
    for pc, word, displacement in ((0x40, 0x80803000, 0x5000), (0x50, 0x82803000, 0x5001),
                                  (0x60, 0x8c803000, 0x123), (0x70, 0xb0803000, 0x5000),
                                  (0x80, 0xb2803000, 0x5000)):
        struct.pack_into("<II", program, pc, word, displacement)
    struct.pack_into("<I", program, 0x100, 0x09000100) # call 0x200
    struct.pack_into("<I", program, 0x200, 0x0a000000) # ret
    synmovq = (0x60 << 24) | (2 << 7) | 16 | (18 << 14)
    struct.pack_into("<I", program, 0x300, synmovq)
    image = output / "synthetic.bin"
    image.write_bytes(program)
    seeds = output / "seeds.txt"
    seeds.write_text("40\n50\n60\n70\n80\n100\n200\n300\n")
    generated = output / "gen"
    generated.mkdir(exist_ok=True)
    for stale in generated.glob("*.cpp"):
        stale.unlink()
    subprocess.run([str(tool), str(image), str(generated), "--board", "model2a", "--seeds", str(seeds)],
                   cwd=ROOT, check=True)
    executable = output / "runtime_timing"
    subprocess.run(compiler + ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc",
        "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp", "tests/test_i960_runtime_timing.cpp"] +
        [str(source) for source in sorted(generated.glob("*.cpp"))] + ["-o", str(executable)],
        cwd=ROOT, check=True)
    subprocess.run([str(executable), str(image)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
