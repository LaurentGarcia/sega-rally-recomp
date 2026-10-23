#!/usr/bin/env python3
"""Compare sparse EE guest checkpoints with a host core-probe CSV.

Only observed checkpoint values are checked. This cannot establish behavior
between checkpoints, emulator accuracy, physical hardware execution or speed.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[3]
FIELDS = ("frame", "instructions", "cycles", "board_ns", "pc",
          "register_hash", "screen_hash")
HEADER = ",".join(FIELDS)
ROW = re.compile(r"([0-9]+),([0-9]+),([0-9]+),([0-9]+),"
                 r"([0-9a-f]{8}),([0-9a-f]{16}),([0-9a-f]{16})")
CHECKPOINT_START = re.compile(r"[+-]?[0-9]+\s*,")


def parse_checkpoints(data, *, mixed_log=False, label="input"):
    """Parse canonical main.cpp rows; never silently ignore a broken row."""
    rows = {}
    previous = 0
    header_seen = False
    for line_number, raw in enumerate(data.decode("utf-8").splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        location = f"{label}:{line_number}"
        if line == HEADER:
            if header_seen or rows:
                raise ValueError(f"{location}: repeated or misplaced checkpoint header")
            header_seen = True
            continue
        match = ROW.fullmatch(line)
        if not match:
            if not mixed_log or CHECKPOINT_START.match(line) or line.startswith("frame,"):
                raise ValueError(f"{location}: malformed seven-field checkpoint")
            continue
        if not mixed_log and not header_seen:
            raise ValueError(f"{location}: reference CSV requires its checkpoint header")
        fields = match.groups()
        values = tuple(int(value, 10 if index < 4 else 16)
                       for index, value in enumerate(fields))
        frame = values[0]
        if not 0 < frame <= 0xffffffff or any(value > 0xffffffffffffffff for value in values[1:4]):
            raise ValueError(f"{location}: checkpoint counter is outside its unsigned range")
        if frame <= previous:
            raise ValueError(f"{location}: duplicate or out-of-order checkpoint frame {frame}")
        rows[frame] = fields
        previous = frame
    if not rows:
        raise ValueError(f"{label}: no checkpoints found")
    return rows


def compare(reference, guest):
    """Validate every observed guest row, returning the matched frame numbers."""
    matched = []
    for frame, actual in guest.items():
        if frame not in reference:
            raise ValueError(f"guest frame {frame}: missing reference checkpoint")
        expected = reference[frame]
        for field, wanted, observed in zip(FIELDS, expected, actual):
            if wanted != observed:
                raise ValueError(f"guest frame {frame}: {field} mismatch: "
                                 f"reference={wanted}, guest={observed}")
        matched.append(frame)
    if not matched:
        raise ValueError("guest: no checkpoints found")
    return matched


def evidence(reference_path, guest_path):
    inputs = {}
    parsed = {}
    for name, path, mixed in (("reference", reference_path, False),
                              ("guest", guest_path, True)):
        path = Path(path).resolve()
        data = path.read_bytes()
        parsed[name] = parse_checkpoints(data, mixed_log=mixed, label=str(path))
        inputs[name] = {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
                        "bytes": len(data), "checkpoint_count": len(parsed[name])}
    frames = compare(parsed["reference"], parsed["guest"])
    return {
        "schema": 1,
        "status": "matched",
        "inputs": inputs,
        "fields": list(FIELDS),
        "matched_frames": frames,
        "matched_checkpoint_count": len(frames),
        "scope": "Only the seven recorded fields at the listed guest checkpoints were compared. "
                 "Missing guest checkpoints and behavior between checkpoints were not checked.",
        "partial_run_evidence": True,
        "complete_reference_coverage": len(frames) == len(parsed["reference"]),
        "physical_ps2_execution_verified": False,
        "arcade_fidelity_certified": False,
        "performance_measured": False,
    }


def write_report(output, report):
    output = Path(output)
    if output.exists() or output.is_symlink():
        raise ValueError(f"refusing to overwrite existing report: {output}")
    output = output.resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or output.suffix != ".json":
        raise ValueError("report must be a new .json file under ignored build/")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True, help="host core-probe CSV")
    parser.add_argument("--guest-log", type=Path, required=True, help="mixed EE stdout log")
    parser.add_argument("--output", type=Path, required=True, help="new JSON evidence file under build/")
    args = parser.parse_args()
    try:
        report = evidence(args.reference, args.guest_log)
        output = write_report(args.output, report)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"checkpoint comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"Matched {report['matched_checkpoint_count']} guest checkpoints: "
          f"{','.join(map(str, report['matched_frames']))}; evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
