#!/usr/bin/env python3
"""Synthetic ISA fixtures for bounded table seed provenance; no game bytes."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_recompile import table_seeds


class TableSeedTests(unittest.TestCase):
    def setUp(self):
        self.program = bytearray(0x400)
        guest, source, index = 0x5a0000, 0x40, 16
        # Self-authored three-entry fixture using g0, different addresses,
        # index bound and table contents from the supplied game's table.
        bound = (0x34 << 24) | (2 << 19) | (index << 14) | 0x2000 | 0x28
        load = (0x90 << 24) | (index << 19) | (14 << 10) | (2 << 7) | index
        branch = (0x84 << 24) | (index << 14) | 0x1000
        for offset, value in ((0x80, bound), (0x84, load), (0x88, guest + 0x50),
                              (0x8c, branch), (0x90, guest + 0x70), (0x94, guest + 0x80),
                              (0x98, guest + 0x70), (0xb0, 0x08000000), (0xc0, 0x08000000)):
            struct.pack_into("<I", self.program, offset, value)
        self.relocations = {"relocations": [{"name": "fixture_copy", "guest": hex(guest),
            "program_offset": hex(source), "size": "0x200"}]}
        table = {"name": "fixture", "relocation": "fixture_copy", "program_offset": "0x90",
            "runtime_address": hex(guest + 0x50), "entry_count": 3, "index_register": index,
            "maximum_index": 2, "rejected_index_target": hex(guest + 0x68),
            "targets": [hex(guest + 0x70), hex(guest + 0x80), hex(guest + 0x70)],
            "code_evidence": [{"role": role, "program_offset": hex(offset),
                "runtime_address": hex(guest + offset - source), "size": size}
                for role, offset, size in (("bound", 0x80, 4), ("load", 0x84, 8), ("branch", 0x8c, 4))]}
        self.config = {"schema": 1, "romset": "srallycb", "board": "model2a", "tables": [table]}
        self.refresh_hashes(self.program, self.config)

    @staticmethod
    def refresh_hashes(program, config):
        config["program_sha256"] = hashlib.sha256(program).hexdigest()
        for table in config["tables"]:
            offset, size = int(table["program_offset"], 16), table["entry_count"] * 4
            table["table_sha256"] = hashlib.sha256(program[offset:offset + size]).hexdigest()
            for row in table["code_evidence"]:
                offset = int(row["program_offset"], 16)
                row["sha256"] = hashlib.sha256(program[offset:offset + row["size"]]).hexdigest()

    def reject(self, config=None, program=None, relocations=None):
        with self.assertRaises(ValueError):
            table_seeds(self.program if program is None else program,
                        self.config if config is None else config,
                        self.relocations if relocations is None else relocations)

    def test_verified_unique_candidates_and_exact_aliases(self):
        candidates = table_seeds(self.program, self.config, self.relocations)
        self.assertEqual([row["address"] for row in candidates], ["0x000000b0", "0x000000c0"])
        self.assertEqual([row["runtime_address"] for row in candidates], ["0x005a0070", "0x005a0080"])
        self.assertEqual(candidates[0]["table_indices"], [0, 2])
        self.assertIn("runtime observation", candidates[0]["provenance"])

    def test_program_code_and_table_hashes_rejected(self):
        for role in ("program", "bound", "load", "branch", "table"):
            with self.subTest(role=role):
                config = copy.deepcopy(self.config)
                if role == "program": config["program_sha256"] = "0" * 64
                elif role == "table": config["tables"][0]["table_sha256"] = "0" * 64
                else:
                    next(row for row in config["tables"][0]["code_evidence"] if row["role"] == role)["sha256"] = "0" * 64
                self.reject(config=config)

    def test_bounds_load_scale_and_branch_register_semantics(self):
        # Recompute all hashes, proving semantic checks are independent of them.
        for offset, xor in ((0x80, 1 << 24), (0x80, 1 << 19), (0x80, 1 << 13),
                            (0x84, 1 << 7), (0x84, 1 << 19), (0x8c, 1 << 14)):
            with self.subTest(offset=offset, xor=xor):
                program, config = bytearray(self.program), copy.deepcopy(self.config)
                struct.pack_into("<I", program, offset, struct.unpack_from("<I", program, offset)[0] ^ xor)
                self.refresh_hashes(program, config)
                self.reject(program=program, config=config)

    def test_table_entries_must_match_metadata(self):
        program, config = bytearray(self.program), copy.deepcopy(self.config)
        struct.pack_into("<I", program, 0x90, 0x5a0090)
        self.refresh_hashes(program, config)
        self.reject(program=program, config=config)

    def test_target_range_and_alignment(self):
        for target in (0x5a0200, 0x59ffff, 0x5a0071):
            with self.subTest(target=target):
                program, config = bytearray(self.program), copy.deepcopy(self.config)
                struct.pack_into("<I", program, 0x90, target)
                config["tables"][0]["targets"][0] = hex(target)
                self.refresh_hashes(program, config)
                self.reject(program=program, config=config)

    def test_exact_relocation_and_code_sequence_required(self):
        for change in ("guest", "source", "size", "ambiguous", "missing_role", "noncontiguous"):
            with self.subTest(change=change):
                config, relocations = copy.deepcopy(self.config), copy.deepcopy(self.relocations)
                if change == "guest": relocations["relocations"][0]["guest"] = "0x5a0004"
                elif change == "source": relocations["relocations"][0]["program_offset"] = "0x44"
                elif change == "size": relocations["relocations"][0]["size"] = "0x40"
                elif change == "ambiguous": relocations["relocations"].append(copy.deepcopy(relocations["relocations"][0]))
                elif change == "missing_role": config["tables"][0]["code_evidence"].pop()
                else: config["tables"][0]["code_evidence"][1]["runtime_address"] = "0x5a0048"
                self.reject(config=config, relocations=relocations)

    def test_count_and_rejection_target(self):
        for field, value in (("entry_count", 0), ("entry_count", 33), ("maximum_index", 3),
                             ("rejected_index_target", "0x5a006c")):
            with self.subTest(field=field):
                config = copy.deepcopy(self.config)
                config["tables"][0][field] = value
                self.reject(config=config)


if __name__ == "__main__":
    unittest.main()
