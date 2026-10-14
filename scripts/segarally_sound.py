#!/usr/bin/env python3
"""Audit and import Rally's SCSP sound ROMs into ignored build storage.

Uses the pinned MAME srallycb layout, not Daytona's Model 1 sound map.
Images contain big-endian bus bytes: ROM_LOAD16_WORD_SWAP reverses each
input pair once. MAME's subsequent host-endian region normalization is
an internal storage detail and must not be applied to these portable files.
This imports data only; it does not compile or execute the sound program.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile
import zlib

from segarally_recompile import ROOT


def build_path(path: Path) -> Path:
    path = path.resolve()
    if not path.is_relative_to(ROOT / "build") or path == ROOT / "build":
        raise ValueError("sound output must be a child of the project's ignored build directory")
    return path


def word_swap(data: bytes) -> bytes:
    if len(data) & 1:
        raise ValueError("ROM_LOAD16_WORD_SWAP requires an even byte count")
    result = bytearray(len(data))
    result[0::2], result[1::2] = data[1::2], data[0::2]
    return bytes(result)


def reset_vectors(program: bytes) -> list[str]:
    if len(program) < 16:
        raise ValueError("sound program is too short for the reset vector copy")
    return [f"0x{value:08x}" for value in struct.unpack_from(">4I", program)]


def import_regions(archive: Path, manifest: dict, output: Path) -> tuple[dict, list]:
    """Verify every present canonical/alias input before writing any image."""
    output = build_path(output)
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
            name, size, fill = region["name"], region["size"], region["fill"]
            if not re.fullmatch(r"[a-z][a-z0-9_]*", name) or name in images:
                raise ValueError("invalid or duplicate sound region name")
            if size <= 0 or size & 1 or not 0 <= fill <= 255:
                raise ValueError("invalid sound region size or fill")
            image = bytearray([fill]) * size
            ranges = []
            for item in region["loads"]:
                offset, count = item["offset"], item["size"]
                if item["load"] != "ROM_LOAD16_WORD_SWAP":
                    raise ValueError("unsupported sound ROM load")
                if offset < 0 or count <= 0 or (offset | count) & 1 or offset + count > size:
                    raise ValueError("invalid sound ROM word placement")
                if any(offset < end and start < offset + count for start, end in ranges):
                    raise ValueError("overlapping sound ROM loads")
                ranges.append((offset, offset + count))
                candidates = [candidate for candidate in [item["file"], *item.get("aliases", [])]
                              if candidate in names]
                if not candidates:
                    raise ValueError(f"missing verified sound input: {item['file']}")
                for candidate in candidates:
                    if names[candidate].file_size != count:
                        raise ValueError(f"sound ROM size mismatch: {candidate}")
                    data = roms.read(names[candidate])
                    crc = f"{zlib.crc32(data) & 0xffffffff:08x}"
                    sha1 = hashlib.sha1(data).hexdigest()
                    if crc != item["crc32"] or sha1 != item["sha1"]:
                        raise ValueError(f"sound ROM verification failed: {candidate}")
                image[offset:offset + count] = word_swap(data)
                verified.append({"file": item["file"], "archive_names": candidates,
                                 "crc32": crc, "sha1": sha1, "size": count})
            images[name] = image
    if "reset" in manifest:
        if reset_vectors(images["audiocpu"]) != manifest["reset"]["expected_vectors"]:
            raise ValueError("sound reset vectors differ from the audited revision")
    paths = {name: build_path(output / f"{name}.bin") for name in images}
    output.mkdir(parents=True, exist_ok=True)
    audit = {}
    for name, image in images.items():
        paths[name].write_bytes(image)
        audit[name] = {"path": str(paths[name].relative_to(ROOT)), "size": len(image),
                       "sha256": hashlib.sha256(image).hexdigest()}
    return audit, verified


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/segarally")
    args = parser.parse_args()
    try:
        output = build_path(args.build_dir) / "sound"
        manifest = json.loads((ROOT / "config/segarally_sound.json").read_text())
        images, files = import_regions(args.roms, manifest, output)
        report = {"schema": 1, "romset": manifest["romset"], "board": manifest["board"],
                  "sound_board": manifest["sound_board"],
                  "source": manifest["source"], "images": images, "verified_files": files,
                  "image_byte_order": manifest["image_byte_order"],
                  "reset": manifest["reset"], "clocks_hz": manifest["clocks_hz"],
                  "native_code_compiled": False, "runtime_boot_verified": False,
                  "scsp_implemented": False}
        audit_path = build_path(output / "audit.json")
        audit_path.write_text(json.dumps(report, indent=2) + "\n")
        print(f"Verified {len(files)} sound ROMs; SCSP import audit: {audit_path}")
        return 0
    except (OSError, ValueError, KeyError, struct.error, zipfile.BadZipFile) as failure:
        print(f"segarally_sound: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
