#!/usr/bin/env python3
"""Compare two bounded main-helper input reductions with original execution."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output
from check_driving_contact import block
from main_vehicle_capture import SOURCES as VEHICLE_SOURCES, read_vehicle_capture, vehicle_envelopes

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
REGIONS = {"selection": (0x3789c, 0x378f4, "23e90e789234c677e4b34dc7b7e08c9d7f12357f918d7ccbc4c55dba49d3b531"),
           "balance": (0x37900, 0x379d0, "f634c8ea2f277ad29173f03c24b7afbdf77c1038be6bb54d5cc4d77e0a189a17")}
SOURCES = list(dict.fromkeys([
    "platform/ps2/native/paired_balance.h", "platform/ps2/native/paired_balance.cpp",
    "platform/ps2/native/vehicle_parameter_selection.h", "platform/ps2/native/vehicle_parameter_selection.cpp",
    "platform/ps2/tests/paired_balance_cases.h", "platform/ps2/tests/paired_balance_oracle.cpp",
    "platform/ps2/tools/check_paired_balance.py", *VEHICLE_SOURCES,
    "src/runtime/cpu.h", "src/runtime/cpu.cpp", "src/runtime/fast_paths.h",
    "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]))


def paired_rows(events, marker):
    """Return balance8-word rows, selection15-word rows and provenance.

    Parent/child return and chronology proofs are enforced by the common reader.
    These additional checks bind the isolated numerical inputs and preserved
    memory to their real helper fields; no full-helper numerical claim is made.
    """
    groups = vehicle_envelopes(events, marker)
    balances, selections, provenance = [], [], []
    for group in groups:
        records = {event["event"]: event for event in group}
        pre, post = records["balance_pre"], records["balance_post"]
        obj = pre["object"]
        values = block(pre, "balance_pairs", obj+0x64, 4)
        if any((word & 0x7fffffff) > 0x49800000 for word in values):
            raise ValueError("Paired-balance input exceeds the finite native domain")
        if values != pre["object_words"][25:29] or pre["object_words"] != post["object_words"]:
            raise ValueError("Paired-balance input differs from unchanged object fields")
        for name, address, count in (("balance_pairs", obj+0x64, 4),
                                     ("balance_prior", 0x21425c, 1),
                                     ("balance_vector", pre["registers"][3], 3),
                                     ("balance_output", pre["registers"][15], 3)):
            if block(pre, name, address, count) != block(post, name, address, count):
                raise ValueError("Paired-balance region changed ordinary input storage")
        if post["registers"][12] != block(pre, "balance_prior", 0x21425c, 1)[0]:
            raise ValueError("Paired-balance preserved prior read differs")
        output = [post["registers"][index] for index in (5, 7, 14, 13)]
        balances.append(tuple(values+output))

        selected_pre, selected_post = records["selection_pre"], records["selection_post"]
        flags = block(selected_pre, "selection_flags", 0x214208, 1)[0]
        selected = block(selected_post, "selected_parameters", 0x2141c0, 13)
        index = (flags & 0x3fe).bit_length()-1 if flags & 0x3fe else 0
        if block(selected_post, "selection_flags", 0x214208, 1) != [flags] or \
           selected_pre["object_words"] != selected_post["object_words"] or \
           selected_post["registers"][10] != index or selected_post["registers"][17] != 0x214254 or \
           selected_post["registers"][20] != selected[12] or selected_post["registers"][4:8] != selected[8:12]:
            raise ValueError("Parameter selection index, copy or preserved inputs differ")
        selections.append(tuple([flags, index]+selected))
        provenance.append({"call_id": pre["call_id"], "frame": pre["frame"],
                           "balance_instructions": post["instructions"]-pre["instructions"],
                           "selection_instructions": selected_post["instructions"]-selected_pre["instructions"],
                           "selection_index": index})
    return balances, selections, provenance


def read_capture(directory):
    events, marker, hashes = read_vehicle_capture(directory)
    balances, selections, provenance = paired_rows(events, marker)
    return balances, selections, provenance, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    if program.stat().st_size != 2097152 or digest(program) != PROGRAM_SHA:
        parser.error("program differs from verified private identity")
    data = program.read_bytes()
    for start, end, expected in REGIONS.values():
        if hashlib.sha256(data[start:end]).hexdigest() != expected:
            parser.error("verified instruction region differs")
    balances, selections, provenance, capture_hashes = read_capture(args.capture) if args.capture else ([], [], [], {})
    output = private_output(args.output)
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    hashes = {name: digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    executable = output/"paired-balance-oracle"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
               "-I", str(ROOT/"src"), "-I", str(ROOT/"platform/ps2/native"),
               *(str(ROOT/name) for name in ("platform/ps2/native/paired_balance.cpp",
                  "platform/ps2/native/vehicle_parameter_selection.cpp", "platform/ps2/tests/paired_balance_oracle.cpp",
                  "src/runtime/cpu.cpp", "src/refcore/i960_ref.cpp")), "-o", str(executable)]
    report = {"schema": 1, "status": "failed", "scope": "Bounded paired balance and raw parameter selection only",
              "program_sha256": PROGRAM_SHA, "regions": REGIONS, "sources_sha256": hashes,
              "capture_sha256": capture_hashes, "command": command, "live_pairs": len(balances),
              "live_selections": len(selections), "live_provenance": provenance,
              "full_helper_recovered": False, "native_gameplay_connected": False, "physical_console_verified": False}
    try:
        built = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output/"build.log").write_text(built.stdout+built.stderr)
        built.check_returncode()
        run = [str(executable), str(program)]
        if balances:
            pair_path, selection_path = output/"private-pairs.bin", output/"private-selections.bin"
            pair_path.write_bytes(b"PAIRBAL1"+struct.pack("<I", len(balances))+b"".join(struct.pack("<8I", *row) for row in balances))
            selection_path.write_bytes(b"VPARAMS1"+struct.pack("<I", len(selections))+b"".join(struct.pack("<15I", *row) for row in selections))
            report["records_sha256"] = {"pairs": digest(pair_path), "selections": digest(selection_path)}
            run.extend((str(pair_path), str(selection_path)))
        tested = subprocess.run(run, capture_output=True, text=True, timeout=90)
        (output/"comparison.log").write_text(tested.stdout+tested.stderr)
        report.update(result=tested.stdout.strip(), exit_code=tested.returncode, executable_sha256=digest(executable))
        tested.check_returncode()
        lines = tested.stdout.splitlines()
        if len(lines) != 2 or not lines[0].startswith("PAIRED_BALANCE_ORACLE PASS ") or \
           not lines[1].startswith("VEHICLE_PARAMETER_ORACLE PASS ") or any(f" live={len(balances)} " not in line for line in lines):
            raise RuntimeError("Missing comparison completion/count marker")
        if hashes != {name: digest(ROOT/name) for name in SOURCES} or \
           capture_hashes != {name: digest(Path(name)) for name in capture_hashes} or digest(program) != PROGRAM_SHA:
            raise RuntimeError("Comparison source, private image or capture changed during execution")
        report["status"] = "passed"
        print(tested.stdout, end="")
    finally:
        (output/"result.json").write_text(json.dumps(report, indent=2)+"\n")


if __name__ == "__main__":
    main()
