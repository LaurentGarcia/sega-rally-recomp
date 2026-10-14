#!/usr/bin/env python3
"""Compile Rally's actual sound reset path with the shared Daytona generator.

Requires segarally_sound.py's verified import. Stops at the first unsupported
SCSP access; reaching that frontier does not establish sound-chip execution.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

from segarally_recompile import ROOT, run_logged
from segarally_sound import build_path


def verified_images(build: Path, specification: dict) -> dict[str, Path]:
    imported = json.loads((build / "sound/audit.json").read_text())
    source = json.loads((ROOT / "config/segarally_sound.json").read_text())["source"]
    if (imported["romset"] != specification["romset"] or imported["board"] != "model2a"
            or imported["sound_board"] != "scsp" or imported["source"] != source):
        raise ValueError("sound generation requires the pinned verified srallycb SCSP import")
    result = {}
    for name, expected_hash in specification["images"].items():
        metadata = imported["images"][name]
        path = (ROOT / metadata["path"]).resolve()
        if not path.is_relative_to(build):
            raise ValueError("sound image is outside the audited build directory")
        content = path.read_bytes()
        actual_hash = hashlib.sha256(content).hexdigest()
        if len(content) != metadata["size"] or actual_hash != metadata["sha256"] or actual_hash != expected_hash:
            raise ValueError(f"sound image differs from verified revision: {name}")
        result[name] = path
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--max-instructions", type=int, default=1_000_000)
    args = parser.parse_args()
    try:
        if args.max_instructions <= 0:
            raise ValueError("instruction limit must be positive")
        build = build_path(args.build_dir)
        spec = json.loads((ROOT / "config/segarally_sound_boot.json").read_text())
        images = verified_images(build, spec)
        output = build_path(build / "sound/native")
        if not output.is_relative_to(build):
            raise ValueError("native sound output is outside the audited build directory")
        output.mkdir(parents=True, exist_ok=True)
        compiler = shlex.split(args.compiler)
        flags = ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc"]
        generator_sources = ["tools/m2sndrecomp/main.cpp", "src/m68k/decode.cpp", "src/m68k/reach.cpp"]
        runtime_sources = ["tools/srally_sound_boot/main.cpp", "src/runtime/snd_cpu.cpp", "src/runtime/model2a_sound_bus.cpp"]
        tool = output / "m2sndrecomp"
        generated = output / "snd_gen.cpp"
        run_logged(compiler + flags + generator_sources + ["-o", str(tool)], output / "generator-build.log")
        run_logged([str(tool), str(images["audiocpu"]), str(generated), "--board", "model2a",
                    "--audit", str(output / "generation.json")], output / "generation.log")
        executable = output / "srally_sound_boot"
        run_logged(compiler + flags + runtime_sources + [str(generated), "-o", str(executable)], output / "build.log")
        execution = subprocess.run([str(executable), str(images["audiocpu"]), str(images["samples"]),
                                    str(args.max_instructions)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        (output / "boot.txt").write_text(execution.stdout + execution.stderr)
        result = json.loads(execution.stdout) if execution.stdout.strip() else {"status": "error"}
        expected = spec["expected_frontier"]
        reached = (execution.returncode == 3 and result.get("status") == "frontier"
                   and all(result.get(key) == int(expected[key], 16) for key in ("pc", "address", "data", "mask"))
                   and result.get("region") == expected["region"] and result.get("write") == expected["write"]
                   and result.get("reset_pc") == int(spec["expected_reset"]["pc"], 16)
                   and result.get("reset_ssp") == int(spec["expected_reset"]["ssp"], 16)
                   and result.get("ram_nonzero_bytes") == 0)
        sources = generator_sources + runtime_sources + ["src/m68k/decode.h", "src/m68k/reach.h",
            "src/runtime/snd_cpu.h", "src/runtime/snd_gen_support.h", "src/runtime/snd_sched.h",
            "src/runtime/model2a_sound_bus.h", "config/segarally_sound_boot.json",
            "config/segarally_sound.json", "scripts/segarally_sound_recompile.py"]
        report = {"schema": 1, "romset": spec["romset"], "board": spec["board"],
                  "images_sha256": spec["images"], "compiler": compiler, "flags": flags,
                  "sources_sha256": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sources},
                  "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
                  "generation": json.loads((output / "generation.json").read_text()),
                  "execution": result, "exit_code": execution.returncode,
                  "expected_scsp_frontier_reached": reached, "sound_chip_implemented": False,
                  "timing_scope": spec["timing_scope"], "full_sound_parity_verified": False}
        (output / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
        print((output / "generation.log").read_text(), end="")
        print(execution.stdout + execution.stderr, end="")
        print(f"Native sound audit: {output / 'audit.json'}")
        return 0 if reached else 1
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as failure:
        print(f"segarally_sound_recompile: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
