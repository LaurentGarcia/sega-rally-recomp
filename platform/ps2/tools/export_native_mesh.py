#!/usr/bin/env python3
"""Export privately captured, unprojected model objects as bounded native meshes.

Original game data is never embedded here. Outputs must be new files below the
repository's ignored build directory. Original materials/camera are not recovered.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[3]
POLYGON_SHA256 = "3fddc91c5b0ab9247d32518047066aa71a1db398a38d3d4f56ba3bc84c2ddfb7"
VERTICES, FACES, OBJECTS = 65536, 32768, 1024


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_json(path: Path, limit: int):
    if path.stat().st_size > limit:
        raise ValueError(f"metadata limit exceeded: {path.name}")
    return json.loads(path.read_text())


def scalar(bits: int) -> float:
    if type(bits) is not int or not 0 <= bits <= 0xffffffff:
        raise ValueError("coordinate must be a binary32 bit pattern")
    magnitude = bits & 0x7fffffff
    if (magnitude and magnitude < 0x00800000) or magnitude > 0x47800000:
        raise ValueError("coordinate outside native normal-or-zero / 65536 domain")
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def verify_decoded_source(record: dict, raw: bytes) -> None:
    """Independently reconstruct indexed connectivity from fixed-width links.

    This verification never uses JSON coordinates/triangles to drive decoding.
    Source records are six initial XYZ words, then ten-word link slots, followed
    optionally by a one/four-word terminator. Link type selects retained edges.
    """
    if len(raw) % 4 or len(raw) < 24:
        raise ValueError("truncated model source")
    words = struct.unpack(f"<{len(raw)//4}I", raw)
    points = [list(words[0:3]), list(words[3:6])]
    triangles = []
    edge = [0,1]
    cursor, links, hidden, degenerate, terminated = 6,0,0,0,False
    count, mode = record["declared_links"], record["mode"] & 3
    if type(count) is not int or not 1 <= count <= 0xfffff:
        raise ValueError("invalid declared source links")
    while links < count:
        if cursor >= len(words) or links == 32768:
            raise ValueError("source link missing or limit exceeded")
        attr = words[cursor]
        if attr & 3 == 0:
            cursor += 4 if mode < 2 else 1
            terminated = True
            break
        if cursor + 10 > len(words):
            raise ValueError("truncated source link")
        a = len(points)
        points.append(list(words[cursor+4:cursor+7]))
        b = a
        if attr & 1:
            b += 1
            points.append(list(words[cursor+7:cursor+10]))
        corners = [edge[1],edge[0],a,b]
        connection = attr >> 8 & 3
        if connection:
            fan = [(corners[0],corners[1],corners[2])]
            if attr & 1:
                fan.append((corners[0],corners[2],corners[3]))
            for face in fan:
                if len(set(face)) == 3:
                    triangles.append([*face,attr])
                else:
                    degenerate += 1
        else:
            hidden += 1
        edge = ([a,b], [edge[0],a], [a,b], [b,edge[1]])[connection]
        cursor += 10
        links += 1
    if cursor != len(words):
        raise ValueError("source decode did not consume exactly the captured records")
    expected = {"points": points, "faces": triangles, "links": links,
                "hidden_links": hidden, "degenerate_faces": degenerate,
                "terminated": terminated}
    if any(record[key] != value for key,value in expected.items()):
        raise ValueError("captured coordinates/topology disagree with original source words")


def native_payload(records: list[dict]) -> tuple[bytes, dict]:
    """Pure encoder; tests use only original synthetic data."""
    vertices, faces, objects = bytearray(), bytearray(), bytearray()
    nv = nf = 0
    summaries = []
    for record in records:
        points, triangles, matrix = record["points"], record["faces"], record["matrix_bits"]
        if len(points) < 3 or not triangles or len(matrix) != 12:
            raise ValueError("selected object has no complete drawable mesh")
        if len(points) + nv > VERTICES or len(triangles) + nf > FACES or len(summaries) == OBJECTS:
            raise ValueError("native mesh capacity exceeded")
        for point in points:
            if len(point) != 3:
                raise ValueError("invalid XYZ record")
            for word in point:
                scalar(word)
            vertices.extend(struct.pack("<3I", *point))
        # A deliberately original, constant diagnostic material. No arcade
        # palette/texture/shading interpretation is asserted by this format.
        color = 0xffa6c9e8
        for triangle in triangles:
            if len(triangle) != 4:
                raise ValueError("invalid triangle record")
            indices = triangle[:3]
            if len(set(indices)) != 3 or any(type(i) is not int or not 0 <= i < len(points) for i in indices):
                raise ValueError("triangle indices outside object")
            faces.extend(struct.pack("<4I", *(i + nv for i in indices), color))
        for word in matrix:
            scalar(word)
        affine = [*matrix[0:3], 0, *matrix[3:6], 0, *matrix[6:9], 0, *matrix[9:12], 0x3f800000]
        objects.extend(struct.pack("<20I", nv, len(points), nf, len(triangles), *affine))
        summaries.append({"capture_object": record["object"], "first_vertex": nv,
                          "vertex_count": len(points), "first_face": nf,
                          "face_count": len(triangles), "source_kind": record.get("source_kind", "synthetic"),
                          "source_sha256": record.get("source_sha256")})
        nv += len(points)
        nf += len(triangles)
    if not summaries:
        raise ValueError("no drawable objects selected")
    payload = bytes(vertices + faces + objects)
    crc = zlib.crc32(payload)
    header = struct.pack("<8s10I16x", b"RLYMESH\0", 1, 64, nv, nf, len(summaries), crc,
                         len(payload), 12, 16, 80)
    return header + payload, {"vertices": nv, "faces": nf, "objects": len(summaries),
                              "payload_crc32": f"{crc:08x}", "selection": summaries}


def export_capture(capture: Path, polygons: Path, output: Path, selected: set[int] | None) -> dict:
    build = (ROOT / "build").resolve()
    if (ROOT / "build").is_symlink():
        raise ValueError("build directory must not be a symlink")
    capture, output = capture.resolve(), output.resolve()
    if not capture.is_relative_to(build) or not output.is_relative_to(build):
        raise ValueError("capture and output must remain below ignored build/")
    report_path = output.with_suffix(".json")
    if output.exists() or report_path.exists() or output.suffix != ".bin":
        raise ValueError("output must be a new .bin file with a new .json sidecar")
    summary = load_json(capture / "capture.json", 4096)
    if summary.get("schema") != 1 or summary.get("complete") is not True:
        raise ValueError("capture is incomplete or unsupported")
    if not 0 <= summary["objects"] <= OBJECTS or not 0 <= summary["source_words"] <= 4194304:
        raise ValueError("capture limits exceeded")
    index_path = capture / "objects.jsonl"
    if index_path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("capture index size exceeded")
    records = [json.loads(line) for line in index_path.read_text().splitlines()]
    if len(records) != summary["objects"] or [r["object"] for r in records] != list(range(len(records))):
        raise ValueError("object index does not match complete capture")
    if polygons.stat().st_size != 33554432:
        raise ValueError("polygon region size mismatch")
    rom = polygons.read_bytes()
    if sha256(rom) != POLYGON_SHA256:
        raise ValueError("polygon region hash does not match verified local import")
    chosen, excluded = [], []
    total_words = 0
    for record in records:
        ordinal = record["object"]
        if record.get("schema") != 1 or record.get("frame") != summary["frame"]:
            raise ValueError("object schema/frame differs from capture summary")
        if record["source_file"] != f"object-{ordinal}.bin":
            raise ValueError("unexpected source filename")
        source_path = capture / record["source_file"]
        if source_path.is_symlink() or not 0 <= record["source_words"] <= 327690:
            raise ValueError("invalid source file or word limit")
        if source_path.stat().st_size != record["source_words"] * 4:
            raise ValueError("source byte count mismatch")
        raw = source_path.read_bytes()
        total_words += len(raw) // 4
        verify_decoded_source(record, raw)
        record["source_sha256"] = sha256(raw)
        address = record["object_address"]
        is_rom = bool(address & 0x00800000) and not bool(address & 0x01000000)
        record["source_kind"] = "verified-polygon-ROM" if is_rom else "captured-polygon-RAM"
        if is_rom:
            size = record["source_size_words"]
            if size * 4 != len(rom) or record["source_start_word"] != address & (size - 1):
                raise ValueError("captured ROM addressing disagrees with imported region")
            begin = record["source_start_word"] * 4
            expected = rom[begin:begin + len(raw)]
            if len(expected) != len(raw):
                expected += rom[:len(raw) - len(expected)]
            if raw != expected:
                raise ValueError("captured source words do not match original polygon ROM")
        elif record["source_size_words"] != 0x8000 or record["source_start_word"] != address & 0x7fff:
            raise ValueError("captured polygon RAM addressing is inconsistent")
        if selected is not None and ordinal not in selected:
            continue
        if not record["faces"]:
            excluded.append({"object": ordinal, "reason": "no drawable source faces"})
            continue
        # Fail on unsupported selected geometry, rather than silently dropping it.
        native_payload([record])
        chosen.append(record)
    if total_words != summary["source_words"]:
        raise ValueError("aggregate source count mismatch")
    if selected is not None and not selected.issubset(range(len(records))):
        raise ValueError("selected object does not exist")
    blob, report = native_payload(chosen)
    report.update({"schema": 1, "frame": summary["frame"], "mesh_sha256": sha256(blob),
                   "bytes": len(blob), "polygon_sha256": sha256(rom),
                   "capture_index_sha256": sha256(index_path.read_bytes()),
                   "exporter_sha256": sha256(Path(__file__).read_bytes()), "excluded": excluded,
                   "source_mapping": "coordinates, connectivity and link counts re-derived from captured words",
                   "direct_commands_excluded": summary["direct_commands_excluded"],
                   "materials": "original diagnostic opaque flat color; arcade materials not recovered",
                   "geometry": "raw model XYZ and complete drawable strip links before camera culling",
                   "matrix": "captured object_to_reference_view, before focus; not recovered world/camera"})
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(blob)
    with report_path.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--polygons", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--objects", help="comma-separated capture object ordinals; default all drawable objects")
    args = parser.parse_args()
    try:
        selected = None if args.objects is None else {int(v) for v in args.objects.split(",")}
        report = export_capture(args.capture, args.polygons, args.output, selected)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"mesh export failed: {error}\n")
    print(json.dumps({k: report[k] for k in ("vertices", "faces", "objects", "bytes", "mesh_sha256")}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
