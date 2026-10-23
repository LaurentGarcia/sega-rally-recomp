#!/usr/bin/env python3
"""Compile Rally's actual sound reset path with the shared Daytona generator.

Requires segarally_sound.py's verified import. Default mode preserves the reset
probe's absent-chip frontier. --timed attaches the native SCSP and accounts
instruction totals; MMIO remains at the previous instruction boundary, so this
is a bounded diagnostic, not bus-phase or full sound-board timing parity.
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


def observed_seeds(program: bytes, config: dict) -> list[int]:
    digest = hashlib.sha256(program).hexdigest()
    if config.get("schema") != 1 or config.get("romset") != "srallycb" or config.get("program_sha256") != digest:
        raise ValueError("sound seed provenance differs from verified program")
    addresses = []
    for row in config["seeds"]:
        address = int(row["address"], 16)
        if address & 1 or not 0x600000 <= address < 0x680000:
            raise ValueError("observed sound seed must be aligned program ROM")
        if row.get("observed_status") != "missing_code":
            raise ValueError("sound seed requires an actual native dispatcher frontier")
        addresses.append(address)
    return sorted(set(addresses))


def table_candidates(program: bytes, config: dict, proof_tool: Path, seed_file: Path, image: Path) -> list[dict]:
    """Validate exact source spans and decoded bounds; do not scan for pointers."""
    if (config.get("schema") != 1 or config.get("romset") != "srallycb" or config.get("board") != "model2a"
            or hashlib.sha256(program).hexdigest() != config.get("program_sha256")):
        raise ValueError("sound table provenance differs from verified program")
    result = []
    for table in config["tables"]:
        if table["kind"] != "nibble_pc_relative_jumps" or table["entries"] != 16 or table["stride"] != 4:
            raise ValueError("unsupported sound table proof kind or dimensions")
        start, jump, base = (int(table[key], 16) for key in ("start", "jump", "table_base"))
        body_size = table["body_size"]
        if (start & 1 or base & 1 or not 0x600000 <= start < jump < 0x680000
                or body_size != jump + 2 - start or not jump + 2 <= base <= 0x680000 - 64):
            raise ValueError("sound table evidence outside aligned program")
        body = program[start-0x600000:jump+2-0x600000]
        slots = program[base-0x600000:base+64-0x600000]
        if hashlib.sha256(body).hexdigest() != table["body_sha256"] or hashlib.sha256(slots).hexdigest() != table["table_sha256"]:
            raise ValueError("sound table source/table evidence hash differs")
        checked = subprocess.run([str(proof_tool), str(image), hex(start), hex(jump), str(seed_file)],
                                 cwd=ROOT, capture_output=True, text=True)
        if checked.returncode:
            raise ValueError("sound dispatch proof failed: " + checked.stderr.strip())
        proof = json.loads(checked.stdout)
        if any(proof[key] != value for key, value in (("start",start),("jump",jump),("body_size",body_size),
                ("table_base",base),("entries",16),("stride",4))):
            raise ValueError("decoded sound dispatch differs from declared evidence")
        if [row["address"] for row in proof["candidates"]] != list(range(base,base+64,4)):
            raise ValueError("sound proof returned an incomplete bounded candidate set")
        for row in proof["candidates"]:
            result.append({"address":row["address"], "direct_target":row["direct_target"], "table":table["name"],
                "classification":"static_bounded_candidate", "observed":False,
                "provenance":"Decoded nibble bound and stride, same-register dataflow, program jump slots, source hashes and known side-entry checks. Unknown indirect entries and actual execution are not implied."})
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--max-instructions", type=int, default=1_000_000)
    parser.add_argument("--timed", action="store_true", help="bounded instruction timing with native SCSP; MMIO at prior boundary")
    parser.add_argument("--compact-scsp", action="store_true", help="Use exact compact SCSP gain tables in the timed execution probe")
    args = parser.parse_args()
    try:
        if args.max_instructions <= 0:
            raise ValueError("instruction limit must be positive")
        if args.compact_scsp and not args.timed:
            raise ValueError("--compact-scsp requires --timed")
        build = build_path(args.build_dir)
        spec = json.loads((ROOT / "config/segarally_sound_boot.json").read_text())
        images = verified_images(build, spec)
        output = build_path(build / ("sound/native-timed" if args.timed else "sound/native"))
        if not output.is_relative_to(build):
            raise ValueError("native sound output is outside the audited build directory")
        output.mkdir(parents=True, exist_ok=True)
        compiler = shlex.split(args.compiler)
        flags = ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc"]
        generator_sources = ["tools/m2sndrecomp/main.cpp", "src/m68k/decode.cpp", "src/m68k/reach.cpp", "src/m68k/timing.cpp"]
        runtime_sources = ["tools/srally_sound_boot/main.cpp", "src/runtime/snd_cpu.cpp", "src/runtime/model2a_sound_bus.cpp"]
        generation_flags = []
        candidates = []
        proof_sources = []
        runtime_flags = []
        if args.compact_scsp:
            runtime_flags.append("-DM2_COMPACT_SOUND=1")
        seed_config_path = "config/segarally_sound_observed_seeds.json"
        if args.timed:
            generation_flags += ["--timing", "model2a-boundary"]
            config = json.loads((ROOT / seed_config_path).read_text())
            seeds = observed_seeds(images["audiocpu"].read_bytes(), config)
            seed_file = output / "observed-seeds.txt"
            seed_file.write_text("".join(f"{address:x}\n" for address in seeds))
            proof_sources = ["tools/srally_sound_tables/main.cpp", "src/m68k/decode.cpp", "src/m68k/reach.cpp"]
            proof_tool = output / "srally_sound_tables"
            run_logged(compiler + flags + proof_sources + ["-o", str(proof_tool)], output / "table-proof-build.log")
            tables = json.loads((ROOT / "config/segarally_sound_tables.json").read_text())
            program = images["audiocpu"].read_bytes()
            candidates = table_candidates(program, tables, proof_tool, seed_file, images["audiocpu"])
            combined_file = output / "seeds.txt"
            combined = sorted(set(seeds) | {row["address"] for row in candidates})
            combined_file.write_text("".join(f"{address:x}\n" for address in combined))
            # New static candidates may expose direct side entries into a proof body.
            # Recheck against the complete candidate reachability before generation.
            if table_candidates(program, tables, proof_tool, combined_file, images["audiocpu"]) != candidates:
                raise ValueError("sound candidate proof changed under combined reachability")
            (output / "table-proof.json").write_text(json.dumps(candidates, indent=2) + "\n")
            generation_flags += ["--seeds", str(combined_file)]
            runtime_flags += ["-DSEGARALLY_SOUND_TIMED_PROBE"]
            runtime_sources += ["src/runtime/model2_scsp.cpp", "src/runtime/model2_scsp_dsp.cpp", "src/runtime/model2a_clock.cpp"]
        tool = output / "m2sndrecomp"
        generated = output / "snd_gen.cpp"
        run_logged(compiler + flags + generator_sources + ["-o", str(tool)], output / "generator-build.log")
        run_logged([str(tool), str(images["audiocpu"]), str(generated), "--board", "model2a",
                    "--audit", str(output / "generation.json"), *generation_flags], output / "generation.log")
        executable = output / "srally_sound_boot"
        run_logged(compiler + flags + runtime_flags + runtime_sources + [str(generated), "-o", str(executable)], output / "build.log")
        execution = subprocess.run([str(executable), str(images["audiocpu"]), str(images["samples"]),
                                    str(args.max_instructions), *(["--timed"] if args.timed else [])], cwd=ROOT, capture_output=True, text=True, timeout=30)
        (output / "boot.txt").write_text(execution.stdout + execution.stderr)
        result = json.loads(execution.stdout) if execution.stdout.strip() else {"status": "error"}
        expected = spec["expected_frontier"]
        reached = (execution.returncode == 3 and result.get("status") == "frontier"
                   and all(result.get(key) == int(expected[key], 16) for key in ("pc", "address", "data", "mask"))
                   and result.get("region") == expected["region"] and result.get("write") == expected["write"]
                   and result.get("reset_pc") == int(spec["expected_reset"]["pc"], 16)
                   and result.get("reset_ssp") == int(spec["expected_reset"]["ssp"], 16)
                   and result.get("ram_nonzero_bytes") == 0)
        timed_reached = (args.timed and execution.returncode in (3, 4)
                         and result.get("status") in ("missing_code", "execution_frontier", "instruction_limit")
                         and result.get("scsp_writes", 0) > 0 and result.get("cycles", 0) > 38)
        sources = generator_sources + runtime_sources + proof_sources + ["src/m68k/decode.h", "src/m68k/reach.h", "src/m68k/timing.h",
            "src/runtime/snd_cpu.h", "src/runtime/snd_gen_support.h", "src/runtime/snd_sched.h",
            "src/runtime/model2a_sound_bus.h", "src/runtime/sound_image_source.h", "config/segarally_sound_boot.json",
            "config/segarally_sound.json", "scripts/segarally_sound_recompile.py"]
        if args.timed:
            sources += [seed_config_path, "config/segarally_sound_tables.json", "src/runtime/model2_scsp.h", "src/runtime/scsp_pan_table.h", "src/runtime/model2_scsp_dsp.h", "src/runtime/model2a_clock.h"]
        report = {"schema": 1, "romset": spec["romset"], "board": spec["board"],
                  "images_sha256": spec["images"], "compiler": compiler, "flags": flags, "runtime_flags": runtime_flags,
                  "sources_sha256": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sources},
                  "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
                  "generation": json.loads((output / "generation.json").read_text()),
                  "static_seed_candidates": candidates,
                  "execution": result, "exit_code": execution.returncode,
                  "expected_scsp_frontier_reached": reached, "sound_chip_attached": args.timed,
                  "native_scsp_startup_reached": timed_reached,
                  "timing_reference_commit": "dddd73680656e355bb2b5beecab1167c9f07bf81" if args.timed else None,
                  "timing_scope": ("Pinned M68000 instruction totals, mapped RAM/SCSP waits and phase-dependent autovector IACK; actual MMIO at previous instruction boundary. Modeled prefetch transfers do not read values. Live code guards excluded from bus counts. Exceptions other than autovector/reset are explicit frontiers. No bus-phase or interrupt-capture parity."
                                   if args.timed else spec["timing_scope"]), "full_sound_parity_verified": False}
        (output / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
        print((output / "generation.log").read_text(), end="")
        print(execution.stdout + execution.stderr, end="")
        print(f"Native sound audit: {output / 'audit.json'}")
        return 0 if (timed_reached if args.timed else reached) else 1
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as failure:
        print(f"segarally_sound_recompile: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
