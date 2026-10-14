#!/usr/bin/env python3
"""Self-authored i960 fixtures for compiler-side indirect-target proofs."""
import copy
import hashlib
import os
from pathlib import Path
import struct
import shlex
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from segarally_recompile import build_continuation_tool, continuation_seeds, table_seeds


def reg(op, dst, first, second=0, m1=0, m2=0):
    return ((op & 0xff0) << 20) | ((op & 15) << 7) | dst << 19 | second << 14 | m2 << 12 | m1 << 11 | first


def mem(op, dst, base=0, offset=0, mode=None, index=0, scale=0):
    return op << 24 | dst << 19 | base << 14 | (0x2000 | offset if mode is None else mode << 10 | scale << 7 | index)


class ProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = ROOT / "build/validation/continuation-tests"
        cls.tool = build_continuation_tool(shlex.split(os.environ.get("CXX", "c++")), output)

    def setUp(self):
        self.program = bytearray(0x400)
        self.relocations = {"relocations": [{"name": "synthetic", "guest": "0x500000",
            "program_offset": "0x0", "size": "0x400"}]}
        self.words = [mem(0x8c, 30, mode=12), 0x500058, reg(0x5cc, 17, 30),
            reg(0x5cc, 30, 0, m1=1), mem(0x92, 20, 16, 12), mem(0x84, 0, 17, mode=4)]
        self.install(0x40, self.words)
        self.reach = {"seeds": ["0x500040"], "instruction_addresses":
            [hex(0x500000 + p) for p in (0x40, 0x48, 0x4c, 0x50, 0x54)]}

    def install(self, address, words):
        for index, word in enumerate(words):
            struct.pack_into("<I", self.program, address + index * 4, word)

    def direct(self):
        return continuation_seeds(self.program, self.reach, self.relocations, self.tool)

    def test_adjacent_definition_and_rom_alias(self):
        rows = self.direct()
        self.assertEqual([r["address"] for r in rows], ["0x00000058"])
        self.assertEqual(rows[0]["runtime_address"], "0x00500058")
        self.assertEqual(rows[0]["source_sha256"], hashlib.sha256(self.program[0x40:0x58]).hexdigest())

    def test_rejects_clobbers_unknown_and_control_transfers(self):
        for replacement in (reg(0x5cc, 17, 0, m1=1), 0x08000004, 0x09000004,
                            0x0a000000, 0x42000000, mem(0x90, 17, 4, 0)):
            with self.subTest(replacement=replacement):
                self.install(0x4c, [replacement])
                self.assertEqual(self.direct(), [])

    def test_requires_exact_move_and_adjacent_constant(self):
        self.install(0x48, [reg(0x5cc, 17, 29)])
        self.assertEqual(self.direct(), [])
        self.install(0x40, self.words)
        self.install(0x44, [0x50005c])
        self.assertEqual(self.direct(), [])

    def test_integer_and_preserves_definition_except_destination_clobber(self):
        self.install(0x4c, [reg(0x581, 20, 20, 21)])
        self.assertEqual(len(self.direct()), 1)
        self.install(0x4c, [reg(0x581, 17, 20, 21)])
        self.assertEqual(self.direct(), [])

    def test_rejects_known_seed_and_direct_branch_side_entries(self):
        self.reach["seeds"].append("0x50004c")
        self.assertEqual(self.direct(), [])
        self.reach["seeds"].pop()
        self.install(0x20, [0x0800002c])  # independent b into the definition's interior
        self.reach["instruction_addresses"].append("0x500020")
        self.assertEqual(self.direct(), [])

    def test_requires_reached_entry_and_all_instructions(self):
        self.reach["instruction_addresses"].remove("0x50004c")
        self.assertEqual(self.direct(), [])
        self.reach["instruction_addresses"].append("0x50004c")
        self.reach["seeds"] = []
        self.assertEqual(self.direct(), [])

    def test_copy_bounds_and_direct_incoming_edge_outside_copy(self):
        original = copy.deepcopy(self.relocations)
        self.relocations["relocations"][0]["guest"] = "0x100500000"
        with self.assertRaises(ValueError):
            self.direct()
        self.relocations = copy.deepcopy(original)
        self.relocations["relocations"][0]["size"] = "0x50"
        self.assertEqual(self.direct(), [])
        self.relocations = copy.deepcopy(original)
        self.relocations["relocations"][0].update(guest="0x500030", program_offset="0x30", size="0x3d0")
        self.assertEqual(len(self.direct()), 1)
        self.install(0x20, [0x0800002c])
        self.reach["instruction_addresses"].append("0x20")
        self.assertEqual(self.direct(), [])

    def callback_fixture(self):
        words = [reg(0x581, 20, 31, 20, m1=1), mem(0x90, 20, mode=14, index=20, scale=2),
            0x500300, reg(0x590, 21, 16, 4, m2=1), mem(0x92, 20, 21, mode=4),
            mem(0x90, 20, 4, 0), mem(0x9a, 22, 16, 24), mem(0x90, 21, 21, mode=4),
            mem(0x90, 20, 4, 12), reg(0x5a1, 0, 21, 0, m2=1),
            mem(0x92, 20, 16, 60), 0x12000008, mem(0x86, 0, 21, mode=4)]
        self.install(0x80, words)
        targets = [0x500180, 0x5001c0] + [0] * 30
        self.install(0x300, targets)
        table = {"kind": "masked_callback", "name": "fixture", "relocation": "synthetic",
            "program_offset": "0x300", "runtime_address": "0x500300", "entry_count": 32,
            "targets": [hex(v) for v in targets], "code_evidence": [{"role": "body",
                "program_offset": "0x80", "runtime_address": "0x500080", "size": len(words)*4}]}
        config = {"board": "model2a", "romset": "srallycb", "tables": [table]}
        self.callback_hashes(config)
        return config

    def callback_hashes(self, config):
        config["program_sha256"] = hashlib.sha256(self.program).hexdigest()
        table = config["tables"][0]
        table["table_sha256"] = hashlib.sha256(self.program[0x300:0x380]).hexdigest()
        body = table["code_evidence"][0]
        body["sha256"] = hashlib.sha256(self.program[0x80:0x80+body["size"]]).hexdigest()

    def test_masked_table_nulls_and_saved_pointer(self):
        config = self.callback_fixture()
        rows = table_seeds(self.program, config, self.relocations, self.tool)
        self.assertEqual([r["address"] for r in rows], ["0x00000180", "0x000001c0"])

    def test_masked_table_mutations_reject_independently_of_hashes(self):
        mutations = {0x80: reg(0x581, 20, 30, 20, m1=1), 0x84: mem(0x90, 20, mode=14, index=19, scale=2),
            0x8c: reg(0x590, 21, 16, 8, m2=1), 0x98: mem(0x9a, 22, 16, 0),
            0x94: mem(0x90, 21, 4, 0), 0xac: 0x1200000c, 0xb0: mem(0x86, 0, 20, mode=4)}
        for address, word in mutations.items():
            with self.subTest(address=address):
                config = self.callback_fixture()
                self.install(address, [word])
                self.callback_hashes(config)
                with self.assertRaises(ValueError):
                    table_seeds(self.program, config, self.relocations, self.tool)

    def test_table_hash_alias_and_target_range_rejected(self):
        config = self.callback_fixture()
        for field, value in (("table_sha256", "0"*64), ("runtime_address", "0x500304")):
            changed = copy.deepcopy(config)
            changed["tables"][0][field] = value
            with self.assertRaises(ValueError):
                table_seeds(self.program, changed, self.relocations, self.tool)
        self.install(0x300, [0x500400])
        config["tables"][0]["targets"][0] = "0x500400"
        self.callback_hashes(config)
        with self.assertRaises(ValueError):
            table_seeds(self.program, config, self.relocations, self.tool)


if __name__ == "__main__":
    unittest.main()
