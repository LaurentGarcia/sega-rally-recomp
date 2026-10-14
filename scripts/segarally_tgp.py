#!/usr/bin/env python3
"""Generate Rally's actual TGP program with Daytona's native recompiler.

Requires a verified segarally_recompile.py import and the supplied ROM archive.
Every ROM-derived artifact stays in the selected ignored build directory.
"""
from __future__ import annotations
import argparse
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

from segarally_recompile import ROOT, interleave_words, run_logged


def import_regions(archive: Path, manifest: dict, output: Path) -> tuple[dict, list]:
    images, verified = {}, []
    with zipfile.ZipFile(archive) as roms:
        names = {}
        for entry in roms.infolist():
            if entry.is_dir():
                continue
            name = Path(entry.filename).name
            if name in names:
                raise ValueError(f"ambiguous archive filename: {name}")
            names[name] = entry
        for region in manifest["regions"]:
            image = bytearray([region["fill"]]) * region["size"]
            for item in region["loads"]:
                candidates = [name for name in [item["file"], *item.get("aliases", [])] if name in names]
                if not candidates:
                    raise ValueError(f"missing verified TGP input: {item['file']}")
                for name in candidates:
                    data = roms.read(names[name])
                    crc = f"{zlib.crc32(data) & 0xffffffff:08x}"
                    sha1 = hashlib.sha1(data).hexdigest()
                    if len(data) != item["size"] or crc != item["crc32"] or sha1 != item["sha1"]:
                        raise ValueError(f"TGP ROM verification failed: {name}")
                if item["load"] != "ROM_LOAD32_WORD":
                    raise ValueError("unsupported TGP ROM load")
                interleave_words(image, item["offset"], data)
                verified.append({"file": item["file"], "archive_names": candidates,
                                 "crc32": crc, "sha1": sha1, "size": len(data)})
            images[region["name"]] = image
    output.mkdir(parents=True, exist_ok=True)
    audit = {}
    for name, image in images.items():
        path = output / f"{name}.bin"
        path.write_bytes(image)
        audit[name] = {"path": str(path.relative_to(ROOT)), "size": len(image),
                       "sha256": hashlib.sha256(image).hexdigest()}
    return audit, verified


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if not build.is_relative_to(ROOT / "build") or build == ROOT / "build":
        parser.error("--build-dir must be a child of the project's ignored build directory")
    output = build / "tgp"
    try:
        manifest = json.loads((ROOT / "config/segarally_tgp.json").read_text())
        imported = json.loads((build / "import.json").read_text())
        if imported["romset"] != manifest["romset"] or imported["board"] != manifest["board"]:
            raise ValueError("TGP program requires the verified srallycb import")
        image_info = imported["images"]["program"]
        program_path = (ROOT / image_info["path"]).resolve()
        if not program_path.is_relative_to(build):
            raise ValueError("program image lies outside the audited build directory")
        program = program_path.read_bytes()
        if hashlib.sha256(program).hexdigest() != image_info["sha256"]:
            raise ValueError("program image differs from verified import")
        spec = manifest["program"]
        count = struct.unpack_from("<I", program, int(spec["count_offset"], 16))[0]
        if count != spec["word_count"]:
            raise ValueError("TGP upload length differs from verified revision")
        start = int(spec["offset"], 16)
        firmware = program[start:start + count * 4]
        if hashlib.sha256(firmware).hexdigest() != spec["sha256"]:
            raise ValueError("TGP program differs from verified upload")
        images, files = import_regions(args.roms, manifest, output)
        firmware_path = output / "program.bin"
        firmware_path.write_bytes(firmware)
        compiler = shlex.split(args.compiler)
        flags = ["-std=c++20", "-O1", "-fno-fast-math", "-ffp-contract=off", "-Isrc"]
        tool = output / "m2tgprecomp"
        run_logged(compiler + flags + ["tools/m2tgprecomp/main.cpp", "-o", str(tool)], output / "generator-build.log")
        generated = output / "program.cpp"
        run_logged([str(tool), str(firmware_path), str(generated)], output / "generate.log")
        executable = output / "srally_tgp_boot"
        run_logged(compiler + flags + ["tools/srally_tgp_boot/main.cpp", "src/runtime/tgp.cpp",
                   "src/runtime/m2_tgp_board.cpp", "src/runtime/model2a_bus.cpp", str(generated),
                   "-o", str(executable)], output / "build.log")
        result = subprocess.run([str(executable), str(firmware_path),
                str(output / "copro_tgp_tables.bin"), str(output / "copro_data.bin")],
                cwd=ROOT, text=True, capture_output=True, timeout=30)
        transcript = result.stdout + result.stderr
        (output / "boot.txt").write_text(transcript)
        report = {"schema": 1, "romset": manifest["romset"], "source": manifest["source"],
                  "program": spec, "images": images, "verified_files": files,
                  "native_code_compiled": True, "full_command_parity_verified": False,
                  "exit_code": result.returncode, "transcript": transcript,
                  "generation": (output / "generate.log").read_text()}
        (output / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
        print(report["generation"], end="")
        print(transcript, end="")
        print(f"TGP audit: {output / 'audit.json'}")
        return result.returncode
    except (OSError, ValueError, KeyError, RuntimeError, struct.error, subprocess.SubprocessError) as failure:
        print(f"segarally_tgp: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
