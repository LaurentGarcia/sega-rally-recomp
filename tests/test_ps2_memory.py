#!/usr/bin/env python3
"""PS2 asset storage bounds and exact reconstruction, using synthetic bytes."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ps2_asset_pack", ROOT / "platform/ps2/tools/asset_pack.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)

CPP = r'''
#include "platform/ps2/tools/pack_reader.h"
#include "platform/ps2/game/rom_cache.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <cstdlib>
#include <new>
static bool guard_table_allocation = false, oversized_allocation = false;
void *operator new(size_t size) {
    if (guard_table_allocation && size >= 0x40000) {
        oversized_allocation = true;
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t) noexcept { std::free(pointer); }
int main(int argc, char **argv) {
    try {
        rally::ps2::PackReader reader(argv[1], 16);
        if (argc == 2) return 0;
        if (std::string(argv[2]) == "--invalid-tgp-size") {
            rally::ps2::RomCache adapter(argv[1], 16);
            guard_table_allocation = true;
            try { adapter.tgp_tables(); }
            catch (const std::runtime_error &error) {
                guard_table_allocation = false;
                return !oversized_allocation && std::string(error.what()) ==
                    "invalid TGP table size in asset pack" ? 0 : 20;
            }
            guard_table_allocation = false;
            return 21;
        }
        using Region = rally::ps2::Region;
        const char *names[] = {"program", "main_data", "tgp_tables", "copro_data", "polygons",
                               "textures", "sound_program", "sound_samples"};
        if (reader.cache_data_bytes() != 16 * 4096) return 3;
        const uint8_t *first = reader.page(Region::Program, 0);
        const uint8_t remembered = first[0];
        for (unsigned i = 1; i <= 8; ++i) reader.page(Region::Program, i);
        if (first[0] != remembered) return 4;
        for (unsigned r = 0; r != 8; ++r) {
            std::ifstream source(std::string(argv[2]) + "/" + names[r] + ".bin", std::ios::binary);
            std::vector<uint8_t> expected{std::istreambuf_iterator<char>(source), {}};
            const auto region = Region(r);
            if (reader.size(region) != expected.size()) return 5;
            std::vector<uint8_t> output(expected.size());
            // Non-page chunks deliberately cross boundaries and force eviction.
            for (size_t offset = 0; offset < output.size(); offset += 317) {
                const size_t n = std::min(size_t(317), output.size() - offset);
                reader.read(region, uint32_t(offset), output.data() + offset, n);
            }
            if (output != expected) return 6;
            reader.read(region, reader.size(region), nullptr, 0);
            bool rejected = false;
            try { reader.read(region, reader.size(region), output.data(), 1); }
            catch (const std::runtime_error &) { rejected = true; }
            if (!rejected) return 7;
            rejected = false;
            try { reader.read(region, 1, output.data(), size_t(-1)); }
            catch (const std::runtime_error &) { rejected = true; }
            if (!rejected) return 8;
            rejected = false;
            try { reader.page(region, UINT32_MAX); }
            catch (const std::runtime_error &) { rejected = true; }
            if (!rejected) return 9;
        }
        if (reader.misses() <= 16) return 10;
        bool rejected = false;
        try { reader.size(Region(8)); }
        catch (const std::runtime_error &) { rejected = true; }
        if (!rejected) return 11;
        // The runtime adapter remembers recent page pointers. Every underlying
        // eviction, including table staging, must invalidate those pointers.
        rally::ps2::RomCache adapter(argv[1], 16);
        const uint32_t first_word = adapter.dword(rt::RomRegion::Program, 0);
        for (unsigned page = 1; page < 24; ++page)
            adapter.dword(rt::RomRegion::Program, page * 4096);
        if (adapter.dword(rt::RomRegion::Program, 0) != first_word) return 12;
        const uint64_t before_tables = adapter.misses();
        adapter.tgp_tables();
        if (adapter.misses() - before_tables < 16) return 14; // must evict the saved program page
        if (adapter.dword(rt::RomRegion::Program, 0) != first_word) return 13;
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 2; }
}
'''


class Ps2MemoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("clang++") or shutil.which("c++")
        if not cls.compiler:
            raise unittest.SkipTest("C++17 compiler required for bounded-reader validation")
        cls.compile_dir = tempfile.TemporaryDirectory(prefix="rally-ps2-reader-")
        source = Path(cls.compile_dir.name) / "reader.cpp"
        source.write_text(CPP)
        cls.reader = source.with_suffix("")
        subprocess.run([cls.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DM2_PAGED_ROM=1",
                        "-I", str(ROOT), "-I", str(ROOT / "src"),
                        str(source), "-o", str(cls.reader)], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.compile_dir.cleanup()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rally-ps2-memory-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.images = []
        for n, name in enumerate(pack.REGIONS):
            if n == 0:
                data = b"".join(bytes([p + 1]) * pack.PAGE for p in range(24))
            elif name == "tgp_tables":
                # Exact hardware table size. 64 nonzero logical pages force
                # eviction of the adapter's just-remembered program pointer.
                data = b"".join(bytes([p + 17]) * pack.PAGE for p in range(64))
            else:
                data = pack.ZERO + bytes([n]) * pack.PAGE * 2 + bytes([0xa5]) * 19
            path = self.directory / (name + ".bin")
            path.write_bytes(data)
            self.images.append(dict(name=name, path=path, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        self.path = self.directory / "test.srp"
        self.report = pack.write_pack(self.images, self.path)

    def test_reconstruction_dedup_and_cpp_cache(self):
        reader = pack.PackReader(self.path)
        self.addCleanup(reader.close)
        self.assertEqual(reader.verify()["verified_regions"], 8)
        self.assertLess(self.path.stat().st_size, sum(i["size"] for i in self.images))
        result = subprocess.run([str(self.reader), str(self.path), str(self.directory)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in pack.REGIONS:
            with self.assertRaises(ValueError):
                reader.page(name, -1)
            with self.assertRaises(ValueError):
                reader.page(name, reader.regions[name]["pages"])

    def test_memory_audit_separates_storage_and_runtime(self):
        report = pack.audit(self.images, 16)
        self.assertFalse(report["complete_ee_runtime_footprint_measured"])
        self.assertEqual(report["estimated_pack_bytes"], self.path.stat().st_size)
        self.assertEqual(report["configured_cache_data_bytes"], 65536)
        self.assertTrue(report["unmeasured"])
        for cache_pages in (0, 15, 4097):
            with self.assertRaises(ValueError):
                pack.audit(self.images, cache_pages)

    def test_adapter_rejects_wrong_tgp_size_before_allocating_region(self):
        # The generic pack structure permits these lengths within its total
        # page bound. The machine adapter must impose the exact TGP contract.
        for table_size in (4096, 32 * 1024 * 1024):
            with self.subTest(table_size=table_size):
                sizes = [table_size if name == "tgp_tables" else 4096 for name in pack.REGIONS]
                count = sum(size // pack.PAGE for size in sizes)
                payload = pack.align(pack.HEADER.size + pack.ENTRY.size * 8 + count * 4)
                data = bytearray(pack.HEADER.pack(pack.MAGIC, 1, pack.PAGE, 8, 32, 544, payload))
                cursor = 544
                for name, size in zip(pack.REGIONS, sizes):
                    pages = size // pack.PAGE
                    data.extend(pack.ENTRY.pack(name.encode(), size, cursor, pages, 0, bytes(32)))
                    cursor += pages * 4
                data.extend(bytes(payload - len(data))) # zero-page references; no large payload
                self.path.write_bytes(data)
                result = subprocess.run([str(self.reader), str(self.path), "--invalid-tgp-size"],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_bad_header_index_and_truncation_rejected_in_both_readers(self):
        original = self.path.read_bytes()
        cases = [b"broken", b"WRONGPK1" + original[8:], original[:-1]]
        for invalid_offset in (4, len(original), 0xfffff000):
            mutated = bytearray(original)
            struct.pack_into("<I", mutated, pack.HEADER.size + pack.ENTRY.size * 8, invalid_offset)
            cases.append(mutated)
        mutated = bytearray(original)
        struct.pack_into("<I", mutated, pack.HEADER.size + 20, 0xffffffff)
        cases.append(mutated)
        for data in cases:
            with self.subTest(length=len(data)):
                self.path.write_bytes(data)
                with self.assertRaises(ValueError):
                    pack.PackReader(self.path)
                result = subprocess.run([str(self.reader), str(self.path)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2, result.stderr)

    def test_payload_mutation_fails_hash_verification(self):
        data = bytearray(self.path.read_bytes())
        payload = pack.HEADER.unpack_from(data)[-1]
        data[payload + 17] ^= 1
        self.path.write_bytes(data)
        reader = pack.PackReader(self.path)
        self.addCleanup(reader.close)
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            reader.verify()

    def test_changed_input_cannot_replace_existing_pack(self):
        original = self.path.read_bytes()
        self.images[-1]["path"].write_bytes(b"bad import")
        with self.assertRaisesRegex(ValueError, "content mismatch"):
            pack.write_pack(self.images, self.path)
        self.assertEqual(self.path.read_bytes(), original)
        self.assertEqual(list(self.directory.glob("test.srp.*")), [])

    def test_wrong_order_is_rejected(self):
        with self.assertRaises(ValueError):
            pack.write_pack(list(reversed(self.images)), self.path)

    def test_staging_rejects_wrong_size_and_path(self):
        manifest = {"romset": "srallycb", "images": {i["name"]: {"file": i["path"].name,
                    "size": i["size"], "sha256": i["sha256"]} for i in self.images}}
        target = self.directory / "assets.json"
        target.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "size mismatch"):
            pack.staged_images(self.directory)
        manifest["images"]["program"]["file"] = "../outside.bin"
        target.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "unexpected file"):
            pack.staged_images(self.directory)

    def test_output_cannot_escape_ignored_build(self):
        with self.assertRaises(ValueError):
            pack.build_output(ROOT / "platform/ps2/game.srp")
        with self.assertRaises(ValueError):
            pack.build_output(ROOT / "build/../game.srp")


if __name__ == "__main__":
    unittest.main()
