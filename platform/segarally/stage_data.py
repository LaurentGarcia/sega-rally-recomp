#!/usr/bin/env python3
"""Stage only hash-verified, personally imported images for the native host."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_boot import sound_inputs
from segarally_recompile import boundary_artifact_hashes, verify_boundary_contract

TGP_MANIFEST = ROOT / "config/segarally_tgp.json"


def confined_file(path: Path, build: Path) -> Path:
    path = path.resolve()
    if not path.is_relative_to(build) or not path.is_file():
        raise ValueError("staging provenance artifact is outside its build or missing")
    return path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference_identity(audit: dict) -> dict:
    # These are declared public reference IDs, not hashes of local source files.
    return {key: audit["source"][key] for key in ("repository", "commit")
            if key in audit.get("source", {})}


def staging_provenance(source: Path, audits: list[dict], boundary: dict | None) -> dict:
    """Bind actual staging inputs without inventing generation authentication.

    Legacy audits need not contain every optional artifact. Present artifacts
    are confined and checked against existing audit assertions. Ordinary main
    and TGP audits do not seal generated source bytes: their hashes identify
    the files being consumed, and do not prove how those files were produced.
    """
    coverage, tgp, video = audits
    main = {"coverage_sha256": digest(confined_file(source / "coverage.json", source)),
            "generation_source_verification": "boundary_contract" if boundary else "identity_only"}
    tgp_info = {"audit_sha256": digest(confined_file(source / "tgp/audit.json", source)),
                "generation_source_verification": "identity_only"}
    video_info = {"audit_sha256": digest(confined_file(source / "video/audit.json", source)),
                  "manifest_sha256": video["manifest_sha256"]}
    for record, audit in ((main, coverage), (tgp_info, tgp), (video_info, video)):
        reference = reference_identity(audit)
        if reference:
            record["reference"] = reference
    reach_path = source / "reachability.json"
    if reach_path.exists() or reach_path.is_symlink():
        reach_path = confined_file(reach_path, source)
        if json.loads(reach_path.read_text()) != coverage["reachability"]:
            raise ValueError("main reachability report differs from coverage audit")
        main["reachability_sha256"] = digest(reach_path)
    generated = source / "gen"
    if generated.exists() or generated.is_symlink():
        count = coverage["reachability"].get("chunk_count")
        if count is not None:
            if type(count) is not int or count <= 0:
                raise ValueError("invalid main generated chunk count")
            names = {f"chunk_{i:03d}.cpp" for i in range(count)} | {"gen_table.cpp"}
        else:
            names = {path.name for path in generated.glob("*.cpp")}
        main["generated_sha256"] = boundary_artifact_hashes(source, "gen", names)
    if boundary and "sources_sha256" in boundary:
        # Already independently checked by verify_boundary_contract above.
        main["sources_sha256"] = boundary["sources_sha256"]
    tgp_generated = source / "tgp/program.cpp"
    if tgp_generated.exists() or tgp_generated.is_symlink():
        tgp_info["generated_sha256"] = digest(confined_file(tgp_generated, source / "tgp"))
    if "program" in tgp:
        manifest = json.loads(TGP_MANIFEST.read_text())
        spec = manifest["program"]
        if manifest["romset"] != coverage["romset"] or tgp["program"] != spec:
            raise ValueError("TGP firmware specification differs from current manifest")
        program_entry = coverage["images"]["program"]
        program = confined_file(ROOT / program_entry["path"], source).read_bytes()
        count_offset, offset = int(spec["count_offset"], 16), int(spec["offset"], 16)
        count = spec["word_count"]
        if (count_offset < 0 or count_offset + 4 > len(program) or offset < 0 or
            type(count) is not int or count <= 0 or offset + count * 4 > len(program) or
            struct.unpack_from("<I", program, count_offset)[0] != count):
            raise ValueError("TGP firmware upload bounds or length differs")
        firmware = program[offset:offset + count * 4]
        if hashlib.sha256(firmware).hexdigest() != spec["sha256"]:
            raise ValueError("TGP firmware differs from verified main program upload")
        firmware_path = source / "tgp/program.bin"
        if firmware_path.exists() or firmware_path.is_symlink():
            if confined_file(firmware_path, source / "tgp").read_bytes() != firmware:
                raise ValueError("TGP firmware artifact differs from verified main program upload")
        tgp_info.update(firmware_sha256=spec["sha256"], manifest_sha256=digest(TGP_MANIFEST))
    return {"schema": 1, "main": main, "tgp": tgp_info, "video": video_info}


def stage(source: Path, destination: Path, sound_source: Path | None = None,
          instruction_boundary_hook: bool = False) -> None:
    source, destination = source.resolve(), destination.resolve()
    if not source.is_relative_to(ROOT / "build") or not destination.is_relative_to(ROOT / "build"):
        raise ValueError("source and destination must be inside the ignored project build directory")
    audits = [json.loads(confined_file(source / name, source).read_text()) for name in
              ("coverage.json", "tgp/audit.json", "video/audit.json")]
    coverage, tgp, video = audits
    timing = coverage.get("reachability", {})
    if coverage.get("board") != "model2a" or not coverage.get("native_objects_compiled"):
        raise ValueError("native Model2A compilation audit required")
    if not timing.get("reference_cycle_accounting") or timing.get("reference_cycle_accounting_version") != 1:
        raise ValueError("timed native generation required")
    # The host recompiles these sources itself. Object hashes are checked by
    # runners that link pipeline objects; source/header provenance still applies.
    boundary = verify_boundary_contract(source, coverage, verify_objects=False)
    if bool(boundary) != instruction_boundary_hook:
        raise ValueError("host boundary support must match the audited native generation")
    if any(a.get("romset") != "srallycb" for a in audits) or not tgp.get("native_code_compiled"):
        raise ValueError("matching srallycb native TGP and video imports required")
    manifest = ROOT / "config/segarally_video.json"
    if video.get("manifest_sha256") != hashlib.sha256(manifest.read_bytes()).hexdigest():
        raise ValueError("video import does not match the current verified manifest")
    entries = [(coverage, "program", "program"), (coverage, "main_data", "main_data"),
               (tgp, "copro_tgp_tables", "tgp_tables"), (tgp, "copro_data", "copro_data"),
               (video, "polygons", "polygons"), (video, "textures", "textures")]
    verified = []
    for audit, image, output in entries:
        entry = audit["images"][image]
        path = (ROOT / entry["path"]).resolve()
        if not path.is_relative_to(source) or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"{image} differs from the audited import")
        verified.append((path, output, entry["sha256"]))
    sound_audit = None
    if sound_source is not None:
        sound_source = sound_source.resolve()
        images, generated = sound_inputs(sound_source, coverage["romset"])
        for path, name in zip(images, ("sound_program", "sound_samples")):
            path = path.resolve()
            if not path.is_relative_to(sound_source):
                raise ValueError("sound image is outside its verified build")
            verified.append((path, name, hashlib.sha256(path.read_bytes()).hexdigest()))
        sound_audit = {"timing": "instruction_boundary", "bus_phase_verified": False,
            "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
            "audit_sha256": hashlib.sha256((sound_source / "sound/native-timed/audit.json").read_bytes()).hexdigest()}
    provenance = staging_provenance(source, audits, boundary)
    destination.mkdir(parents=True, exist_ok=True)
    report = {"romset": "srallycb", "images": {}, "native_coverage_is_partial": True}
    report["provenance"] = provenance
    if sound_audit is not None:
        report["sound"] = sound_audit
    if boundary is not None:
        report["instruction_boundary_hook"] = {"version": boundary["version"],
            "runtime_enabled_by_default": False,
            "coverage_sha256": hashlib.sha256((source / "coverage.json").read_bytes()).hexdigest()}
    for path, output, digest in verified:
        target = destination / f"{output}.bin"
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            shutil.copyfile(path, target)
        report["images"][output] = {"file": target.name, "size": target.stat().st_size, "sha256": digest}
    report_path = destination / "assets.json"
    content = json.dumps(report, indent=2) + "\n"
    if not report_path.exists() or report_path.read_text() != content:
        report_path.write_text(content)
    print(f"Staged {len(verified)} verified images: {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-build", type=Path, default=ROOT / "build/segarally-timed")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sound-build", type=Path, help="Optional audited native sound generation")
    parser.add_argument("--instruction-boundary-hook", action="store_true",
                        help="Require audited exact per-chunk boundary support")
    args = parser.parse_args()
    try:
        stage(args.native_build, args.output, args.sound_build, args.instruction_boundary_hook)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(2, f"stage_data: {error}\n")
