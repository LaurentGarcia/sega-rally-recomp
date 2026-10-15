#!/usr/bin/env python3
"""Synthetic ISA fixtures for bounded table seed provenance; no game bytes."""
import copy
import hashlib
import os
import shlex
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_recompile import table_seeds, build_continuation_tool


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

    def test_distinct_loaded_destination_and_bx_register(self):
        table = self.config["tables"][0]
        table["destination_register"] = 18
        load = struct.unpack_from("<I", self.program, 0x84)[0]
        branch = struct.unpack_from("<I", self.program, 0x8c)[0]
        struct.pack_into("<I", self.program, 0x84, (load & ~(31 << 19)) | 18 << 19)
        struct.pack_into("<I", self.program, 0x8c, (branch & ~(31 << 14)) | 18 << 14)
        self.refresh_hashes(self.program, self.config)
        self.assertEqual(len(table_seeds(self.program, self.config, self.relocations)), 2)
        for destination in (16, 17, 32):
            changed = copy.deepcopy(self.config)
            changed["tables"][0]["destination_register"] = destination
            self.reject(config=changed)
        struct.pack_into("<I", self.program, 0x8c, branch)
        self.refresh_hashes(self.program, self.config)
        self.reject()

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


class DirectMaskTableTests(unittest.TestCase):
    """Invented four-entry layout, unrelated registers/data/addresses."""
    @classmethod
    def setUpClass(cls):
        cls.tool = build_continuation_tool(shlex.split(os.environ.get("CXX", "c++")),
                                          ROOT / "build/validation/direct-mask-tests")

    @staticmethod
    def reg(op, dst, first, second=0, m1=0):
        return ((op & 0xff0) << 20) | ((op & 15) << 7) | dst << 19 | second << 14 | m1 << 11 | first

    @staticmethod
    def mem(op, dst, base=0, mode=4, index=0, scale=0):
        return op << 24 | dst << 19 | base << 14 | mode << 10 | scale << 7 | index

    def setUp(self):
        self.program = bytearray(0x400)
        self.words = [self.reg(0x581, 23, 3, 23, m1=1),
                      self.mem(0x90, 23, mode=14, index=23, scale=2), 0x500300,
                      0x3d << 24 | 23 << 14 | 1 << 13 | 0x1c,
                      self.reg(0x5cc, 17, 5, m1=1),
                      self.mem(0x92, 30, mode=12), 0x5002a0,
                      self.mem(0x92, 17, mode=12), 0x5002a4,
                      0x0a000000, self.mem(0x86, 0, 23)]
        self.install(0x80, self.words)
        targets = [0x500180, 0, 0x5001c0, 0x500180]
        self.install(0x300, targets)
        self.config = {"board": "model2a", "romset": "srallycb", "tables": [{
            "name": "invented_direct", "kind": "direct_mask_callback", "relocation": "fixture",
            "program_offset": "0x300", "runtime_address": "0x500300", "entry_count": 4,
            "targets": [hex(t) for t in targets], "code_evidence": [{"role": "body",
            "program_offset": "0x80", "runtime_address": "0x500080", "size": len(self.words)*4}]}]}
        self.relocations = {"relocations": [{"name": "fixture", "program_offset": "0x0",
                            "guest": "0x500000", "size": "0x400"}]}
        self.refresh()

    def install(self, where, words):
        for n, word in enumerate(words): struct.pack_into("<I", self.program, where+n*4, word)

    def refresh(self):
        TableSeedTests.refresh_hashes(self.program, self.config)

    def rows(self):
        return table_seeds(self.program, self.config, self.relocations, self.tool)

    def test_direct_candidates_unique_null_skipped_and_static_provenance(self):
        rows = self.rows()
        self.assertEqual([r["address"] for r in rows], ["0x00000180", "0x000001c0"])
        self.assertEqual(rows[0]["table_indices"], [0, 3])
        self.assertIn("direct four-entry", rows[0]["provenance"])
        self.assertIn("runtime observation", rows[0]["provenance"])

    def test_all_nonzero_entries(self):
        targets = [0x500180, 0x5001c0, 0x500200, 0x500240]
        self.install(0x300, targets)
        self.config["tables"][0]["targets"] = [hex(t) for t in targets]
        self.refresh()
        self.assertEqual(len(self.rows()), 4)

    def test_rehashed_operand_and_control_mutations(self):
        mutations = {
            "wrong_mask": (0x80, self.reg(0x581, 23, 7, 23, m1=1)),
            "mask_SFR_quirk": (0x80, self.words[0] | 1 << 5),
            "load_reserved_quirk": (0x84, self.words[1] | 1 << 5),
            "guard_unaligned_quirk": (0x8c, self.words[3] | 2),
            "call_reserved_quirk": (0xa8, self.words[10] | 1 << 6),
            "mask_not_literal": (0x80, self.reg(0x581, 23, 3, 23)),
            "mask_source": (0x80, self.reg(0x581, 23, 3, 22, m1=1)),
            "mask_literal_destination": (0x80, self.words[0] | 1 << 13),
            "load_index": (0x84, self.mem(0x90, 23, mode=14, index=22, scale=2)),
            "load_scale": (0x84, self.mem(0x90, 23, mode=14, index=23, scale=1)),
            "load_destination": (0x84, self.mem(0x90, 22, mode=14, index=23, scale=2)),
            "load_base_alias": (0x84, self.mem(0x90, 23, base=23, mode=15, index=23, scale=2)),
            "guard_equal": (0x8c, 0x3a << 24 | 23 << 14 | 1 << 13 | 0x1c),
            "guard_nonzero_operand": (0x8c, self.words[3] | 1 << 19),
            "guard_wrong_register": (0x8c, 0x3d << 24 | 22 << 14 | 1 << 13 | 0x1c),
            "guard_not_literal": (0x8c, self.words[3] ^ 1 << 13),
            "guard_extension_word": (0x8c, 0x3d << 24 | 23 << 14 | 1 << 13 | 0x14),
            "guard_after_call": (0x8c, 0x3d << 24 | 23 << 14 | 1 << 13 | 0x20),
            "live_register_clobber": (0x90, self.reg(0x5cc, 23, 1, m1=1)),
            "unknown_register_op": (0x90, self.reg(0x590, 17, 1, 17, m1=1)),
            "null_path_call": (0x90, 0x09000018),
            "null_path_branch": (0xa4, 0x08000004),
            "call_wrong_register": (0xa8, self.mem(0x86, 0, 22)),
            "call_indexed": (0xa8, self.mem(0x86, 0, 23, mode=7, index=23)),
            "call_bx": (0xa8, self.mem(0x84, 0, 23)),
        }
        for name, (where, word) in mutations.items():
            with self.subTest(name=name):
                self.setUp(); self.install(where, [word]); self.refresh()
                with self.assertRaises(ValueError): self.rows()

    def test_rehashed_count_alias_body_and_targets_rejected(self):
        for kind in ("count", "alias", "body", "table_alias", "target_range", "target_alignment", "target_metadata"):
            with self.subTest(kind=kind):
                self.setUp(); t=self.config["tables"][0]
                if kind=="count": t["entry_count"]=3
                elif kind=="alias": self.relocations["relocations"][0]["guest"]="0x500004"
                elif kind=="body": t["code_evidence"][0]["size"]-=4
                elif kind=="table_alias": t["runtime_address"]="0x500304"
                elif kind=="target_metadata": self.install(0x300, [0x500200])
                else:
                    target=0x500400 if kind=="target_range" else 0x500181
                    self.install(0x300, [target]); t["targets"][0]=hex(target)
                self.refresh()
                with self.assertRaises(ValueError): self.rows()

    def test_wrong_hashes_and_truncated_instruction_rejected(self):
        for field in ("program", "table", "body", "truncated"):
            with self.subTest(field=field):
                self.setUp();t=self.config["tables"][0]
                if field=="program":self.config["program_sha256"]="0"*64
                elif field=="table":t["table_sha256"]="0"*64
                elif field=="body":t["code_evidence"][0]["sha256"]="0"*64
                else:self.relocations["relocations"][0]["size"]="0xa8"
                with self.assertRaises(ValueError):self.rows()


if __name__ == "__main__":
    unittest.main()
