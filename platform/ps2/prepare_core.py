#!/usr/bin/env python3
"""Describe a locally audited Sega Rally core for PS2 cross-compilation.

Only paths, build settings and provenance hashes are written. Generated game
code and ROM images stay in their original ignored native-build directory.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("ps2_staging_provenance", ROOT / "platform/segarally/stage_data.py")
staging = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(staging)

RUNTIME_SOURCES = [
    "cpu", "lockstep", "model2a_bus", "model2a_clock", "model2a_machine",
    "model2a_control", "model2_io", "model2_eeprom", "model2a_io", "model2a_comm",
    "comm_board", "model2_uart", "model2a_uart", "tgp", "m2_tgp_board",
    "model2a_video", "video", "raster", "geo",
]
HOST_SOURCES = ["gs_display", "model2_gs", "vu0_pack", "pad_input", "presentation", "controls", "storage", "status_screen"]
DEFINITIONS = [
    "M2_PAGED_ROM=1", "M2_COMPACT_TILEMAP=1", "M2_INSTRUCTION_BOUNDARY_HOOK=1", "M2_SRALLY_TGP=1",
    "M2_SRALLY_IO=1", "M2_SRALLY_COMM=1", "M2_SRALLY_CONTROL=1",
    "M2_SRALLY_VIDEO=1", "M2_SRALLY_SCREEN=1", "M2_SRALLY_UART=1", "RALLY_PS2_EE=1",
]
LIBRARIES = ["draw", "graph", "dma", "packet", "patches", "pad", "m", "fileXio"]
IOP_MODULES = ["sio2man", "padman", "iomanX", "fileXio", "bdm", "bdmfs_fatfs", "usbd", "usbmass_bd"]
SOUND_AUDIT_DEPENDENCIES = {
    "src/runtime/scsp_pan_table.h", "src/runtime/sound_image_source.h",
    *["src/runtime/" + name + suffix for name in
      ("model2_scsp", "model2_scsp_dsp", "model2a_sound_bus", "model2a_clock")
      for suffix in (".h", ".cpp")],
}


def validate_sound_configuration(audit: dict) -> None:
    flags = audit.get("runtime_flags", [])
    if not isinstance(flags, list) or "-DM2_COMPACT_SOUND=1" not in flags:
        raise ValueError("PS2 sound requires a timed --compact-scsp execution audit")
    sources = audit.get("sources_sha256")
    if not isinstance(sources, dict) or not SOUND_AUDIT_DEPENDENCIES <= sources.keys():
        raise ValueError("PS2 sound audit lacks compact table, image source or clock dependencies")


def build_path(path: Path) -> Path:
    resolved = path.resolve()
    if not resolved.is_relative_to((ROOT / "build").resolve()):
        raise ValueError("native build and output must stay inside this checkout's ignored build directory")
    return resolved


def validate_native_build(source: Path) -> tuple[list[Path], dict]:
    source = build_path(source)
    audits = [json.loads(staging.confined_file(source / name, source).read_text())
              for name in ("coverage.json", "tgp/audit.json", "video/audit.json")]
    coverage, tgp, video = audits
    if (coverage.get("board") != "model2a" or coverage.get("native_objects_compiled") is not True or
            any(audit.get("romset") != "srallycb" for audit in audits) or
            tgp.get("native_code_compiled") is not True):
        raise ValueError("audited Revision B Model2A and native TGP generation required")
    reach = coverage.get("reachability", {})
    if reach.get("reference_cycle_accounting") is not True or reach.get("reference_cycle_accounting_version") != 1:
        raise ValueError("timed native generation required")
    boundary = staging.verify_boundary_contract(source, coverage, verify_objects=False)
    if boundary is None:
        raise ValueError("PS2 core requires audited instruction-boundary-hook generation")
    if video.get("manifest_sha256") != staging.digest(ROOT / "config/segarally_video.json"):
        raise ValueError("video audit differs from the current verified manifest")
    if "program" not in tgp:
        raise ValueError("TGP firmware upload provenance is required")
    # Bind the six imported regions used by this core, without staging copies.
    for audit, names in ((coverage, ("program", "main_data")),
                         (tgp, ("copro_tgp_tables", "copro_data")),
                         (video, ("polygons", "textures"))):
        for name in names:
            entry = audit["images"][name]
            path = staging.confined_file(ROOT / entry["path"], source)
            if staging.digest(path) != entry["sha256"]:
                raise ValueError(name + " differs from its audited import")
    provenance = staging.staging_provenance(source, audits, boundary)
    generated = [staging.confined_file(source / "gen" / f"chunk_{index:03d}.cpp", source / "gen")
                 for index in range(reach["chunk_count"])]
    generated += [staging.confined_file(source / "gen/gen_table.cpp", source / "gen"),
                  staging.confined_file(source / "tgp/program.cpp", source / "tgp")]
    return generated, provenance


def prepare(source: Path, output: Path, with_sound: bool = False) -> dict:
    source, output = build_path(source), build_path(output)
    if output.suffix != ".json":
        raise ValueError("--output must name a JSON manifest file")
    if output.is_relative_to(source):
        raise ValueError("write the PS2 manifest outside the audited native-build directory")
    generated, provenance = validate_native_build(source)
    sources = [ROOT / "platform/ps2/game/main.cpp", *generated]
    sources += [ROOT / "src/runtime" / (name + ".cpp") for name in RUNTIME_SOURCES]
    sources += [ROOT / "platform/ps2/src" / (name + ".cpp") for name in HOST_SOURCES]
    sources += [ROOT / "src/app/gpu/model2_frame.cpp"]
    if with_sound:
        _, sound_generated = staging.sound_inputs(source, "srallycb")
        sound_audit = source / "sound/native-timed/audit.json"
        validate_sound_configuration(json.loads(staging.confined_file(sound_audit, source).read_text()))
        sources += [sound_generated, ROOT / "platform/ps2/src/audio_output.cpp"]
        sources += [ROOT / "src/runtime" / (name + ".cpp") for name in
                    ("model2a_sound", "model2a_sound_bus", "snd_cpu", "model2_scsp", "model2_scsp_dsp")]
        provenance["sound_generation_audit_sha256"] = staging.digest(source / "sound/native-timed/audit.json")
        provenance["sound_generated_sha256"] = staging.digest(sound_generated)
    if any(not path.is_file() or not path.resolve().is_relative_to(ROOT) for path in sources):
        raise ValueError("PS2 core source inventory is missing a file or escapes the checkout")
    manifest = {
        "schema": 1,
        "target": "ps2-core-probe",
        "playable": False,
        "native_build": source.relative_to(ROOT).as_posix(),
        "generated_game_source_exported": False,
        "native_coverage_is_partial": True,
        "provenance": provenance,
        "sources": [path.relative_to(ROOT).as_posix() for path in sources],
        "includes": ["src", ".", "platform/ps2/include"],
        "defines": DEFINITIONS + (["M2_SRALLY_SOUND=1", "M2_COMPACT_SOUND=1"] if with_sound else []),
        "libraries": LIBRARIES,
        "compile_flags": [],
        "embed_iop_modules": True,
        "iop_modules": IOP_MODULES + (["libsd", "rally_pcm"] if with_sound else []),
        "pcm_transport": with_sound,
        "sound_compiled": with_sound,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile("w", dir=output.parent, prefix=output.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(manifest, stream, indent=2)
            stream.write("\n")
        temporary.replace(output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Ignored JSON build manifest; no game bytes copied")
    parser.add_argument("--with-sound", action="store_true", help="Require audited native sound generation and add the original IOP PCM transport")
    args = parser.parse_args()
    try:
        manifest = prepare(args.native_build, args.output, args.with_sound)
        print(f"Prepared {len(manifest['sources'])} sources in {args.output}; PS2 gameplay remains unverified.")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"prepare_core: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
