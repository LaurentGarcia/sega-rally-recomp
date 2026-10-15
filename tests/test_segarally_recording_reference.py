#!/usr/bin/env python3
"""Build the test-only sound-OFF whole-recording consumer from verified objects.

No ROM generation, native chunk compilation, SDL or GPU work. --run is explicit
and bounded. The measured host build records bind existing files; they are not
cryptographic proof of upstream generation or PCB behavior. Signed/repackaged
hosts need their own matching build evidence; this first consumer accepts the
preserved raw executable only. Production same-executable replay is unchanged.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MAX_FILE = 256 * 1024 * 1024
RUNTIME = ("model2a_machine model2a_bus cpu lockstep model2a_clock model2a_control "
           "model2_io model2a_io model2_eeprom model2a_comm comm_board model2_uart "
           "model2a_uart tgp m2_tgp_board model2a_video geo video raster").split()


def identity(path: Path, limit: int = MAX_FILE) -> dict:
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError(f"missing/oversized file: {path}")
    digest, count = hashlib.sha256(), 0
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            count += len(block)
            if count > limit:
                raise ValueError(f"file grew beyond bound: {path}")
            digest.update(block)
    return {"bytes": count, "sha256": digest.hexdigest()}


def strict_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path: Path, limit: int = 16 * 1024 * 1024):
    identity(path, limit)
    return json.loads(path.read_bytes(), object_pairs_hook=strict_pairs)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def confined(path: Path, parent: Path) -> Path:
    path = path.resolve()
    require(path.is_relative_to(parent) and path != parent, f"path outside required directory: {path}")
    return path


def module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def verify_host(build: Path, evidence: Path):
    """Check original measured source/object identities, actual link outputs,
    and matching compile commands. Never execute commands supplied by audits.
    """
    commands = read_json(build / "compile_commands.json")
    proof = read_json(evidence / "compiled-object-contract.json")
    artifacts = read_json(evidence / "linked-artifact-identities.json")
    require(isinstance(proof, list) and 1 <= len(proof) <= 4096, "invalid host object inventory")
    require(len(commands) >= len(proof), "host compile inventory changed")
    actual_commands = {(entry["file"], entry["directory"], entry["command"]) for entry in commands}
    require(len(actual_commands) == len(commands), "duplicate host compile command")
    files, sources = set(), {}
    for entry in proof:
        source = Path(entry["source"]).resolve()
        obj = confined(build / entry["object"], build)
        directory = Path(entry["directory"]).resolve()
        require(directory == build or directory.is_relative_to(build), "host object directory changed")
        require((str(source), str(directory), entry["command"]) in actual_commands, "host compile command changed")
        require(identity(source) == entry["source_identity"], f"host source changed: {source}")
        require(identity(obj) == entry["object_identity"], f"host object changed: {obj}")
        require(source not in sources, "duplicate host source")
        files.update((source, obj)); sources[source] = obj
    for name, expected in artifacts.items():
        path = confined(build / name, build)
        require(identity(path) == expected, f"measured linked artifact changed: {name}")
        files.add(path)
    require("segarally_native" in artifacts, "missing actual host executable measurement")
    # CMake also lists EXCLUDE_FROM_ALL test targets. They were never linked
    # into the captured host and need not have a compiled-object measurement.
    unmatched = [entry for entry in commands if Path(entry["file"]).resolve() not in sources]
    require(all(Path(entry["file"]).name.startswith("test_") for entry in unmatched), "unmeasured host compile input")
    for label, target in (("native", "segarally_native"), ("recording", "segarally_recording")):
        link_path = build / "CMakeFiles" / (target + ".dir") / "link.txt"
        recorded = evidence / (label + "-link.txt")
        require(identity(link_path) == identity(recorded), "actual host link command changed")
        for token in shlex.split(link_path.read_text()):
            if token.endswith(".o"):
                require((build / token).resolve() in sources.values(), "linked object lacks original measurement")
        files.update((link_path, recorded))
    machines = [p for p in sources if p.as_posix().endswith("/src/runtime/model2a_machine.cpp")]
    require(len(machines) == 1, "ambiguous host runtime root")
    host_root = machines[0].parents[2]
    # Runtime source/header compatibility is checked from actual files. The
    # recording parser and presentation source intentionally have separate
    # identities and are not claimed to be the independent CPU implementation.
    pending = [host_root / "src/runtime" / (name + ".cpp") for name in RUNTIME]
    require(all(p in sources for p in pending), "host runtime object measurement missing")
    visited = set()
    import re
    while pending:
        path = pending.pop().resolve()
        if path in visited:
            continue
        visited.add(path)
        relative = path.relative_to(host_root)
        require(identity(path) == identity(ROOT / relative), f"unsupported runtime/header difference: {relative}")
        files.update((path, ROOT / relative))
        for include in re.findall(r'^\s*#include\s+"([^"\n]+)"', path.read_text(), re.MULTILINE):
            candidates = (path.parent / include, host_root / "src" / include, host_root / include)
            match = next((p for p in candidates if p.is_file()), None)
            require(match is not None, f"unresolved runtime header: {include}")
            pending.append(match)
    files.update((build / "compile_commands.json", evidence / "compiled-object-contract.json",
                  evidence / "linked-artifact-identities.json"))
    return host_root, sources, files


def prepare(generation: Path, host_build: Path, host_evidence: Path, output: Path,
            maximum_instructions: int):
    host_root, host_sources, files = verify_host(host_build, host_evidence)
    # Import the original, byte-identical audit verifier so its established
    # confined relative paths stay intact; do not rebind ROOT or rewrite audits.
    for relative in ("scripts/segarally_recompile.py", "platform/segarally/stage_data.py"):
        require(identity(host_root / relative) == identity(ROOT / relative), "unsupported audit verifier version")
    sys.path.insert(0, str(host_root / "scripts"))
    audit_module = module(host_root / "scripts/segarally_recompile.py", "recording_generation_audit")
    coverage = read_json(generation / "coverage.json")
    boundary = audit_module.verify_boundary_contract(generation, coverage, verify_objects=True)
    require(boundary is not None, "audited exact boundary generation required")
    reach = coverage["reachability"]
    addresses = [int(pc, 16) for pc in reach["instruction_addresses"]]
    require(addresses == sorted(set(addresses)) and len(addresses) == reach["instruction_count"], "invalid generated instruction inventory")
    require(0 < len(addresses) <= 1000000, "instruction inventory exceeds bound")
    relocations = reach["relocated_code"]
    require(len(relocations) <= 16 and all(r.get("runtime_word_guard") is True for r in relocations), "unverified reset-copy profile")
    assets_dir = host_build / "assets"
    assets = read_json(assets_dir / "assets.json")
    require(assets.get("romset") == "srallycb", "unsupported assets ROM set")
    stage = module(host_root / "platform/segarally/stage_data.py", "recording_stage_audit")
    audits = [coverage, read_json(generation / "tgp/audit.json"), read_json(generation / "video/audit.json")]
    require(assets["provenance"] == stage.staging_provenance(generation, audits, boundary), "actual staged generation provenance differs")
    for name, entry in assets["images"].items():
        require(entry["file"] == name + ".bin", "noncanonical staged image name")
        path = confined(assets_dir / entry["file"], assets_dir)
        require(identity(path) == {"bytes": entry["size"], "sha256": entry["sha256"]}, "actual staged image changed")
        files.add(path)
    for audit, source_name, staged_name in ((coverage,"program","program"),(coverage,"main_data","main_data"),
            (audits[1],"copro_tgp_tables","tgp_tables"),(audits[1],"copro_data","copro_data"),
            (audits[2],"polygons","polygons"),(audits[2],"textures","textures")):
        entry = audit["images"][source_name]
        path = confined(host_root / entry["path"], generation)
        actual = identity(path)
        require(actual["sha256"] == entry["sha256"] and actual == identity(assets_dir / (staged_name + ".bin")), "loaded image differs from audited import")
        files.add(path)
    generated_objects = sorted((generation / "objects").glob("*.o"))
    for name, digest in boundary["generated_sha256"].items():
        source = generation / "gen" / name
        require(source in host_sources and identity(source)["sha256"] == digest, "host did not compile the audited main inventory")
        files.add(source)
    tgp = generation / "tgp/program.cpp"
    require(tgp in host_sources, "host TGP object missing")
    crypto = []
    sha_sources = []
    for suffix in ("/src/runtime/sha256_digest.cpp", "/C/Sha256.c", "/C/Sha256Opt.c", "/C/CpuArch.c"):
        matches = [obj for source, obj in host_sources.items() if source.as_posix().endswith(suffix) and "segarally_recording.dir" in str(obj)]
        require(len(matches) == 1, "ambiguous verified SHA object")
        crypto.extend(matches)
        if suffix == "/C/Sha256.c":
            sha_sources = [source for source, obj in host_sources.items() if obj in matches]
    sha_dir = sha_sources[0].parent
    revision = subprocess.run(["git","-C",str(sha_dir.parent),"rev-parse","HEAD"],capture_output=True,text=True,check=True).stdout.strip()
    require(revision == "0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17", "unsupported SHA dependency revision")
    subprocess.run(["git","-C",str(sha_dir.parent),"diff","--quiet","HEAD","--","C"],check=True)
    files.update(sha_dir.glob("*.h"))
    objects = generated_objects + [host_sources[tgp]] + crypto
    files.update(objects)
    files.update(generation / name for name in ("coverage.json", "reachability.json", "tgp/audit.json", "video/audit.json"))
    files.update((assets_dir / "assets.json", Path(__file__).resolve()))
    exe = host_build / "segarally_native"
    header = output / "recording_reference_contract.h"
    host_digest, assets_digest = identity(exe)["sha256"], identity(assets_dir / "assets.json")["sha256"]
    lines = ["// Actual verified build inputs; ignored test artifact.", "#pragma once", "namespace srally::recording_contract {",
             f"inline constexpr std::array<uint32_t,{len(addresses)}> addresses{{{{"]
    lines.extend(",".join(f"0x{pc:08x}u" for pc in addresses[n:n+12]) + "," for n in range(0,len(addresses),12))
    lines += ["}};", f"inline constexpr std::array<recording_reference::Relocation,{len(relocations)}> relocations{{{{"]
    lines.extend("{" + ",".join(str(r[k]) + "u" for k in ("guest","program_offset","size")) + "}," for r in relocations)
    lines += ["}};", "inline recording_reference::BuildBinding binding() { return {",
              f'"{host_digest}","{assets_digest}",addresses,relocations,{maximum_instructions}ull}}; }}', "}"]
    header.write_text("\n".join(lines) + "\n")
    sources = [ROOT / "tests/test_segarally_boot_parity.cpp", ROOT / "tools/srally_boot/recording_bundle.cpp",
               ROOT / "src/refcore/i960_ref.cpp", ROOT / "src/i960/decode.cpp", ROOT / "src/i960/isa.cpp"]
    sources.extend(ROOT / "src/runtime" / (name + ".cpp") for name in RUNTIME)
    files.update(sources);files.add(header)
    # Include every quoted header/inc actually available to this consumer,
    # including the new parser/stepper/profile code, in the frozen build input.
    import re
    pending = list(sources); visited = set()
    while pending:
        path = pending.pop().resolve()
        if path in visited:continue
        visited.add(path);files.add(path)
        for include in re.findall(r'^\s*#include\s+"([^"\n]+)"',path.read_text(),re.MULTILINE):
            candidates=(path.parent/include,ROOT/"src"/include,ROOT/include)
            match=next((p for p in candidates if p.is_file()),None)
            require(match is not None,"unresolved consumer header: "+include);pending.append(match)
    command = shlex.split(os.environ.get("CXX", "c++")) + ["-std=c++20","-O1","-fno-fast-math","-ffp-contract=off",
        "-I"+str(ROOT),"-I"+str(ROOT/"src"),"-I"+str(output),"-isystem",str(sha_dir),"-DM2_SRALLY_RECORDING_REFERENCE",
        '-DM2_SRALLY_RECORDING_CONTRACT_HEADER="recording_reference_contract.h"']
    command.extend("-DM2_SRALLY_" + flag for flag in ("IO","COMM","CONTROL","VIDEO","SCREEN","UART","TGP"))
    command += audit_module.boundary_compile_flags(command) + list(map(str,sources+objects)) + ["-o",str(output/"recording_reference")]
    (output / "build-command.json").write_text(json.dumps(command,indent=2)+"\n")
    snapshot = {str(path): identity(path) for path in sorted(files)}
    with (output / "build.log").open("w") as log:
        subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    require(snapshot == {str(path): identity(path) for path in sorted(files)}, "inputs changed during test build")
    # Revalidate against the original assertions too: a newly observed hash
    # cannot replace a measured object/audit expectation during compilation.
    verify_host(host_build,host_evidence)
    audit_module.verify_boundary_contract(generation,coverage,verify_objects=True)
    require(read_json(assets_dir/"assets.json")==assets,"staged report changed during build")
    for name,entry in assets["images"].items():
        require(identity(assets_dir/entry["file"])=={"bytes":entry["size"],"sha256":entry["sha256"]},"staged image changed during build")
    snapshot[str(output/"recording_reference")]=identity(output/"recording_reference")
    result = {"scope":"independent_i960_shared_boards_sound_off_only", "maximum_instructions":maximum_instructions,
              "executable":identity(output/"recording_reference"), "actual_capture_host":identity(exe),
              "actual_assets_report":identity(assets_dir/"assets.json"),"instruction_count":len(addresses),
              "measured_inputs":snapshot,"generation_provenance_limits":"existing source/object measurements; TGP source identity_only; no hardware attestation"}
    (output / "prepared.json").write_text(json.dumps(result,indent=2)+"\n")
    return exe, assets_dir, snapshot


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("generation","host-build","host-evidence","output"):
        parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--bundle",type=Path)
    parser.add_argument("--run",action="store_true",help="Execute the preflighted sound-off bundle after build")
    parser.add_argument("--max-instructions",type=int,default=20000000)
    parser.add_argument("--timeout",type=int,default=90)
    args=parser.parse_args()
    require(1<=args.max_instructions<=20000000 and 1<=args.timeout<=180,"first checkpoint bound is 1..20M instructions and <=180 seconds")
    require(not args.run or args.bundle is not None,"--run requires --bundle")
    common=Path(subprocess.run(["git","rev-parse","--path-format=absolute","--git-common-dir"],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()).parent
    for name in ("generation","host_build","host_evidence","output","bundle"):
        value=getattr(args,name)
        if value is not None:setattr(args,name,confined(value,common/"build"))
    require(args.output.is_relative_to(ROOT/"build"),"output must be under this checkout's ignored build/")
    args.output.mkdir(parents=True,exist_ok=False)
    exe,data,snapshot=prepare(args.generation,args.host_build,args.host_evidence,args.output,args.max_instructions)
    if args.run:
        command=[str(args.output/"recording_reference"),"--recording",str(args.bundle),str(data),str(exe)]
        (args.output/"run-command.json").write_text(json.dumps(command,indent=2)+"\n")
        with (args.output/"run.log").open("w") as log:
            subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=args.timeout)
        require(snapshot=={path:identity(Path(path)) for path in snapshot},"actual inputs changed during replay")
        log=(args.output/"run.log").read_text()
        require("PASS independent sound-off recording records=" in log,"consumer did not report complete independent comparison")
        print(log,end="")
    else:print(f"Prepared only; no guest instruction executed: {args.output}")


if __name__=="__main__":
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError) as error:
        message = f"process returned {error.returncode}; see output build.log/run.log" if isinstance(error,subprocess.CalledProcessError) else str(error)
        print(f"recording reference: {message}",file=sys.stderr);sys.exit(1)
