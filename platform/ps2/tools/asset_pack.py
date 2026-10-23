#!/usr/bin/env python3
"""Measure or pack staged Rally images without loading complete ROM regions.

The pack keeps guest byte offsets intact; zero/repeated 4096-byte pages share
storage. It is a seekable data source, not evidence that a PS2 game fits RAM.
Input SHA-256 values are checked against assets.json, whose importer provenance
must be verified separately. Game-derived output belongs only in ignored build/.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PAGE = 4096
MAGIC = b"SRPS2PK1"
HEADER = struct.Struct("<8s6I")
ENTRY = struct.Struct("<16s4I32s")
REGIONS = ("program", "main_data", "tgp_tables", "copro_data", "polygons",
           "textures", "sound_program", "sound_samples")
EXPECTED_SIZES = (0x200000, 0x2400000, 0x40000, 0x800000, 0x2000000,
                  0x1000000, 0x80000, 0x800000)
EE_BYTES = 32 * 1024 * 1024
ZERO = bytes(PAGE)
MAX_FILE = 0x7fffffff  # PS2 stdio uses signed 32-bit file offsets.


def align(value: int) -> int:
    return (value + PAGE - 1) // PAGE * PAGE


def staged_images(directory: Path) -> list[dict]:
    """Validate names, confinement and size before any game data is written."""
    directory = directory.resolve()
    manifest = json.loads((directory / "assets.json").read_text())
    if manifest.get("romset") != "srallycb" or set(manifest["images"]) != set(REGIONS):
        raise ValueError("expected all eight srallycb staged images")
    result = []
    for name, expected in zip(REGIONS, EXPECTED_SIZES):
        item = manifest["images"][name]
        if item.get("file") != name + ".bin":
            raise ValueError(f"unexpected file for {name}")
        path = directory / item["file"]
        if path.is_symlink() or not path.resolve().is_relative_to(directory):
            raise ValueError(f"image escapes asset directory: {name}")
        if type(item.get("size")) is not int or item["size"] != expected or path.stat().st_size != expected:
            raise ValueError(f"image size mismatch: {name}")
        digest = item.get("sha256", "")
        if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError(f"invalid SHA-256 for {name}")
        result.append(dict(name=name, path=path, size=expected, sha256=digest))
    return result


def pages(image: dict):
    digest = hashlib.sha256()
    total = 0
    with image["path"].open("rb") as stream:
        while chunk := stream.read(PAGE):
            digest.update(chunk)
            total += len(chunk)
            yield chunk.ljust(PAGE, b"\0")
    if total != image["size"] or digest.hexdigest() != image["sha256"]:
        raise ValueError(f"image content mismatch: {image['name']}")


def audit(images: list[dict], cache_pages: int = 512) -> dict:
    if not 16 <= cache_pages <= 4096:
        raise ValueError("cache-pages must be between 16 and 4096")
    unique = set()
    regions = []
    for image in images:
        own, zeros, count = set(), 0, 0
        for page in pages(image):
            count += 1
            if page == ZERO:
                zeros += 1
            else:
                own.add(hashlib.sha256(page).digest())
        unique.update(own)
        regions.append(dict(name=image["name"], bytes=image["size"], pages=count,
                            zero_pages=zeros, unique_nonzero_pages=len(own),
                            sha256=image["sha256"]))
    page_count = sum(r["pages"] for r in regions)
    image_bytes = sum(r["bytes"] for r in regions)
    metadata = HEADER.size + ENTRY.size * len(images) + page_count * 4
    return dict(schema=1, romset="srallycb", page_bytes=PAGE, regions=regions,
                image_bytes=image_bytes, ee_main_ram_bytes=EE_BYTES,
                image_bytes_exceed_ee_ram=image_bytes > EE_BYTES,
                unique_nonzero_page_bytes=len(unique) * PAGE,
                estimated_pack_bytes=align(metadata) + len(unique) * PAGE,
                pack_index_bytes=page_count * 4,
                configured_cache_data_bytes=cache_pages * PAGE,
                cache_page_lookup_bytes=page_count * 4,
                cache_metadata_bytes=cache_pages * 16,
                bounded_reader_zero_page_bytes=PAGE,
                known_model2a_bus_writable_bytes=0x40000 + 0x100000 + 0x20000 + 0x38 + 0x4000,
                complete_ee_runtime_footprint_measured=False,
                unmeasured=["R5900 executable, SDK and stack", "TGP and geometry working state",
                            "video RAM, texture cache and DMA packets", "SCSP state and audio buffers"],
                runtime_integration="pack reader requires explicit Model2A bus/device adapters")


def write_pack(images: list[dict], destination: Path) -> dict:
    if tuple(i["name"] for i in images) != REGIONS:
        raise ValueError("pack regions must use the fixed eight-region order")
    count = sum((i["size"] + PAGE - 1) // PAGE for i in images)
    table_at = HEADER.size
    index_at = table_at + ENTRY.size * len(images)
    payload_at = align(index_at + count * 4)
    if payload_at + count * PAGE > MAX_FILE:
        raise ValueError("pack exceeds signed 32-bit file offsets")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=destination.name + ".", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(HEADER.pack(MAGIC, 1, PAGE, len(images), table_at, index_at, payload_at))
            cursor = index_at
            for image in images:
                n = (image["size"] + PAGE - 1) // PAGE
                stream.write(ENTRY.pack(image["name"].encode(), image["size"], cursor, n, 0,
                                        bytes.fromhex(image["sha256"])))
                cursor += n * 4
            stream.write(bytes(payload_at - stream.tell()))
            offsets, seen = [], {}
            for image in images:
                for page in pages(image):
                    if page == ZERO:
                        offsets.append(0)
                        continue
                    digest = hashlib.sha256(page).digest()
                    offset = seen.get(digest)
                    if offset is None:
                        offset = stream.tell()
                        stream.write(page)
                        seen[digest] = offset
                    offsets.append(offset)
            total_bytes = stream.tell()
            stream.seek(index_at)
            for offset in offsets:
                stream.write(struct.pack("<I", offset))
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, destination)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise
    return dict(bytes=total_bytes, unique_nonzero_pages=len(seen), logical_pages=count)


class PackReader:
    """Host validation reader. Each call reads at most one 4 KiB data page."""
    def __init__(self, path: Path):
        self.stream = path.open("rb")
        self.regions = {}
        try:
            size = path.stat().st_size
            raw = self.stream.read(HEADER.size)
            if len(raw) != HEADER.size:
                raise ValueError("truncated pack header")
            magic, version, page, count, table, index, payload = HEADER.unpack(raw)
            if (magic, version, page, count, table, index) != (MAGIC, 1, PAGE, len(REGIONS), HEADER.size, HEADER.size + ENTRY.size * len(REGIONS)):
                raise ValueError("invalid pack header")
            if size > MAX_FILE or payload < index or payload % PAGE or size < payload or size % PAGE:
                raise ValueError("invalid pack file size")
            cursor = index
            for name in REGIONS:
                raw = self.stream.read(ENTRY.size)
                if len(raw) != ENTRY.size:
                    raise ValueError("truncated region table")
                text, length, page_index, page_count, reserved, digest = ENTRY.unpack(raw)
                if text != name.encode().ljust(16, b"\0") or reserved or not length:
                    raise ValueError("invalid region entry")
                if page_index != cursor or page_count != (length + PAGE - 1) // PAGE:
                    raise ValueError("invalid region index")
                cursor += page_count * 4
                if cursor > payload:
                    raise ValueError("region index overlaps payload")
                self.regions[name] = dict(size=length, index=page_index, pages=page_count, sha256=digest.hex())
            if payload != align(cursor):
                raise ValueError("invalid payload alignment")
            if (cursor - index) // 4 > 26304:
                raise ValueError("asset pack page map exceeds Rally bounds")
            self.payload, self.file_size = payload, size
            # Validate every page pointer before permitting a single data read.
            self.stream.seek(index)
            for _ in range((cursor - index) // 4):
                raw = self.stream.read(4)
                if len(raw) != 4:
                    raise ValueError("truncated page index")
                offset, = struct.unpack("<I", raw)
                if offset and (offset < payload or offset % PAGE or offset > size - PAGE):
                    raise ValueError("page points outside pack payload")
        except BaseException:
            self.stream.close()
            raise

    def close(self):
        self.stream.close()

    def page(self, name: str, index: int) -> bytes:
        region = self.regions[name]
        if type(index) is not int or not 0 <= index < region["pages"]:
            raise ValueError("page index outside region")
        self.stream.seek(region["index"] + index * 4)
        raw = self.stream.read(4)
        if len(raw) != 4:
            raise ValueError("truncated page index")
        offset, = struct.unpack("<I", raw)
        if offset == 0:
            return ZERO
        if offset < self.payload or offset % PAGE or offset > self.file_size - PAGE:
            raise ValueError("page points outside pack payload")
        self.stream.seek(offset)
        data = self.stream.read(PAGE)
        if len(data) != PAGE:
            raise ValueError("truncated page data")
        return data

    def verify(self) -> dict:
        for name, region in self.regions.items():
            digest = hashlib.sha256()
            for index in range(region["pages"]):
                remaining = region["size"] - index * PAGE
                digest.update(self.page(name, index)[:min(PAGE, remaining)])
            if digest.hexdigest() != region["sha256"]:
                raise ValueError(f"reconstructed SHA-256 mismatch: {name}")
        return dict(verified_regions=len(self.regions), reconstructed_bytes=sum(r["size"] for r in self.regions.values()))


def build_output(path: Path) -> Path:
    path = path.resolve()
    if not path.is_relative_to(ROOT / "build"):
        raise ValueError("outputs must be inside the ignored project build directory")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("audit", "pack"):
        p = sub.add_parser(name)
        p.add_argument("--assets", type=Path, required=True)
        p.add_argument("--output", type=Path, required=True)
        if name == "audit":
            p.add_argument("--cache-pages", type=int, default=512)
    sub.add_parser("verify").add_argument("--pack", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "verify":
            reader = PackReader(args.pack)
            try:
                report = reader.verify()
            finally:
                reader.close()
        else:
            destination = build_output(args.output)
            images = staged_images(args.assets)
            if destination in {i["path"].resolve() for i in images} or destination == (args.assets / "assets.json").resolve():
                raise ValueError("output would replace an input")
            if args.command == "pack":
                report = write_pack(images, destination)
            else:
                report = audit(images, args.cache_pages)
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"asset_pack: {error}\n")


if __name__ == "__main__":
    main()
