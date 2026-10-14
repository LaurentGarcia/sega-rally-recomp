#!/usr/bin/env python3
"""Verify the supplied Revision B ROMs and build a seeded i960 bootstrap.

This reuses Daytona's decoder, reachability and native emitter. It does not
build or claim a working Sega Rally game. Every derived image/source/object
and its audit are written under the ignored build/segarally directory.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "segarally"


def load_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text())
    if manifest["romset"] != "srallycb" or manifest["board"] != "model2a":
        raise ValueError("This bootstrap requires the srallycb Model2A manifest")
    return manifest


def interleave_words(region: bytearray, offset: int, data: bytes) -> None:
    if len(data) % 2 or offset % 4 not in (0, 2):
        raise ValueError("ROM_LOAD32_WORD requires even data and lane 0 or 2")
    end = offset + len(data) * 2
    if end - 2 > len(region):
        raise ValueError("ROM load exceeds its declared region")
    region[offset:end:4] = data[0::2]
    region[offset + 1:end:4] = data[1::2]


def extract(archive: Path, manifest: dict, output: Path) -> dict:
    checked, images = [], {}
    with zipfile.ZipFile(archive) as roms:
        entries = {}
        for entry in roms.infolist():
            if entry.is_dir():
                continue
            name = Path(entry.filename).name
            if name in entries:
                raise ValueError(f"Ambiguous archive filename: {name}")
            entries[name] = entry
        # Validate everything before writing any imported image.
        for region in manifest["regions"]:
            image = bytearray([region["fill"]]) * region["size"]
            loaded = 0
            for load in region["loads"]:
                name = load["file"]
                if name not in entries:
                    raise ValueError(f"Missing {name}; use the Revision B program pair")
                data = roms.read(entries[name])
                crc = f"{zlib.crc32(data) & 0xffffffff:08x}"
                sha1 = hashlib.sha1(data).hexdigest()
                if len(data) != load["size"] or crc != load["crc32"] or sha1 != load["sha1"]:
                    raise ValueError(f"ROM verification failed: {name} size={len(data):#x} CRC={crc} SHA1={sha1}")
                if load["load"] != "ROM_LOAD32_WORD":
                    raise ValueError("Unsupported load operation in bootstrap manifest")
                interleave_words(image, load["offset"], data)
                loaded += len(data)
                checked.append({"file": name, "size": len(data), "crc32": crc, "sha1": sha1})
            images[region["name"]] = (image, loaded)
    reset = struct.unpack_from("<I", images["program"][0], 12)[0]
    if reset != int(manifest["expected_reset_ip"], 16):
        raise ValueError(f"Unexpected reset IP {reset:#x}")
    output.mkdir(parents=True, exist_ok=True)
    image_audit = {}
    for name, (image, loaded) in images.items():
        target = output / f"{name}.bin"
        target.write_bytes(image)
        image_audit[name] = {"path": str(target.relative_to(ROOT)), "region_bytes": len(image),
                             "loaded_bytes": loaded, "sha256": hashlib.sha256(image).hexdigest()}
    return {"romset": manifest["romset"], "board": manifest["board"], "source": manifest["source"],
            "verified_files": checked, "images": image_audit, "reset_ip": f"0x{reset:08x}"}


def run_logged(command: list[str], logfile: Path) -> None:
    with logfile.open("w") as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        tail = "\n".join(logfile.read_text(errors="replace").splitlines()[-25:])
        raise RuntimeError(f"Command exited {result.returncode}; {logfile}\n{tail}")


def build_generator(compiler: list[str], output: Path) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    tool = output / ("m2recomp.exe" if os.name == "nt" else "m2recomp")
    run_logged(compiler + ["-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-Isrc",
                          "tools/m2recomp/main.cpp", "src/i960/isa.cpp", "src/i960/decode.cpp",
                          "src/i960/reach.cpp", "src/i960/timing.cpp", "-o", str(tool)], output / "generator-build.log")
    return tool


def relocation_seeds(program: bytes, seeds: dict, config: dict) -> tuple[list[str], list[dict]]:
    """Verify the reset-copy operands and add provenance-backed RAM aliases.

    Full loop semantics were decoded from this checksum-verified Revision B
    image; these checks fence the source/destination/count operands as well.
    This is analysis metadata, never a mutable-RAM bus mapping.
    """
    flags, aliases = [], []
    for row in config["relocations"]:
        guest, source, size = (int(row[key], 16) for key in ("guest", "program_offset", "size"))
        evidence = row["evidence"]
        for key, expected, register in (("destination_lda_pc", guest, 22), ("source_lda_pc", source, 21)):
            pc = int(evidence[key], 16)
            word, value = struct.unpack_from("<II", program, pc)
            if word >> 24 != 0x8c or (word >> 19) & 31 != register or (word >> 10) & 15 != 12 or value != expected:
                raise ValueError(f"Reset relocation {key} operand does not match verified copy")
        word = struct.unpack_from("<I", program, int(evidence["count_shlo_pc"], 16))[0]
        opcode = (word >> 20) & 0xff0 | (word >> 7) & 15
        if (opcode != 0x59c or not word & (1 << 11) or not word & (1 << 12)
                or (word >> 19) & 31 != 29 or ((word >> 14) & 31) << (word & 31) != size // 4):
            raise ValueError("Reset relocation word count does not match verified copy")
        if size % 4 or source + size > len(program) or not 0x500000 <= guest < guest + size <= 0x600000:
            raise ValueError("Relocation exceeds verified program or work RAM")
        flags += ["--relocated-code", f"{guest:x}:{source:x}:{size:x}"]
        for seed in seeds["seeds"]:
            pc = int(seed["address"], 16)
            if source <= pc < source + size:
                aliases.append({"address": f"0x{guest + pc - source:08x}", "original_rom_address": seed["address"],
                                "name": seed["name"], "evidence": seed["evidence"],
                                "relocation": row["name"]})
    return flags, aliases


def compile_generated(compiler: list[str], generated: Path, output: Path, jobs: int) -> int:
    sources = sorted(generated.glob("*.cpp"))
    if not sources:
        raise RuntimeError("The recompiler produced no sources")
    output.mkdir(parents=True, exist_ok=True)
    def compile_one(source: Path) -> None:
        run_logged(compiler + ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc",
                              "-c", str(source), "-o", str(output / (source.stem + ".o"))],
                   output / (source.stem + ".log"))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as workers:
        list(workers.map(compile_one, sources))
    return len(sources)


def main() -> int:
    global BUILD
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, default=ROOT / "roms" / "srallyc.zip")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--build-dir", type=Path, default=BUILD,
                        help="Separate pipeline output directory beneath this checkout's build/")
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--seed-profile", choices=("bootstrap", "lift-labels"), default="bootstrap",
                        help="Sparse known bootstrap labels or opt-in prior-lift address candidates")
    args = parser.parse_args()
    BUILD = (args.build_dir if args.build_dir.is_absolute() else ROOT / args.build_dir).resolve()
    if not BUILD.is_relative_to((ROOT / "build").resolve()) or BUILD == (ROOT / "build").resolve():
        parser.error("--build-dir must be a child of this checkout's ignored build/ directory")
    if args.jobs < 1 or args.jobs > 8:
        parser.error("--jobs must be between 1 and 8")
    compiler = shlex.split(args.compiler)
    try:
        manifest = load_manifest(ROOT / "config" / "segarally_roms.json")
        audit = extract(args.roms.expanduser().resolve(), manifest, BUILD / "rom_cache")
        audit.update({"milestone": "seeded i960 tooling bootstrap", "complete_game_coverage": False,
                      "runtime_boot_verified": False, "native_game_linked": False,
                      "rom_import_scope": [region["name"] for region in manifest["regions"]],
                      "floating_point_basis": "Inherited MAME host-double reference semantics; hardware/extF80 parity unverified",
                      "known_remaining_work": ["Model2A board/runtime adaptation", "indirect target harvest",
                                               "TGP program identification and parity", "SCSP sound board",
                                               "Sega Rally input replay and full-race parity"]})
        (BUILD / "import.json").write_text(json.dumps(audit, indent=2) + "\n")
        print(f"Verified {len(audit['verified_files'])} Revision B ROMs; reset IP {audit['reset_ip']}", flush=True)
        if args.verify_only:
            return 0
        tool = build_generator(compiler, BUILD / "tools")
        generated, objects = BUILD / "gen", BUILD / "objects"
        generated.mkdir(parents=True, exist_ok=True)
        # Remove only this pipeline's previous derived output, never source.
        for stale in generated.glob("*.cpp"):
            stale.unlink()
        for stale in objects.glob("*.o"):
            stale.unlink()
        seed_config = "segarally_seeds.json" if args.seed_profile == "bootstrap" else "segarally_lift_seeds.json"
        seeds = json.loads((ROOT / "config" / seed_config).read_text())
        observed = json.loads((ROOT / "config/segarally_observed_seeds.json").read_text())
        program = (BUILD / "rom_cache/program.bin").read_bytes()
        program_hash = hashlib.sha256(program).hexdigest()
        for row in observed["seeds"]:
            if row["program_sha256"] != program_hash:
                raise ValueError("Observed target provenance belongs to a different program image")
        relocations = json.loads((ROOT / "config" / "segarally_relocations.json").read_text())
        combined = {"seeds": seeds["seeds"] + observed["seeds"]}
        relocation_flags, aliases = relocation_seeds(program, combined, relocations)
        seedfile = BUILD / "seeds.txt"
        seedfile.write_text("".join(row["address"][2:] + "\n" for row in combined["seeds"] + aliases))
        report = BUILD / "reachability.json"
        run_logged([str(tool), str(BUILD / "rom_cache" / "program.bin"), str(generated),
                    "--board", "model2a", "--main-data", str(BUILD / "rom_cache" / "main_data.bin"),
                    "--seeds", str(seedfile), "--report", str(report)] + relocation_flags, BUILD / "recompile.log")
        reach = json.loads(report.read_text())
        compiled = compile_generated(compiler, generated, objects, args.jobs)
        audit.update({"seed_profile": args.seed_profile, "seed_provenance": seeds,
                      "observed_seed_provenance": observed, "relocation_provenance": relocations,
                      "relocated_seed_provenance": aliases, "reachability": reach,
                      "native_objects_compiled": True, "native_object_count": compiled,
                      "compiler_command": compiler})
        (BUILD / "coverage.json").write_text(json.dumps(audit, indent=2) + "\n")
        print(f"Generated {reach['instruction_count']} seeded instructions; compiled {compiled} native objects")
        print(f"Unresolved indirect sites: {len(reach['indirect_sites'])}; "
              f"invalid stops: {len(reach['invalid_stops'])}; unmapped targets: {len(reach['unmapped_targets'])}")
        print("This is a tooling bootstrap; Sega Rally boot and full-game coverage remain unverified.")
        print(f"Audit: {BUILD / 'coverage.json'}")
        return 0
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile, KeyError) as error:
        print(f"segarally_recompile: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
