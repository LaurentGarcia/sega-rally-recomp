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
import tempfile
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


def build_continuation_tool(compiler: list[str], output: Path) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    tool = output / "srally_continuations"
    run_logged(compiler + ["-std=c++20", "-O2", "-Isrc", "tools/srally_continuations/main.cpp",
                          "src/i960/decode.cpp", "src/i960/isa.cpp", "-o", str(tool)],
               output / "continuations-build.log")
    return tool


def continuation_seeds(program: bytes, reach: dict, relocations: dict, proof_tool: Path) -> list[dict]:
    """Prove adjacent continuations from explicit seed entries, never scan data.

    These are static candidates from the selected seed profile. Known direct
    transfers and seeds into the interior reject the reaching definition;
    unknown indirect transfers and exclusive target coverage are not claimed.
    """
    candidates = []
    with tempfile.TemporaryDirectory(prefix="srally-continuation-") as directory:
        folder = Path(directory)
        image = folder / "program.bin"
        image.write_bytes(program)
        for relocation in relocations["relocations"]:
            guest, source, size = (int(relocation[key], 16) for key in ("guest", "program_offset", "size"))
            def canonical(pc: str) -> int:
                value = int(pc, 16)
                return source + value - guest if guest <= value < guest + size else value
            reached = {canonical(pc) for pc in reach["instruction_addresses"]}
            entries = {canonical(pc) for pc in reach["seeds"]}
            trusted = entries & reached
            for name, addresses in (("trusted", trusted), ("reached", reached), ("entries", entries)):
                (folder / name).write_text("".join(f"{pc:x}\n" for pc in sorted(addresses)))
            result = subprocess.run([str(proof_tool), str(image), str(folder / "trusted"),
                str(folder / "reached"), str(folder / "entries"), hex(source), hex(guest), hex(size)],
                capture_output=True, text=True)
            if result.returncode:
                raise ValueError("continuation proof failed: " + result.stderr.strip())
            for proof in json.loads(result.stdout):
                start, target, length = proof["start"], proof["target"], proof["size"]
                if not source <= start < target < source + size or target != start + length:
                    raise ValueError("continuation target differs from verified adjacent reset-copy alias")
                candidates.append({"address": f"0x{target:08x}",
                    "runtime_address": f"0x{guest + target - source:08x}",
                    "name": f"verified_continuation_{guest + target - source:08x}",
                    "evidence": "Inherited i960 decoder straight-line continuation proof",
                    "source_address": f"0x{start:08x}", "source_size": length,
                    "source_sha256": hashlib.sha256(program[start:target]).hexdigest(),
                    "relocation": relocation["name"],
                    "provenance": "Static seed-profile continuation candidate; explicit absolute lda/mov reaching definition, decoded whitelist and known side-entry rejection. Runtime observation, exclusive indirect targets and full coverage are not implied."})
    return candidates


def object_update_seeds(program: bytes, reach: dict, relocations: dict, proof_tool: Path,
                        config: dict, roots: list[dict]) -> list[dict]:
    """Prove update registrations from constructor/update roots and a pinned caller.

    No arbitrary address scan: roots must carry an explicit object role, all
    inspected instructions must already be decoded, and the established
    dispatcher must load/guard/call the same object's +4 pointer cell.
    """
    if hashlib.sha256(program).hexdigest() != config["program_sha256"]:
        raise ValueError("object registration proof belongs to a different program")
    dispatcher = config["object_update_dispatcher"]
    copies = [row for row in relocations["relocations"] if row["name"] == dispatcher["relocation"]]
    if len(copies) != 1:
        raise ValueError("object dispatcher requires one named verified reset copy")
    copy = copies[0]
    guest, source, size = (int(copy[key], 16) for key in ("guest", "program_offset", "size"))
    start, runtime = int(dispatcher["program_offset"], 16), int(dispatcher["runtime_address"], 16)
    if (dispatcher["size"] != 16 or (start | runtime) & 3 or start < source or start + 16 > source + size or
        runtime != guest + start - source or hashlib.sha256(program[start:start+16]).hexdigest() != dispatcher["sha256"]):
        raise ValueError("object dispatcher span/hash/alias differs from pinned proof")
    def canonical(pc: str) -> int:
        value = int(pc, 16)
        return source + value - guest if guest <= value < guest + size else value
    qualified = [row for row in roots if row.get("root_role") in ("object_constructor", "object_update")]
    reached = {canonical(pc) for pc in reach["instruction_addresses"]}
    if any(start + offset not in reached for offset in (0, 4, 8, 12)):
        return []  # sparse profiles cannot establish this caller yet
    trusted = {canonical(row["address"]) for row in qualified} & reached
    entries = {canonical(pc) for pc in reach["seeds"]}
    with tempfile.TemporaryDirectory(prefix="srally-updates-") as directory:
        folder = Path(directory)
        image = folder / "program.bin"
        image.write_bytes(program)
        for name, values in (("trusted", trusted), ("reached", reached), ("entries", entries)):
            (folder / name).write_text("".join(f"{pc:x}\n" for pc in sorted(values)))
        result = subprocess.run([str(proof_tool), "--updates", str(image), str(folder / "trusted"),
            str(folder / "reached"), str(folder / "entries"), hex(source), hex(guest), hex(size), hex(start)],
            capture_output=True, text=True)
    if result.returncode:
        raise ValueError("object registration proof failed: " + result.stderr.strip())
    candidates = []
    for proof in json.loads(result.stdout):
        root, definition, store, target = (proof[key] for key in ("root", "definition", "store", "target"))
        if root not in trusted or not source <= root <= definition < store < source + size or not guest <= target < guest + size:
            raise ValueError("object registration target/span differs from verified roots/copy")
        candidates.append({"address": f"0x{source + target - guest:08x}",
            "runtime_address": f"0x{target:08x}", "root_role": "object_update",
            "name": f"verified_registered_object_update_{target:08x}",
            "evidence": "Inherited decoder constructor/update pointer-cell proof",
            "source_address": f"0x{root:08x}", "source_size": store + 4 - root,
            "source_sha256": hashlib.sha256(program[root:store+4]).hexdigest(),
            "definition_address": f"0x{definition:08x}", "store_address": f"0x{store:08x}",
            "value_register": proof["register"], "dispatcher_provenance": dispatcher,
            "source_seed_provenance": [row for row in qualified if canonical(row["address"]) == root],
            "provenance": "Static object update registration candidate from a scoped constructor/update root, preserved object base and constant value, known side-entry rejection, and the pinned +4/null-guard/call dispatcher. Runtime observation, immutable pointer cells and exclusive target coverage are not implied."})
    return candidates


def object_update_roots(derived: list[dict], seeds: list[dict], registrations: list[dict]) -> list[dict]:
    """Keep inferred roles separate from existing label/observed seed metadata.

    Registrations qualify their targets even when those addresses already
    belonged to the native seed profile; those update chains need analysis too.
    """
    return ([dict(row, root_role="object_constructor") for row in derived
             if row.get("table") == "object_callback_type"] +
            [row for row in seeds if row.get("root_role") == "object_update"] + registrations)


def table_seeds(program: bytes, config: dict, relocations: dict, proof_tool: Path | None = None) -> list[dict]:
    """Verify individually audited bounded dispatch tables, not arbitrary scans.

    The hashes identify exact code/data in the supplied revision. Independent
    operand checks prove the bound, index scale, load/branch register and reset
    copy aliases; candidates retain provenance separate from observed seeds.
    """
    if (config["romset"] != "srallycb" or config["board"] != "model2a" or
            hashlib.sha256(program).hexdigest() != config["program_sha256"]):
        raise ValueError("table seed provenance belongs to a different program image")
    candidates = []
    number = lambda value: int(value, 16) if isinstance(value, str) else value
    for table in config["tables"]:
        matches = [row for row in relocations["relocations"] if row["name"] == table["relocation"]]
        if len(matches) != 1:
            raise ValueError("table requires one named verified relocation")
        relocation = matches[0]
        guest, source, size = (number(relocation[key]) for key in ("guest", "program_offset", "size"))
        if (guest < 0x500000 or size <= 0 or guest + size > 0x600000 or source < 0 or
                source + size > len(program) or (guest | source | size) & 3):
            raise ValueError("table relocation exceeds aligned program/work RAM")

        def verified_span(offset: int, runtime: int, length: int, digest: str) -> bytes:
            if (length <= 0 or (offset | runtime | length) & 3 or offset < source or
                    offset + length > source + size or runtime != guest + offset - source):
                raise ValueError("table evidence lies outside its exact reset-copy alias")
            data = program[offset:offset + length]
            if hashlib.sha256(data).hexdigest() != digest:
                raise ValueError("table code/data evidence hash differs from supplied program")
            return data

        if table.get("kind") in ("masked_callback", "compare_dispatch"):
            if proof_tool is None or len(table["code_evidence"]) != 1:
                raise ValueError("masked callback requires the inherited-decoder proof tool and one body span")
            body = table["code_evidence"][0]
            if body["role"] != "body":
                raise ValueError("masked callback body evidence role differs")
            start = number(body["program_offset"])
            verified_span(start, number(body["runtime_address"]), body["size"], body["sha256"])
            with tempfile.TemporaryDirectory(prefix="srally-table-") as directory:
                image = Path(directory) / "program.bin"
                image.write_bytes(program)
                mode = "--masked" if table["kind"] == "masked_callback" else "--compare"
                result = subprocess.run([str(proof_tool), mode, str(image), hex(start),
                    hex(source), hex(guest), hex(size)], capture_output=True, text=True)
            if result.returncode:
                raise ValueError("masked callback operand/dataflow proof failed: " + result.stderr.strip())
            proof = json.loads(result.stdout)
            count, address, offset = table["entry_count"], number(table["runtime_address"]), number(table["program_offset"])
            expected = {"table": address, "count": count, "size": body["size"]}
            if table["kind"] == "compare_dispatch":
                expected["fallback"] = number(table["rejected_index_target"])
            if proof != expected:
                raise ValueError("masked callback decoded proof differs from declared table/body")
            data = verified_span(offset, address, count * 4, table["table_sha256"])
            targets = list(struct.unpack("<" + "I" * count, data))
            if targets != [number(target) for target in table["targets"]]:
                raise ValueError("masked callback entries differ from audited metadata")
            for target in dict.fromkeys(targets):
                if target == 0 and table["kind"] == "masked_callback":
                    continue  # decoded cmpi/be skips null callbacks
                if target & 3 or not guest <= target < guest + size:
                    raise ValueError("masked callback target lies outside aligned verified reset copy")
                candidates.append({"address": f"0x{source + target - guest:08x}",
                    "runtime_address": f"0x{target:08x}", "table": table["name"],
                    "table_indices": [i for i, value in enumerate(targets) if value == target],
                    "name": f"verified_table_{table['name']}_target_{target:08x}",
                    "evidence": "config/segarally_table_seeds.json",
                    "provenance": ("Verified masked callback table candidate with null guard and saved-pointer dataflow" if table["kind"] == "masked_callback" else
                        "Verified unsigned compare/branch dispatch candidate with preserved index/flags through frame spills") +
                        "; runtime observation, immutable live RAM tables and full coverage are not implied."})
            continue

        evidence = {}
        for row in table["code_evidence"]:
            role = row["role"]
            if role not in ("bound", "load", "branch") or role in evidence:
                raise ValueError("table requires unique bound/load/branch evidence")
            data = verified_span(number(row["program_offset"]), number(row["runtime_address"]),
                                 row["size"], row["sha256"])
            evidence[role] = (number(row["runtime_address"]), data)
        if set(evidence) != {"bound", "load", "branch"}:
            raise ValueError("incomplete table control-flow evidence")
        count, index = table["entry_count"], table["index_register"]
        destination = table.get("destination_register", index)
        if not 1 <= count <= 32 or not 0 <= index < 32 or not 0 <= destination < 32 or table["maximum_index"] != count - 1:
            raise ValueError("table bound must prove exactly the declared entry count")
        bound_pc, bound = evidence["bound"]
        load_pc, load = evidence["load"]
        branch_pc, branch = evidence["branch"]
        if len(bound) != 4 or len(load) != 8 or len(branch) != 4 or load_pc != bound_pc + 4 or branch_pc != load_pc + 8:
            raise ValueError("table control flow is not a contiguous bound/load/bx sequence")
        bound_word, load_word, branch_word = (struct.unpack_from("<I", data)[0] for data in (bound, load, branch))
        # COBR cmpobl compares literal maximum to unsigned index; taken on
        # maximum < index. Its signed13 displacement is relative to opcode PC.
        displacement = bound_word & 0x1ffc
        if displacement & 0x1000:
            displacement -= 0x2000
        rejected = number(table["rejected_index_target"])
        if (bound_word >> 24 != 0x34 or not bound_word & 0x2000 or
                (bound_word >> 19) & 31 != count - 1 or (bound_word >> 14) & 31 != index or
                bound_word & 3 or bound_pc + displacement != rejected or not guest <= rejected < guest + size):
            raise ValueError("table unsigned bounds check differs from audited proof")
        address, offset = number(table["runtime_address"]), number(table["program_offset"])
        # MEMB index+displacement, scale2 (=4 bytes). The bounded index
        # and loaded destination may differ; bx must use the destination.
        if (load_word >> 24 != 0x90 or (load_word >> 10) & 15 != 14 or
                (load_word >> 7) & 7 != 2 or (load_word & 31) != index or
                (load_word >> 19) & 31 != destination or load_word & 0x60 or
                struct.unpack_from("<I", load, 4)[0] != address):
            raise ValueError("table indexed load differs from audited proof")
        if (branch_word >> 24 != 0x84 or (branch_word >> 14) & 31 != destination or
                branch_word & 0x3fff != 0x1000):
            raise ValueError("table indirect branch does not use the loaded register")
        data = verified_span(offset, address, count * 4, table["table_sha256"])
        targets = list(struct.unpack("<" + "I" * count, data))
        if targets != [number(target) for target in table["targets"]]:
            raise ValueError("table entries differ from audited target metadata")
        for target in dict.fromkeys(targets):
            if target & 3 or not guest <= target < guest + size:
                raise ValueError("table target lies outside its aligned reset-copy code range")
            candidates.append({"address": f"0x{source + target - guest:08x}",
                "runtime_address": f"0x{target:08x}", "table": table["name"],
                "table_indices": [i for i, value in enumerate(targets) if value == target],
                "name": f"verified_table_{table['name']}_target_{target:08x}",
                "evidence": "config/segarally_table_seeds.json",
                "provenance": "Verified bounded table candidate; runtime observation and full coverage are not implied."})
    return candidates


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
        table_config = json.loads((ROOT / "config/segarally_table_seeds.json").read_text())
        proof_tool = build_continuation_tool(compiler, BUILD / "tools")
        derived = table_seeds(program, table_config, relocations, proof_tool)
        combined = {"seeds": seeds["seeds"] + observed["seeds"] + derived}
        relocation_flags, aliases = relocation_seeds(program, combined, relocations)
        seedfile = BUILD / "seeds.txt"
        seedfile.write_text("".join(row["address"][2:] + "\n" for row in combined["seeds"] + aliases))
        report = BUILD / "reachability.json"
        run_logged([str(tool), str(BUILD / "rom_cache" / "program.bin"), str(generated),
                    "--board", "model2a", "--main-data", str(BUILD / "rom_cache" / "main_data.bin"),
                    "--seeds", str(seedfile), "--report", str(report)] + relocation_flags, BUILD / "recompile.log")
        reach = json.loads(report.read_text())
        continuations, registrations = [], []
        for iteration in range(16):
            continued = continuation_seeds(program, reach, relocations, proof_tool)
            for row in continued:
                row["source_seed_provenance"] = [{key: seed[key] for key in
                    ("address", "name", "evidence", "provenance", "table", "source_address") if key in seed}
                    for seed in combined["seeds"] if seed["address"] == row["source_address"]]
            update_roots = object_update_roots(derived, combined["seeds"], registrations)
            registered = object_update_seeds(program, reach, relocations, proof_tool, table_config, update_roots)
            new_update_roots = {row["address"] for row in registered} - {row["address"] for row in update_roots}
            proven = continued + registered
            prior = {row["address"] for row in combined["seeds"]}
            additions = [row for row in proven if row["address"] not in prior]
            continuations.extend(row for row in continued if row["address"] not in {item["address"] for item in continuations})
            registrations.extend(row for row in registered if (row["address"], row["store_address"]) not in
                {(item["address"], item["store_address"]) for item in registrations})
            if not additions and not new_update_roots:
                break
            if not additions:
                continue  # newly qualified existing labels need another proof pass, not recompilation
            combined["seeds"].extend(additions)
            relocation_flags, aliases = relocation_seeds(program, combined, relocations)
            seedfile.write_text("".join(row["address"][2:] + "\n" for row in combined["seeds"] + aliases))
            for stale in generated.glob("*.cpp"):
                stale.unlink()
            run_logged([str(tool), str(BUILD / "rom_cache/program.bin"), str(generated),
                "--board", "model2a", "--main-data", str(BUILD / "rom_cache/main_data.bin"),
                "--seeds", str(seedfile), "--report", str(report)] + relocation_flags,
                BUILD / f"continuations-{iteration}.log")
            reach = json.loads(report.read_text())
        else:
            raise ValueError("continuation proof did not reach a fixed point within 16 passes")
        compiled = compile_generated(compiler, generated, objects, args.jobs)
        audit.update({"seed_profile": args.seed_profile, "seed_provenance": seeds,
                      "observed_seed_provenance": observed, "relocation_provenance": relocations,
                      "table_seed_provenance": table_config, "table_derived_candidates": derived,
                      "continuation_candidates": continuations,
                      "object_update_candidates": registrations,
                      "indirect_seed_analysis": {"version": 1,
                          "helper": "tools/srally_continuations/main.cpp",
                          "helper_sha256": hashlib.sha256((ROOT / "tools/srally_continuations/main.cpp").read_bytes()).hexdigest(),
                          "decoder_sha256": hashlib.sha256((ROOT / "src/i960/decode.cpp").read_bytes()).hexdigest(),
                          "seed_file_sha256": hashlib.sha256(seedfile.read_bytes()).hexdigest(),
                          "runtime_ram_word_guards_preserved": True,
                          "limits": "Static candidates only; unknown indirect side entries and exclusive target coverage are not established."},
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
