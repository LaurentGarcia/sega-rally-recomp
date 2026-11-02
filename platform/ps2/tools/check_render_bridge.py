#!/usr/bin/env python3
"""Bind native chase/body value operations to original code and sealed captures."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_contact_neighborhood import ROOT, digest, private_output

PROGRAM_SHA = "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"
FIRMWARE_SHA = "68bc45488a1c24a292c6b384bd3d015e6469ef2babf1037ab6194b951411c346"
NATIVE = ["driving_render_view", "car_render_bridge", "angle_fold",
          "main_balance_math", "vehicle_arithmetic", "rounded_affine_transform"]
SOURCES = [f"platform/ps2/native/{name}.{ext}" for name in NATIVE for ext in ("h", "cpp")] + [
    "platform/ps2/native/car_pose.h", "platform/ps2/tests/driving_render_view_oracle.cpp",
    "platform/ps2/tests/car_render_bridge_oracle.cpp", "platform/ps2/tools/check_render_bridge.py",
    "platform/ps2/tools/check_contact_neighborhood.py", "platform/ps2/tools/visual_camera_capture.h",
    "platform/ps2/tools/visual_camera_capture.cpp", "tools/m2tgprecomp/main.cpp",
    "src/runtime/tgp.h", "src/runtime/tgp.cpp", "src/runtime/cpu.h", "src/runtime/cpu.cpp",
    "src/runtime/fast_paths.h", "src/refcore/i960_ref.h", "src/refcore/i960_ref.cpp"]


def uint(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise ValueError("Invalid raw word")
    return value


def block(record, name, address, count):
    matches = [b for b in record["blocks"] if b["name"] == name]
    if len(matches) != 1 or matches[0]["address"] != address or len(matches[0]["words"]) != count:
        raise ValueError("Block extent/identity differs: " + name)
    return [uint(v) for v in matches[0]["words"]]


def camera_groups(events, marker):
    """Validate the observer's exact call/return identity, without replaying it."""
    if marker.get("schema") != 1 or marker.get("complete") is not True or \
       not 0 < marker.get("calls", 0) <= 1024 or marker["calls"] != marker.get("completions") or \
       marker.get("events") != len(events):
        raise ValueError("Incomplete camera capture")
    groups, stack, last = [], [], -1
    for event in events:
        regs = event["registers"]
        if event.get("schema") != 1 or len(regs) != 32 or any(uint(v) != v for v in regs) or \
           not marker["first_frame"] <= event["frame"] <= marker["last_frame"] or \
           event["instructions"] <= last or uint(event["ac"]) >> 30:
            raise ValueError("Camera event sequence/rounding differs")
        last = event["instructions"]
        proof = event["return_frame"]
        if len(proof["words"]) != 16 or any(uint(v) != v for v in proof["words"]) or \
           proof["pc"] != proof["words"][2] or proof["pc"] & 3 or proof["frame_pointer"] & 63 or \
           not ((proof["source"] == "ordinary_ram" and proof["cache_index"] == -1) or
                (proof["source"] == "register_cache" and 0 <= proof["cache_index"] < 4)):
            raise ValueError("Camera saved return proof differs")
        kind = event["event"]
        if kind == "entry":
            if event["call_id"] != len(groups) + len(stack) + 1 or \
               event["pc"] != 0x59f000 + event["entry_pc"] or \
               regs[31] != event["frame_pointer"] or (regs[0] & ~63) != proof["frame_pointer"]:
                raise ValueError("Camera entry identity differs")
            stack.append([event])
        elif kind in ("view_matrix", "return"):
            if not stack:
                raise ValueError("Unpaired camera event")
            pre = stack[-1][0]
            if any(event[key] != pre[key] for key in ("call_id", "entry_pc", "frame", "frame_pointer", "return_frame")):
                raise ValueError("Camera call identity changed")
            if kind == "view_matrix":
                if pre["entry_pc"] != 0x1ff60 or len(stack[-1]) != 1 or \
                   event["pc"] != 0x5bf1f0 or regs[31] != pre["frame_pointer"]:
                    raise ValueError("Camera matrix checkpoint differs")
            else:
                if event["pc"] != proof["pc"] or regs[31] != proof["frame_pointer"] or \
                   (regs[0] & ~63) != (proof["words"][0] & ~63):
                    raise ValueError("Camera restored caller differs")
            stack[-1].append(event)
            if kind == "return":
                groups.append(stack.pop())
        else:
            raise ValueError("Unknown camera event")
    if stack or len(groups) != marker["calls"]:
        raise ValueError("Missing camera completion")
    return groups


def capture_rows(directory):
    directory = Path(directory).resolve()
    validation_path = directory.parent / "validation.json"
    validation = json.loads(validation_path.read_text())
    if validation.get("status") != "passed" or validation.get("exact_checkpoint_match") is not True or \
       validation.get("matched_frames") != 2850:
        raise ValueError("Visual capture lacks complete original-machine parity")
    hashes = {str(validation_path): digest(validation_path)}

    def bound(relative):
        path = directory / relative
        actual = digest(path)
        if validation["capture_files_sha256"].get(relative) != actual:
            raise ValueError("Visual capture file differs: " + relative)
        hashes[str(path)] = actual
        return path

    marker = json.loads(bound("camera/capture.json").read_text())
    if marker != validation["camera"]:
        raise ValueError("Camera marker is not bound by parity report")
    events = [json.loads(line) for line in bound("camera/camera.jsonl").read_text().splitlines()]
    groups = camera_groups(events, marker)
    views, bodies, provenance, by_frame = [], [], [], {}
    for group in groups:
        pre, post = group[0], group[-1]
        if pre["entry_pc"] != 0x1ff60:
            continue
        if len(group) != 3:
            raise ValueError("Chase call lacks original matrix observation")
        middle = group[1]
        pose = block(pre, "main_pose", 0x213b40, 22)
        if any(block(e, "main_pose", 0x213b40, 22) != pose for e in group[1:]):
            raise ValueError("Chase camera modified published pose")
        parameters = block(pre, "view_parameters", 0x20ac70, 4)
        if any(block(e, "view_parameters", 0x20ac70, 4) != parameters for e in group[1:]):
            raise ValueError("Camera configuration changed during call")
        matrix = block(middle, "camera_matrix", pre["frame_pointer"] + 0x60, 12)
        old = block(pre, "view_dispatch", 0x20ab4c, 5)[4]
        updated = block(post, "view_dispatch", 0x20ab4c, 5)[4]
        views.append(pose + [old, parameters[1]] + matrix + block(post, "view", 0x202200, 6) +
                     [updated] + block(post, "focus", 0x202270, 2))
        if pre["frame"] in by_frame:
            raise ValueError("Multiple chase updates per captured frame")
        by_frame[pre["frame"]] = (pose, matrix, updated)
        if pre["frame"] - 1 in by_frame and by_frame[pre["frame"] - 1][2] != old:
            raise ValueError("Camera history was reseeded between captured updates")
    for group in groups:
        pre, post = group[0], group[-1]
        if pre["entry_pc"] != 0x23a80:
            continue
        regs = pre["registers"]
        if regs[17] != 0x213b40 or pre["frame"] - 1 not in by_frame:
            continue  # This checker scopes the main car, not all draw roles.
        pose = block(pre, "draw_pose", regs[17], 22)
        if block(post, "draw_pose", regs[17], 22) != pose:
            raise ValueError("Body draw changed published pose")
        descriptor = block(pre, "draw_descriptor", regs[16], 12)
        if block(post, "draw_descriptor", regs[16], 12) != descriptor:
            raise ValueError("Body descriptor changed")
        offsets = block(pre, "wheel_offsets", descriptor[0], 12)
        if block(post, "wheel_offsets", descriptor[0], 12) != offsets:
            raise ValueError("Body attachment data changed")
        prior_pose, matrix, _ = by_frame[pre["frame"] - 1]
        if pose != prior_pose:
            raise ValueError("Draw pose is not the preceding camera publication")
        objects = [json.loads(line) for line in bound(f"frame-{pre['frame']}/objects.jsonl").read_text().splitlines()]
        # Exact observed descriptor/model identity. Do not guess a model from
        # proximity, geometry bounds or a model name.
        expected = [0x80848b, 0x801a8b, 0x8082be, *([0x80835b] * 4)]
        starts = [i for i in range(len(objects) - 6) if [x["object_address"] for x in objects[i:i+7]] == expected]
        if len(starts) != 1 or descriptor[1:6] != [104, 102, 96, 106, 103]:
            raise ValueError("Main-car object binding is absent or ambiguous")
        start = starts[0]
        selected = objects[start:start+7]
        if any(o["frame"] != pre["frame"] or len(o["matrix_bits"]) != 12 for o in selected) or \
           selected[2]["matrix_bits"] != selected[1]["matrix_bits"]:
            raise ValueError("Body/extra object transform differs")
        bodies.append(pose + offsets + matrix + sum((selected[i]["matrix_bits"] for i in (0,1,3,4,5,6)), []))
        provenance.append({"camera_frame": pre["frame"] - 1, "draw_frame": pre["frame"],
                           "call_id": pre["call_id"], "shadow": start, "body": [start+1,start+2],
                           "wheels": list(range(start+3,start+7))})
    if len(views) < 2 or len(bodies) < 2:
        raise ValueError("At least two actual camera and body publications are required")
    return views, bodies, provenance, hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--tgp-program", type=Path, required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        for path, size, expected in ((args.program,2097152,PROGRAM_SHA),(args.tgp_program,6660,FIRMWARE_SHA)):
            if path.stat().st_size != size or digest(path) != expected:
                raise ValueError("Original image differs: " + str(path))
        output = private_output(args.output)
        views, bodies, provenance, capture_hashes = capture_rows(args.capture)
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.error(str(error))
    compiler = shutil.which("clang++") or shutil.which("c++")
    if not compiler:
        parser.error("C++20 compiler required")
    sources = {name: digest(ROOT/name) for name in SOURCES}
    output.mkdir(parents=True, exist_ok=False)
    report = {"schema":1,"status":"failed","sources_sha256":sources,"capture_sha256":capture_hashes,
              "program_sha256":PROGRAM_SHA,"firmware_sha256":FIRMWARE_SHA,
              "live_camera_calls":len(views),"live_body_calls":len(bodies),"bindings":provenance,
              "scope":"Recovered chase view and body/attachment geometry only; no camera-mode selection, dynamic lighting or full-stage loader claim",
              "physical_console_verified":False}
    common = [compiler,"-std=c++20","-O2","-fno-fast-math","-ffp-contract=off","-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/native")]
    generated, generator = output/"private-tgp.cpp", output/"m2tgprecomp"
    commands = [common+[str(ROOT/"tools/m2tgprecomp/main.cpp"),"-o",str(generator)],
                [str(generator),str(args.tgp_program.resolve()),str(generated)]]
    for name in ("driving_render_view","car_render_bridge"):
        cpp = [f"platform/ps2/native/{n}.cpp" for n in NATIVE]
        cpp += [f"platform/ps2/tests/{name}_oracle.cpp","src/runtime/tgp.cpp"]
        if name == "driving_render_view":
            cpp += ["src/runtime/cpu.cpp","src/refcore/i960_ref.cpp"]
        commands.append(common+[*(str(ROOT/p) for p in cpp),str(generated),"-o",str(output/name)])
    report["commands"] = commands
    try:
        for index, command in enumerate(commands):
            run = subprocess.run(command,capture_output=True,text=True,timeout=90)
            (output/f"build-{index}.log").write_text(run.stdout+run.stderr)
            run.check_returncode()
        for name, magic, rows, words in (("driving_render_view",b"RLYVIEW1",views,45),("car_render_bridge",b"RLYBODY1",bodies,118)):
            records = output/(name+".bin")
            records.write_bytes(magic+struct.pack("<I",len(rows))+b"".join(struct.pack(f"<{words}I",*r) for r in rows))
            command = [str(output/name)]
            if name == "driving_render_view": command.append(str(args.program.resolve()))
            command += [str(args.tgp_program.resolve()),str(records)]
            run = subprocess.run(command,capture_output=True,text=True,timeout=90)
            (output/(name+".log")).write_text(run.stdout+run.stderr)
            run.check_returncode()
            if len(run.stdout.splitlines()) != 1 or not run.stdout.startswith(name.upper()+"_ORACLE PASS ") or \
               not run.stdout.rstrip().endswith(f"live_cases={len(rows)}"):
                raise RuntimeError("Original comparison completion differs")
            report[name] = {"result":run.stdout.strip(),"executable_sha256":digest(output/name),"records_sha256":digest(records)}
            print(run.stdout,end="")
        if sources != {name:digest(ROOT/name) for name in SOURCES} or \
           any(digest(Path(name)) != expected for name,expected in capture_hashes.items()) or \
           digest(args.program) != PROGRAM_SHA or digest(args.tgp_program) != FIRMWARE_SHA:
            raise RuntimeError("Source/private input changed during comparison")
        report.update(status="passed",generated_sha256=digest(generated),generator_sha256=digest(generator))
    finally:
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")


if __name__ == "__main__": main()
