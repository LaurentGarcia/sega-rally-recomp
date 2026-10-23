#!/usr/bin/env python3
"""Measure a stable emulator viewport; never treat approximate GS output as exact.

Requires Pillow. Inputs are private captures of one scene shown through the same
PS2 display path, once as the CPU reference, once through scalar GS packing and
once through VU0 GS packing. The explicit crop excludes window chrome/status.
No screenshot is exported or modified; only numerical evidence is written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def metrics(reference, actual) -> dict:
    from PIL import ImageChops
    if reference.size != actual.size:
        raise ValueError("comparison dimensions differ")
    difference = ImageChops.difference(reference, actual)
    channels = difference.split()
    maximum = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    histogram = maximum.histogram()
    pixels = reference.width * reference.height
    all_channels = difference.histogram()
    absolute = sum((index % 256) * count for index, count in enumerate(all_channels))
    squares = sum((index % 256) ** 2 * count for index, count in enumerate(all_channels))
    return {
        "pixels": pixels,
        "identical_rgb_pixels": histogram[0],
        "identical_rgb_percent": round(histogram[0] * 100 / pixels, 6),
        "mean_absolute_channel_error_0_255": round(absolute / (pixels * 3), 6),
        "root_mean_square_channel_error_0_255": round(math.sqrt(squares / (pixels * 3)), 6),
        "maximum_channel_error_0_255": max(index for index, count in enumerate(histogram) if count),
        "pixels_with_any_channel_error_over_8": sum(histogram[9:]),
        "pixels_with_any_channel_error_over_32": sum(histogram[33:]),
    }


def main() -> int:
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", required=True, type=Path)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--scalar", required=True, type=Path)
    parser.add_argument("--vu0", required=True, type=Path)
    parser.add_argument("--viewport", required=True, nargs=4, type=int, metavar=("X", "Y", "W", "H"))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    if not args.output.resolve().is_relative_to((root / "build").resolve()):
        parser.error("output must stay below ignored build/")
    x, y, width, height = args.viewport
    if min(x, y) < 0 or min(width, height) < 1:
        parser.error("invalid viewport")
    paths = {name: getattr(args, name) for name in ("reference", "scalar", "vu0")}
    images = {}
    for name, path in paths.items():
        with Image.open(path) as source:
            if x + width > source.width or y + height > source.height:
                parser.error(f"viewport exceeds {name} capture")
            images[name] = source.convert("RGB").crop((x, y, x + width, y + height))
    with args.scene.open("rb") as source:
        header = source.read(80)
    if len(header) != 80 or header[:8] != b"SRPS2SC1":
        parser.error("not a private PS2 scene")
    frame, vertices, batches = struct.unpack_from("<III", header, 24)
    report = {
        "schema": 1,
        "scene": {"path": str(args.scene), "sha256": sha256(args.scene),
                  "frame": frame, "vertices": vertices, "batches": batches},
        "captures": {name: {"path": str(path), "sha256": sha256(path)} for name, path in paths.items()},
        "viewport_xywh": args.viewport,
        "reference_vs_scalar_gs": metrics(images["reference"], images["scalar"]),
        "reference_vs_vu0_gs": metrics(images["reference"], images["vu0"]),
        "scalar_gs_vs_vu0_gs": metrics(images["scalar"], images["vu0"]),
        "scope": [
            "Measurements compare emulator-window RGB pixels after identical PS2 presentation and host display scaling/color conversion.",
            "The reference is the exact CPU image uploaded through the same GS display path.",
            "Native GS uses nearest base-level texels; filtering, mip blending and raster edge rules differ from the reference.",
            "This is quantified experimental rendering evidence, not arcade fidelity or physical PS2 certification.",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({key: report[key] for key in ("reference_vs_scalar_gs", "scalar_gs_vs_vu0_gs")}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
