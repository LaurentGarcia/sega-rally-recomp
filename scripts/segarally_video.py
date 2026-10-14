#!/usr/bin/env python3
"""Verify Sega Rally Model2A model/texture ROMs and unused video lookup tables.

Only ROM layout is established here. The reciprocal table has no runtime
consumer, and luma is CPU-written RAM, not a ROM. Outputs remain below build/.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile
import zlib

from segarally_recompile import ROOT, interleave_words


def import_regions(archive: Path, manifest: dict, output: Path) -> dict:
    output = output.resolve()
    ignored_build = (ROOT / "build").resolve()
    if not output.is_relative_to(ignored_build) or output == ignored_build:
        raise ValueError("video images must remain below this checkout's ignored build/")
    images, verified = {}, []
    with zipfile.ZipFile(archive) as roms:
        entries = {}
        for entry in roms.infolist():
            if entry.is_dir():
                continue
            name = Path(entry.filename).name
            if name in entries:
                raise ValueError(f"ambiguous archive filename: {name}")
            entries[name] = entry
        for region in manifest["regions"]:
            name = region["name"]
            if name in images or not name.replace("_", "").isalnum():
                raise ValueError("invalid or duplicate video region name")
            if region["size"] <= 0 or not 0 <= region["fill"] <= 255:
                raise ValueError("invalid video region size/fill")
            image = bytearray([region["fill"]]) * region["size"]
            occupied = []
            loaded = 0
            for item in region["loads"]:
                candidates = [candidate for candidate in [item["file"], *item.get("aliases", [])]
                              if candidate in entries]
                if not candidates:
                    raise ValueError(f"missing verified video input: {item['file']}")
                for candidate in candidates:
                    data = roms.read(entries[candidate])
                    crc = f"{zlib.crc32(data) & 0xffffffff:08x}"
                    sha1 = hashlib.sha1(data).hexdigest()
                    if len(data) != item["size"] or crc != item["crc32"] or sha1 != item["sha1"]:
                        raise ValueError(f"video ROM verification failed: {candidate}")
                offset = item["offset"]
                if offset < 0 or not data:
                    raise ValueError("invalid video load offset/size")
                if item["load"] == "ROM_LOAD32_WORD":
                    if len(data) % 2 or offset % 4 not in (0, 2):
                        raise ValueError("invalid video word lane")
                    lanes, count = (offset % 4, offset % 4 + 1), len(data) // 2
                elif item["load"] == "ROM_LOAD32_BYTE":
                    lanes, count = (offset % 4,), len(data)
                else:
                    raise ValueError("unsupported video ROM load")
                first, end = offset // 4, offset // 4 + count
                if (end - 1) * 4 + max(lanes) >= len(image):
                    raise ValueError("video ROM load exceeds declared region")
                for lane in lanes:
                    if any(lane == previous_lane and max(first, begin) < min(end, stop)
                           for previous_lane, begin, stop in occupied):
                        raise ValueError("overlapping video ROM loads")
                    occupied.append((lane, first, end))
                if item["load"] == "ROM_LOAD32_WORD":
                    interleave_words(image, offset, data)
                else:
                    image[offset:offset + len(data) * 4:4] = data
                loaded += len(data)
                verified.append({"file": item["file"], "archive_names": candidates,
                                 "size": len(data), "crc32": crc, "sha1": sha1})
            images[name] = (image, loaded)
    # Validate every required input and load before creating derived files.
    output.mkdir(parents=True, exist_ok=True)
    image_audit = {}
    for name, (image, loaded) in images.items():
        path = output / f"{name}.bin"
        path.write_bytes(image)
        image_audit[name] = {"path": str(path.relative_to(ROOT)), "region_bytes": len(image),
                             "loaded_bytes": loaded, "sha256": hashlib.sha256(image).hexdigest()}
    return {"images": image_audit, "verified_files": verified}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if not build.is_relative_to((ROOT / "build").resolve()) or build == (ROOT / "build").resolve():
        parser.error("--build-dir must be below this checkout's ignored build/")
    try:
        manifest_path = ROOT / "config/segarally_video.json"
        manifest = json.loads(manifest_path.read_text())
        if manifest["romset"] != "srallycb" or manifest["board"] != "model2a":
            raise ValueError("video import requires the srallycb Model2A manifest")
        output = build / "video"
        audit = import_regions(args.roms.expanduser().resolve(), manifest, output / "images")
        audit.update({"schema": 1, "romset": manifest["romset"], "board": manifest["board"],
            "source": manifest["source"], "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            "luma": manifest["luma"], "unused_regions": ["video_unk"],
            "rendering_verified": False, "full_machine_parity_verified": False,
            "layout": "ROM_LOAD32_WORD preserves each file's byte order in two-byte lanes of little-endian dwords; ROM_LOAD32_BYTE fills one byte lane; unpopulated bytes retain declared fill."})
        (output / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")
        print(f"Verified {len(audit['verified_files'])} video ROMs; regions: {', '.join(audit['images'])}")
        print(f"Video audit: {output / 'audit.json'}")
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(f"segarally_video: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
