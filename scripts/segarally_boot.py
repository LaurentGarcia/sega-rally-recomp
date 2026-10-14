#!/usr/bin/env python3
"""Link the generated Sega Rally core and report its bounded native boot frontier.

Run segarally_recompile.py first. Exit 3 means an unimplemented board access;
exit 4 means an unresolved code target. Neither is a successful game boot.
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

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    parser.add_argument("--instructions", type=int, default=1_000_000)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    output = args.build_dir.resolve()
    if not output.is_relative_to(ROOT / "build"):
        parser.error("--build-dir must be inside this project's ignored build directory")
    if not 1 <= args.instructions <= 100_000_000:
        parser.error("--instructions must be 1..100000000")
    try:
        audit = json.loads((output / "coverage.json").read_text())
        if audit.get("board") != "model2a" or not audit.get("native_objects_compiled"):
            raise ValueError("a successful Model2A generation/compile audit is required")
        objects = sorted((output / "objects").glob("*.o"))
        if len(objects) != audit["native_object_count"] or not (output / "gen/gen_table.cpp").exists():
            raise ValueError("generated object set does not match the compile audit")
        images = []
        for name in ("program", "main_data"):
            item = audit["images"][name]
            path = (ROOT / item["path"]).resolve()
            if not path.is_relative_to(output):
                raise ValueError("ROM image is outside the audited build directory")
            if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
                raise ValueError(f"{name} image differs from the verified import")
            images.append(path)
        compiler = shlex.split(args.compiler)
        executable = output / "srally_boot"
        command = compiler + ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc",
            "tools/srally_boot/main.cpp", "src/runtime/model2a_bus.cpp", "src/runtime/cpu.cpp",
            "src/runtime/lockstep.cpp"] + [str(p) for p in objects] + ["-o", str(executable)]
        with (output / "boot-build.log").open("w") as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = subprocess.run([str(executable), *map(str, images), str(args.instructions)],
            cwd=ROOT, capture_output=True, text=True, timeout=30)
        transcript = result.stdout + result.stderr
        (output / "boot.txt").write_text(transcript)
        report = {"native_core_linked": True, "full_game_boot_verified": False,
                  "instruction_limit": args.instructions, "exit_code": result.returncode,
                  "image_sha256": {name: audit["images"][name]["sha256"] for name in ("program", "main_data")},
                  "transcript": transcript}
        for line in result.stdout.splitlines():
            if line.startswith("status="):
                report["frontier"] = dict(token.split("=", 1) for token in line.split() if "=" in token)
        (output / "boot.json").write_text(json.dumps(report, indent=2) + "\n")
        print(transcript, end="")
        print(f"Boot frontier report: {output / 'boot.json'}")
        return result.returncode
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as failure:
        print(f"segarally_boot: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
