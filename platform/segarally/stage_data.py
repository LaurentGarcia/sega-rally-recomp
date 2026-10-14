#!/usr/bin/env python3
"""Stage only hash-verified, personally imported images for the native host."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def stage(source: Path, destination: Path) -> None:
    source, destination = source.resolve(), destination.resolve()
    if not source.is_relative_to(ROOT / "build") or not destination.is_relative_to(ROOT / "build"):
        raise ValueError("source and destination must be inside the ignored project build directory")
    audits = [json.loads((source / name).read_text()) for name in
              ("coverage.json", "tgp/audit.json", "video/audit.json")]
    coverage, tgp, video = audits
    timing = coverage.get("reachability", {})
    if coverage.get("board") != "model2a" or not coverage.get("native_objects_compiled"):
        raise ValueError("native Model2A compilation audit required")
    if not timing.get("reference_cycle_accounting") or timing.get("reference_cycle_accounting_version") != 1:
        raise ValueError("timed native generation required")
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
    destination.mkdir(parents=True, exist_ok=True)
    report = {"romset": "srallycb", "images": {}, "native_coverage_is_partial": True}
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
    args = parser.parse_args()
    try:
        stage(args.native_build, args.output)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(2, f"stage_data: {error}\n")
