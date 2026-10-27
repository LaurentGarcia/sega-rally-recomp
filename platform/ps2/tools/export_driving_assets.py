#!/usr/bin/env python3
"""Convert verified owner data to a private native driving asset package.

No game data is embedded. This is not a vehicle-state initializer.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[3]
INPUTS = {
    "program": (2097152, "bcea5800ff9ec4a2cb0feb69f667f6cceb1aa033eff7fc68d74642316de16439"),
    "main_data": (37748736, "2c55fca2ed44c87a8886cfcfb0638ad8e25f5fac0e9fbe5d4cf990ba950cc8e1"),
    "copro": (8388608, "22d65f223f3b341088c013666c8f35b522ef9b4c0a383b36f7c0b683396c524f"),
    "tables": (262144, "3163343896150d46e3280f71eb276cfeabd9190602c0bfdd53a6cb18d91f0b04"),
}

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def words(data: bytes, offset: int, count: int) -> tuple[int, ...]:
    if offset < 0 or offset % 4 or count < 0 or count > (len(data) - offset) // 4:
        raise ValueError("source word range outside verified image")
    return struct.unpack_from(f"<{count}I", data, offset)

def bounded(bits: int) -> bool:
    return type(bits) is int and 0 <= bits <= 0xffffffff and bits & 0x7fffffff <= 0x49800000

def encode_package(selector: int, vertices: list, quads: list, segments: list,
                   normals: list, parameters: list, local: list, balance_words: list,
                   power_curves: list, motion_words: list, motion_local: list,
                   sound_rows: list, directions: list) -> bytes:
    """Pure encoder accepting raw word records; tests supply original fixtures."""
    if type(selector) is not int or not 0 <= selector < 8:
        raise ValueError("invalid selector")
    count = sum(map(len, segments))
    if not 1 <= len(vertices) <= 65536 or not 1 <= len(segments) <= 4096 or not 1 <= count <= 65535:
        raise ValueError("native count limit exceeded")
    if len(quads) != len(segments) or len(directions) != len(segments) or len(normals) != 8192 or len(parameters) != 10 or len(local) != 4:
        raise ValueError("fixed record count mismatch")
    payload = bytearray()
    for vertex in vertices:
        if len(vertex) != 2 or not all(map(bounded, vertex)):
            raise ValueError("invalid selection coordinate")
        payload.extend(struct.pack("<2I", *vertex))
    for quad in quads:
        if len(quad) != 4 or any(type(i) is not int or not 0 <= i < len(vertices) for i in quad):
            raise ValueError("invalid selection corner")
        payload.extend(struct.pack("<4H", *quad))
    first = 0
    for segment in segments:
        if not 1 <= len(segment) <= 255:
            raise ValueError("invalid polygon range")
        payload.extend(struct.pack("<2I", first, len(segment)))
        first += len(segment)
    for segment in segments:
        for polygon in segment:
            if len(polygon) != 17 or any(type(w) is not int or not 0 <= w <= 0xffffffff for w in polygon):
                raise ValueError("invalid polygon payload")
            if any(not bounded(polygon[i]) for i in (0,2,3,5,6,8,9,11)):
                raise ValueError("invalid contact XZ")
            payload.extend(struct.pack("<17I", *polygon))
    for pair in normals:
        if len(pair) != 2 or any(type(w) is not int for w in pair) or not 0x3e000000 <= pair[0] <= 0x3f800000 or not 0x3f800000 <= pair[1] <= 0x40800000:
            raise ValueError("invalid normal coefficients")
        payload.extend(struct.pack("<2I", *pair))
    for record in parameters:
        if len(record) != 13 or any(type(w) is not int or not 0 <= w <= 0xffffffff for w in record):
            raise ValueError("invalid tuning record")
        payload.extend(struct.pack("<13I", *record))
    for point in local:
        if len(point) != 3 or not all(map(bounded, point)):
            raise ValueError("invalid local point")
        payload.extend(struct.pack("<3I", *point))
    if len(balance_words)!=260 or len(power_curves)!=164 or len(motion_words)!=65 or len(motion_local)!=4 or not 1<=len(sound_rows)<=4096:
        raise ValueError("vehicle asset record count mismatch")
    for records,finite_count in ((balance_words,212),(power_curves,164),(motion_words,0)):
        if any(type(w) is not int or not 0<=w<=0xffffffff for w in records):
            raise ValueError("invalid vehicle asset word")
        if any(w&0x7fffffff>=0x7f800000 for w in records[:finite_count]):
            raise ValueError("nonfinite vehicle asset")
        payload.extend(struct.pack(f"<{len(records)}I",*records))
    if motion_words[64]&0x7fffffff>=0x7f800000:
        raise ValueError("nonfinite motion scalar")
    for point in motion_local:
        if len(point)!=3 or not all(map(bounded,point)):
            raise ValueError("invalid secondary local point")
        payload.extend(struct.pack("<3I",*point))
    for row in sound_rows:
        if len(row)!=3 or any(type(v) is not int or not 0<=v<=255 for v in row):
            raise ValueError("invalid sound command row")
        payload.extend(bytes(row))
    for direction in directions:
        if len(direction)!=2 or not all(map(bounded,direction)):
            raise ValueError("invalid course direction")
        payload.extend(struct.pack("<2I",*direction))
    return struct.pack("<8s13I36x", b"RLYDRV3\0", 3, 96, selector, len(vertices),
                       len(segments), count, 8192, 10, 4, zlib.crc32(payload), len(payload), 0,len(sound_rows)) + payload

def extract(program: bytes, main_data: bytes, copro: bytes, tables: bytes, selector: int) -> tuple[bytes, dict]:
    if type(selector) is not int or not 0 <= selector < 8:
        raise ValueError("invalid selector")
    count_address, descriptor, selection = words(program, 0x3dac0 + 12 * selector, 3)
    count, = words(copro, 4 * count_address, 1)
    selection_count, vertex_base, record_base, direction_base = words(main_data, selection - 0x02000000, 4)
    if count != selection_count or not 1 <= count <= 4096:
        raise ValueError("course and selection counts disagree")
    directions=[words(main_data,direction_base-0x02000000+8*index,2) for index in range(count)]
    quads = []
    for index in range(count):
        offset = record_base - 0x02000000 + 12 * index + 4
        if offset < 0 or offset + 8 > len(main_data):
            raise ValueError("selection record out of range")
        quads.append(struct.unpack_from("<4H", main_data, offset))
    vertex_count = max(max(quad) for quad in quads) + 1
    vertices = []
    for index in range(vertex_count):
        x, _, z = words(main_data, vertex_base - 0x02000000 + 12 * index, 3)
        vertices.append((x, z))
    segments = []
    for index in range(count):
        length, geometry, attributes, _ = words(copro, 4 * (descriptor + 4 * index), 4)
        if not 1 <= length <= 255:
            raise ValueError("polygon record count out of range")
        segments.append([(*words(copro, geometry + 64 * p, 16), *words(copro, attributes + 4 * p, 1)) for p in range(length)])
    normals = [(words(tables, 4 * (0xc000 + 2 * i), 1)[0] & 0x7fffffff,
                words(tables, 4 * (0xc001 + 2 * i), 1)[0]) for i in range(8192)]
    parameters = [words(program, pointer - 0x59f000, 13) for pointer in words(program, 0x377a0, 10)]
    local = [words(program, 0x33e30 + 12 * i, 3) for i in range(4)]
    balance = [w for offset,length in ((0x37680,5),(0x39230,192),(0x39530,2),(0x3a254,1),
               (0x3a264,5),(0x3a300,7),(0x3dd10,40),(0x3ae30,8)) for w in words(program,offset,length)]
    power = [*words(program,0x39f20,82),*words(program,0x3a0c0,82)]
    motion = [*words(program,0x34e40,64),*words(program,0x34e3c,1)]
    # Each location is the immediate word of a verified LDA, not an arbitrary
    # presumed table. The following instruction publishes the loaded register.
    immediate = (0x352d8,0x352e8,0x352f8,0x3532c,0x3533c,0x3534c,
                 0x35390,0x353a0,0x353b0,0x353e4,0x353f4,0x35404)
    secondary=[]
    for i,address in enumerate(immediate):
        opcode,value=words(program,address-4,2)
        store,target=words(program,address+4,2)
        if opcode & 0xff07ffff != 0x8c003000 or store != 0x92003000 | (opcode & 0x00f80000) or target != 0x00884000:
            raise ValueError("secondary point operand is not the verified LDA format")
        if i%3==0:secondary.append([])
        secondary[-1].append(value)
    # Supported direct events <=181; initialized parameterized codes <=188;
    # source sound records <=179, occasional <=120, motion lists <=143.
    # Keep every required table row through demonstrated index188, without
    # claiming a larger arbitrary command namespace.
    sound_rows=[tuple(program[0x24ce0+3*i:0x24ce0+3*i+3]) for i in range(189)]
    package = encode_package(selector, vertices, quads, segments, normals, parameters, local,
                             balance,power,motion,secondary,sound_rows,directions)
    return package, {"selector": selector, "vertices": len(vertices), "segments": count,
                     "polygons": sum(map(len, segments)), "normal_pairs": 8192,
                     "parameter_records": 10, "local_points": 4,"secondary_local_points":4,
                     "power_variants":2,"power_samples_per_variant":82,"sound_rows":189,"course_directions":count,
                     "payload_crc32": f"{zlib.crc32(package[96:]):08x}"}

def private_destination(output: Path) -> Path:
    build = ROOT / "build"
    if build.is_symlink() or not build.is_dir():
        raise ValueError("ignored build directory unavailable")
    absolute = output.absolute()
    for parent in (absolute, *absolute.parents):
        if parent.is_symlink():
            raise ValueError("output path must not contain symlinks")
    resolved = absolute.resolve()
    if not resolved.is_relative_to(build.resolve()) or resolved.suffix != ".bin":
        raise ValueError("output must remain a .bin under ignored build/")
    if resolved.exists() or resolved.with_suffix(".json").exists():
        raise ValueError("output and sidecar must be new")
    return resolved

def export(inputs: dict[str, Path], selector: int, output: Path) -> dict:
    output = private_destination(output)
    images, sources = {}, {}
    for name, (size, expected) in INPUTS.items():
        path = inputs[name]
        if path.stat().st_size != size:
            raise ValueError(f"{name} size mismatch")
        data = path.read_bytes()
        if digest(data) != expected:
            raise ValueError(f"{name} SHA256 mismatch")
        images[name] = data
        sources[name] = {"bytes": size, "sha256": expected}
    package, info = extract(**images, selector=selector)
    report = {"schema": 3, "scope": "native contact, selection, course guidance, balance/input and motion assets; not vehicle-state initialization",
              "inputs": sources, "output": {**info, "bytes": len(package), "sha256": digest(package)},
              "exporter_sha256": digest(Path(__file__).read_bytes())}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(package)
    with output.with_suffix(".json").open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    return report

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for key in INPUTS:
        parser.add_argument("--" + key.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--selector", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = export({key: getattr(args, key) for key in INPUTS}, args.selector, args.output)
    print(json.dumps(report["output"], sort_keys=True))

if __name__ == "__main__":
    main()
