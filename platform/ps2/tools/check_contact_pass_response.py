#!/usr/bin/env python3
"""Compare a composed native response pass against private original boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
from check_contact_neighborhood import ROOT, PROGRAM_SHA256, digest, private_output
from check_contact_result import result_rows, ordinary
from check_driving_contact import block, integer, saved_return
from check_recovered_scalar_metric import FIRMWARE_SHA256, verify_firmware

NATIVE = ["contact_pass_response", "contact_response", "contact_normalization", "rounded_cross_product",
          "recovered_scalar_metric", "contact_aggregate"]
SOURCES = [f"platform/ps2/native/{stem}.{ext}" for stem in NATIVE for ext in ("cpp", "h")] + [
    "platform/ps2/native/contact_result.h", "platform/ps2/native/contact_search.h",
    "platform/ps2/tests/contact_pass_response_cases.h", "platform/ps2/tests/contact_pass_response_oracle.cpp",
    "platform/ps2/tools/check_contact_pass_response.py", "platform/ps2/tools/check_contact_result.py",
    "platform/ps2/tools/check_contact_query.py", "platform/ps2/tools/check_contact_count.py",
    "platform/ps2/tools/check_contact_neighborhood.py", "platform/ps2/tools/check_driving_contact.py",
    "platform/ps2/tools/check_recovered_scalar_metric.py", "tools/m2tgprecomp/main.cpp",
    "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/cpu.h", "src/runtime/cpu.cpp",
    "src/runtime/fast_paths.h", "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]


def pass_rows(events, marker):
    result_rows(events, marker, "main-contact")
    entry = prepared = None
    contacts, rows, provenance = [], [], []
    for event in events:
        kind = event["event"]
        if integer(event.get("ac"), 32) >> 30:
            raise ValueError("Response-pass capture requires nearest-even rounding")
        if kind == "main_entry":
            if not all(ordinary(address, 12) for address in event["registers"][17:19]):
                raise ValueError("Response-pass candidates require ordinary aligned RAM")
            entry, prepared, contacts = event, None, []
        elif kind == "main_prequery":
            if prepared is not None or event.get("return_frame") != entry["return_frame"] or saved_return(event) != saved_return(entry):
                raise ValueError("Repeated response pass preparation or changed saved caller")
            prepared = event
        elif kind == "contact_post":
            if prepared is None or integer(event.get("contact_index"), 32) != len(contacts):
                raise ValueError("Response pass contacts lack complete ordered preparation")
            index = len(contacts)
            contacts.append(block(event, "contact_output", 0x2138d0+36*index, 8)[3:8])
        elif kind == "main_return":
            if entry is None or prepared is None or len(contacts) != 4:
                raise ValueError("Response pass parent is incomplete")
            fp = entry["main_frame_pointer"]
            offsets = block(prepared, "transformed_offsets", fp+0x70, 12)
            old_words, new_words = entry["object_words"], event["object_words"]
            if any(old_words[i] != new_words[i] for i in range(256) if i not in (11, 13, 18)):
                raise ValueError("Response pass modified unrelated object words")
            position, orientation = entry["registers"][17:19]
            old_position = block(entry, "candidate_position", position, 3)
            old_orientation = block(entry, "candidate_orientation", orientation, 3)
            new_position = block(event, "candidate_position", position, 3)
            new_orientation = block(event, "candidate_orientation", orientation, 3)
            if old_position[1] != new_position[1] or old_orientation[::2] != new_orientation[::2]:
                raise ValueError("Response pass modified preserved candidate components")
            enable = block(entry, "feedback_enable", 0x2142c8, 1)
            if block(event, "feedback_enable", 0x2142c8, 1) != enable:
                raise ValueError("Response pass modified feedback gate")
            old_feedback = block(entry, "feedback", 0x213960, 3)
            new_feedback = block(event, "feedback", 0x213960, 3)
            if enable[0] and old_feedback[1:] != new_feedback[1:]:
                raise ValueError("Response pass modified disabled feedback")
            inputs = sum(contacts, []) + offsets + [old_words[i] for i in (20, 21, 22, 5, 7, 15)] + [int(enable[0] == 0)]
            state = [old_words[i] for i in (11, 13, 18)] + old_position + old_orientation + old_feedback[1:] + [old_feedback[0], 0]
            result = [new_words[i] for i in (11, 13, 18)] + new_position + new_orientation + new_feedback[1:] + [new_feedback[0], event["registers"][16]]
            rows.append(tuple(inputs+state+result))
            provenance.append({"call_id": event["call_id"], "entry_frame": entry["frame"], "exit_frame": event["frame"],
                               "selected_scalar": result[-1], "contact_mask": result[-2]})
            entry = prepared = None
    if entry is not None or not rows or len(rows) != integer(marker.get("main_samples"), 64):
        raise ValueError("Incomplete or empty response-pass capture")
    return rows, provenance


def read_capture(directory):
    directory = private_output(Path(directory))
    paths = [directory/"capture.json", directory/"main-contact.jsonl", directory.parent/"validation.json"]
    for path, bound in zip(paths, (16384, 64*1024*1024, 1024*1024)):
        if path.stat().st_size > bound:
            raise ValueError("Response-pass capture exceeds bounded size")
    hashes = {str(path): digest(path) for path in paths}
    marker, validation = json.loads(paths[0].read_text()), json.loads(paths[2].read_text())
    if marker.get("schema") != 1 or marker.get("complete") is not True or validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True:
        raise ValueError("Response-pass capture lacks complete observer parity")
    for path in paths[:2]:
        if validation.get("capture_files_sha256", {}).get(path.name) != hashes[str(path)]:
            raise ValueError("Response-pass capture identity differs")
    rows, provenance = pass_rows([json.loads(line) for line in paths[1].read_text().splitlines()], marker)
    return rows, provenance, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--tgp-program", required=True, type=Path)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    program, firmware = args.program.resolve(), args.tgp_program.resolve()
    try:
        if program.stat().st_size != 2097152 or digest(program) != PROGRAM_SHA256:
            raise ValueError("Program differs from verified Revision B identity")
        verify_firmware(firmware.read_bytes())
        output = private_output(args.output)
        rows, provenance, capture_hashes = read_capture(args.capture) if args.capture else ([], [], {})
    except (ValueError, OSError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    initial = {name: digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    generator, generated, executable = (output/name for name in ("m2tgprecomp", "private-tgp.cpp", "response-pass-oracle"))
    common = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off", "-I", str(ROOT/"src")]
    commands = [common+[str(ROOT/"tools/m2tgprecomp/main.cpp"), "-o", str(generator)], [str(generator), str(firmware), str(generated)],
        common+["-DM2TGP_WITH_HOOK", "-I", str(ROOT/"platform/ps2/native"), str(ROOT/"platform/ps2/tests/contact_pass_response_oracle.cpp"),
            *(str(ROOT/f"platform/ps2/native/{name}.cpp") for name in NATIVE), str(ROOT/"src/runtime/tgp.cpp"),
            str(ROOT/"src/runtime/cpu.cpp"), str(ROOT/"src/refcore/i960_ref.cpp"), str(generated), "-o", str(executable)]]
    report = {"schema": 1, "status": "failed", "scope": "Post-query response composition; original query/adapter calls excluded",
        "sources_sha256": initial, "program_sha256": PROGRAM_SHA256, "firmware_sha256": FIRMWARE_SHA256,
        "commands": commands, "capture_sha256": capture_hashes, "live_passes": len(rows), "live_pairs": provenance,
        "native_gameplay_connected": False, "physical_console_verified": False}
    try:
        for i, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, text=True, timeout=120)
            (output/f"build-{i}.log").write_text(result.stdout+result.stderr)
            result.check_returncode()
        run = [str(executable), str(program), str(firmware)]
        if rows:
            records = output/"private-passes.bin"
            records.write_bytes(b"RXPASS1\0"+struct.pack("<I", len(rows))+b"".join(struct.pack("<65I", *row) for row in rows))
            report["records_sha256"] = digest(records)
            run.append(str(records))
        result = subprocess.run(run, capture_output=True, text=True, timeout=60)
        (output/"oracle.log").write_text(result.stdout+result.stderr)
        report.update(result=result.stdout.strip(), exit_code=result.returncode, executable_sha256=digest(executable))
        result.check_returncode()
        if "CONTACT_PASS_RESPONSE_ORACLE PASS" not in result.stdout or f" live={len(rows)} " not in result.stdout:
            raise RuntimeError("Response-pass comparison lacks expected completion")
        if initial != {name: digest(ROOT/name) for name in SOURCES} or capture_hashes != {name: digest(Path(name)) for name in capture_hashes} or \
           digest(program) != PROGRAM_SHA256 or digest(firmware) != FIRMWARE_SHA256:
            raise RuntimeError("Response-pass comparison inputs changed")
        report["status"] = "passed"
        print(result.stdout, end="")
    finally:
        (output/"result.json").write_text(json.dumps(report, indent=2)+"\n")


if __name__ == "__main__":
    main()
