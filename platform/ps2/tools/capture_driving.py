#!/usr/bin/env python3
"""Capture actual bounded race updates through the separate host reference.

All generated code, snapshots and images remain in ignored build/. This does
not port an integrator or certify the reference's floating-point semantics.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PROGRAM_SHA256 = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-build", type=Path, default=ROOT / "build/ps2/native")
    parser.add_argument("--pack", type=Path, default=ROOT / "build/ps2/assets/game.rpk")
    parser.add_argument("--inputs", type=Path, default=ROOT / "config/segarally_lancia_drive.inputs")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=2850)
    parser.add_argument("--first-frame", type=int, default=2200)
    parser.add_argument("--last-frame", type=int, default=2800)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--full-step", action="store_true", help="trace consecutive whole player callbacks and actual CPU data accesses")
    args = parser.parse_args()
    output, native, pack, inputs = (value.resolve() for value in
                                  (args.output, args.native_build, args.pack, args.inputs))
    build = ROOT / "build"
    if build.is_symlink() or output == build or not output.is_relative_to(build) or output.exists():
        parser.error("output must be a new directory beneath this checkout's real build/")
    if not 0 <= args.first_frame <= args.last_frame < args.frames <= 10000 or args.last_frame-args.first_frame > 1200:
        parser.error("invalid bounded frame interval")
    if not 1 <= args.jobs <= 8 or not inputs.is_file() or inputs.stat().st_size > 1048576:
        parser.error("invalid jobs or bounded input replay")
    prepare = module("platform/ps2/prepare_core.py", "drive_prepare")
    generated, provenance = prepare.validate_native_build(native)
    packing = module("platform/ps2/tools/asset_pack.py", "drive_pack")
    reader = packing.PackReader(pack)
    try:
        reader.verify()
        if reader.regions["program"]["sha256"] != PROGRAM_SHA256:
            raise ValueError("capture requires the verified Revision B program")
        mapping = {"program": ("coverage.json", "program"), "main_data": ("coverage.json", "main_data"),
                   "tgp_tables": ("tgp/audit.json", "copro_tgp_tables"),
                   "copro_data": ("tgp/audit.json", "copro_data"),
                   "polygons": ("video/audit.json", "polygons"), "textures": ("video/audit.json", "textures")}
        for name, (audit, key) in mapping.items():
            expected = json.loads((native / audit).read_text())["images"][key]["sha256"]
            if reader.regions[name]["sha256"] != expected:
                raise ValueError("pack differs from audited native generation: " + name)
        pack_regions = reader.regions
    finally:
        reader.close()
    helpers = module("platform/ps2/build_ps2.py", "drive_build_helpers")
    sources = [ROOT / "platform/ps2/tools/driving_capture_main.cpp", ROOT / "platform/ps2/tools/driving_capture.cpp",
               ROOT / "platform/ps2/tools/full_step_capture.cpp",
               *generated, *[ROOT / "src/runtime" / (name + ".cpp") for name in prepare.RUNTIME_SOURCES]]
    headers = helpers.project_headers(sources, [ROOT / "src", ROOT])
    source_inputs = sorted(set(sources + headers + [Path(__file__).resolve(), ROOT / "platform/ps2/prepare_core.py",
                    ROOT / "platform/ps2/build_ps2.py", ROOT / "platform/ps2/tools/asset_pack.py",
                    ROOT / "platform/segarally/stage_data.py"]))
    initial = {str(path.relative_to(ROOT)): digest(path) for path in source_inputs}
    identities = {"pack": {"path":str(pack.relative_to(ROOT)),"sha256":digest(pack)},
                  "inputs": {"path":str(inputs.relative_to(ROOT)),"sha256":digest(inputs)}}
    output.mkdir(parents=True)
    cmake = ["cmake_minimum_required(VERSION 3.20)", "project(driving_reference LANGUAGES CXX)",
             "set(CMAKE_CXX_STANDARD 20)", "add_executable(driving-reference"]
    cmake += [json.dumps(str(source)) for source in sources]
    cmake += [")", "target_include_directories(driving-reference PRIVATE " + json.dumps(str(ROOT / "src")) + " " + json.dumps(str(ROOT)) + ")",
              "target_compile_definitions(driving-reference PRIVATE " + " ".join(d for d in prepare.DEFINITIONS if d != "RALLY_PS2_EE=1") + ")",
              "target_compile_options(driving-reference PRIVATE -O2 -fno-fast-math -ffp-contract=off)",
              "set_source_files_properties(" + json.dumps(str(ROOT / "src/runtime/model2a_machine.cpp")) +
              " PROPERTIES COMPILE_DEFINITIONS RALLY_OFFLINE_DRIVING_CAPTURE=1)"]
    (output / "CMakeLists.txt").write_text("\n".join(cmake)+"\n")
    report = {"schema":1,"status":"incomplete","program_sha256":PROGRAM_SHA256,
              "inputs":identities,"generation":provenance,"pack_regions":pack_regions,
              "sources_sha256":initial,"frames":args.frames,"first_frame":args.first_frame,"last_frame":args.last_frame,
              "scope":"Original game executes tracked cabinet replay; bounded observer capture, not native driving",
              "full_step_requested":args.full_step,
              "physical_ps2_verified":False,"arcade_arithmetic_fidelity_certified":False}
    commands = [["cmake", "-S", str(output), "-B", str(output / "host"), "-DCMAKE_BUILD_TYPE=Release"],
                ["cmake", "--build", str(output / "host"), "-j", str(args.jobs)]]
    started = time.monotonic()
    try:
        for index, command in enumerate(commands):
            with (output / f"build-{index}.log").open("w") as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
        executable = output / "host/driving-reference"
        report["executable_sha256"] = digest(executable)
        report["build_seconds"] = round(time.monotonic()-started,3)
        print("Host reference compiled; starting actual input replay",flush=True)
        for name, directory in (("captured",str(output / "capture")),("baseline","-")):
            command = [str(executable),str(pack),str(inputs),str(args.frames),directory,
                       str(args.first_frame),str(args.last_frame),"1" if name=="captured" else "0",str(int(args.full_step))]
            run_started=time.monotonic()
            with (output / (name+".csv")).open("w") as stdout, (output / (name+".log")).open("w") as stderr:
                result=subprocess.run(command,stdout=stdout,stderr=stderr,timeout=900)
            report[name+"_seconds"]=round(time.monotonic()-run_started,3)
            report[name+"_returncode"]=result.returncode
            print(name+" run complete",flush=True)
        helpers.verify_source_snapshot(initial)
        if digest(executable)!=report["executable_sha256"]:
            raise ValueError("reference executable changed during capture")
        if digest(pack)!=identities["pack"]["sha256"] or digest(inputs)!=identities["inputs"]["sha256"]:
            raise ValueError("run input changed during capture")
        capture = output / "capture"
        captured_lines=(output / "captured.csv").read_bytes().splitlines()
        baseline_lines=(output / "baseline.csv").read_bytes().splitlines()
        prefix=0
        for captured,baseline in zip(captured_lines,baseline_lines):
            if captured!=baseline: break
            prefix+=1
        report.update(exact_checkpoint_match=False,matched_prefix_frames=max(0,prefix-1),
                      captured_checkpoint_frames=max(0,len(captured_lines)-1),
                      baseline_checkpoint_frames=max(0,len(baseline_lines)-1),
                      baseline_sha256=digest(output / "baseline.csv"),captured_sha256=digest(output / "captured.csv"),
                      capture_files_sha256={path.name:digest(path) for path in sorted(capture.iterdir()) if path.is_file()})
        if report["captured_returncode"] or report["baseline_returncode"]:
            raise ValueError("reference process reported incomplete capture or execution; inspect run logs and observation counts")
        if (output / "captured.csv").read_bytes() != (output / "baseline.csv").read_bytes():
            raise ValueError("capture changed one or more exact frame checkpoints")
        if len(baseline_lines) != args.frames+1:
            raise ValueError("incomplete per-frame checkpoints")
        marker=json.loads((capture / "capture.json").read_text())
        if marker.get("complete") is not True or not marker.get("completed_calls"):
            raise ValueError("capture has no complete original-game caller evidence")
        if not marker.get("main_completions") or not marker.get("main_samples"):
            raise ValueError("capture has no complete sampled main contact pass")
        if not marker.get("vehicle_completions") or not marker.get("vehicle_samples"):
            raise ValueError("capture has no complete sampled main vehicle callback")
        for kind in ("segment","tail"):
            if any(marker.get("motion_"+kind+suffix)!=marker["vehicle_completions"]
                   for suffix in ("_entries","_completions")) or \
               marker.get("motion_"+kind+"_samples")!=marker["vehicle_samples"]:
                raise ValueError("capture lacks one complete sampled motion region per vehicle: "+kind)
        if marker.get("motion_events")!=4*marker["vehicle_samples"]:
            raise ValueError("capture has an incomplete additive motion stream")
        if args.full_step:
            whole=json.loads((capture / "full-step-summary.json").read_text())
            if whole.get("complete") is not True or whole.get("steps")!=marker["vehicle_completions"] or \
               whole.get("first_frame")!=args.first_frame or whole.get("last_frame")!=args.last_frame:
                raise ValueError("whole-step trace is not complete and consecutive")
            report["full_step_summary"]=whole
        report.update(status="passed",exact_checkpoint_match=True,matched_frames=args.frames,capture_summary=marker,
                      baseline_sha256=digest(output / "baseline.csv"),captured_sha256=digest(output / "captured.csv"),
                      capture_files_sha256={path.name:digest(path) for path in sorted(capture.iterdir()) if path.is_file()})
    except Exception as error:
        report.update(status="failed",error=str(error))
        raise
    finally:
        report["elapsed_seconds"]=round(time.monotonic()-started,3)
        (output / "validation.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print(json.dumps({key:report[key] for key in ("status","frames","elapsed_seconds","capture_summary")},indent=2))


if __name__=="__main__":
    main()
