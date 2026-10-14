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
from segarally_recompile import (build_continuation_tool, continuation_seeds, table_seeds,
                                object_update_seeds, object_update_roots)


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
        self.reach["instruction_addresses"].pop()
        self.reach["seeds"].append("0x500044")  # LDA displacement word is also an interior entry
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

    def update_fixture(self):
        words = [mem(0x8c, 20, mode=12), 0x500180, mem(0x92, 30, 16, 84),
                 mem(0x92, 20, 16, 4), 0x0a000000]
        self.install(0x40, words)
        self.install(0x180, [0x0a000000])
        # Self-authored callback dispatcher: target=object(r4)+4, null guard,
        # pass that same object in g0, then call through the loaded g4.
        self.install(0x200, [mem(0x90, 20, 4, 4), 0x3a << 24 | 20 << 14 | 1 << 13 | 12,
                           reg(0x5cc, 16, 4), mem(0x86, 0, 20, mode=4)])
        self.reach = {"seeds": ["0x500040"], "instruction_addresses":
            [hex(0x500000+p) for p in (0x40, 0x48, 0x4c, 0x50, 0x200, 0x204, 0x208, 0x20c)]}
        self.update_roots = [{"address": "0x00000040", "root_role": "object_constructor",
                              "name": "synthetic_constructor", "evidence": "synthetic ISA fixture"}]
        config = {"program_sha256": hashlib.sha256(self.program).hexdigest(),
            "object_update_dispatcher": {"relocation": "synthetic", "program_offset": "0x200",
                "runtime_address": "0x500200", "size": 16,
                "sha256": hashlib.sha256(self.program[0x200:0x210]).hexdigest()}}
        return config

    def updates(self, config):
        return object_update_seeds(self.program, self.reach, self.relocations, self.tool, config, self.update_roots)

    def update_hashes(self, config):
        config["program_sha256"] = hashlib.sha256(self.program).hexdigest()
        config["object_update_dispatcher"]["sha256"] = hashlib.sha256(self.program[0x200:0x210]).hexdigest()

    def test_scoped_object_registration_preserves_cell_and_provenance(self):
        config = self.update_fixture()
        rows = self.updates(config)
        self.assertEqual([r["address"] for r in rows], ["0x00000180"])
        self.assertEqual(rows[0]["definition_address"], "0x00000040")
        self.assertEqual(rows[0]["store_address"], "0x0000004c")
        self.assertEqual(rows[0]["source_sha256"], hashlib.sha256(self.program[0x40:0x50]).hexdigest())
        self.assertEqual(rows[0]["source_seed_provenance"], self.update_roots)
        self.update_roots[0]["root_role"] = "arbitrary_data"
        self.assertEqual(self.updates(config), [])

    def test_registered_existing_label_qualifies_further_update_chain(self):
        config = self.update_fixture()
        self.install(0x180, [mem(0x8c, 20, mode=12), 0x5001c0, mem(0x92, 20, 16, 4), 0x0a000000])
        self.install(0x1c0, [0x0a000000])
        self.reach["instruction_addresses"] += [hex(0x500000+p) for p in (0x180, 0x188, 0x18c, 0x1c0)]
        self.reach["seeds"].append("0x500180")
        self.update_hashes(config)
        constructors = [dict(self.update_roots[0], table="object_callback_type")]
        labels = [{"address": "0x00000180", "name": "existing_label", "evidence": "fixture label"}]
        original = copy.deepcopy(labels)
        first = object_update_seeds(self.program, self.reach, self.relocations, self.tool, config,
                                   object_update_roots(constructors, labels, []))
        self.assertEqual([r["address"] for r in first], ["0x00000180"])
        roots = object_update_roots(constructors, labels, first)
        second = object_update_seeds(self.program, self.reach, self.relocations, self.tool, config, roots)
        self.assertIn("0x000001c0", [r["address"] for r in second])
        self.assertEqual(labels, original)  # classification of pre-existing seeds is preserved

    def test_object_definition_base_clobber_control_and_side_entries(self):
        for word in (reg(0x5cc, 20, 0, m1=1), reg(0x5cc, 16, 0, m1=1),
                     mem(0x90, 20, 4, 0), 0x08000004, 0x09000004, 0x42000000):
            with self.subTest(word=word):
                config = self.update_fixture()
                self.install(0x48, [word])
                self.update_hashes(config)
                self.assertEqual(self.updates(config), [])
        config = self.update_fixture()
        self.reach["seeds"].append("0x50004c")
        self.assertEqual(self.updates(config), [])
        self.reach["seeds"][-1] = "0x500044"
        self.assertEqual(self.updates(config), [])

    def test_object_dispatcher_mutations_hash_and_alias(self):
        for address, word in ((0x200, mem(0x90, 20, 4, 8)),
                (0x204, 0x3a << 24 | 20 << 14 | 1 << 13 | 8),
                (0x208, reg(0x5cc, 16, 5)), (0x20c, mem(0x86, 0, 21, mode=4))):
            with self.subTest(address=address):
                config = self.update_fixture()
                self.install(address, [word])
                self.update_hashes(config)
                with self.assertRaises(ValueError):
                    self.updates(config)
        config = self.update_fixture()
        config["object_update_dispatcher"]["runtime_address"] = "0x500204"
        with self.assertRaises(ValueError):
            self.updates(config)
        config = self.update_fixture()
        self.reach["seeds"].append("0x500208")
        with self.assertRaises(ValueError):
            self.updates(config)

    def test_object_cell_offset_and_noncode_bounds(self):
        for word in (mem(0x92, 20, 16, 8), mem(0x92, 20, 4, 4), mem(0x82, 20, 16, 4)):
            config = self.update_fixture()
            self.install(0x4c, [word])
            self.update_hashes(config)
            self.assertEqual(self.updates(config), [])
        for target in (0, 0x500181, 0x500400):
            config = self.update_fixture()
            self.install(0x44, [target])
            self.update_hashes(config)
            self.assertEqual(self.updates(config), [])
        config = self.update_fixture()
        self.reach["instruction_addresses"].remove("0x500204")
        self.assertEqual(self.updates(config), [])

    def leaf_fixture(self):
        config = self.update_fixture()
        self.install(0x40, [reg(0x5cc, 4, 16), 0x0b0000bc,
            mem(0x8c, 20, mode=12), 0x500180, mem(0x92, 20, 4, 4), 0x0a000000])
        self.install(0x100, [reg(0x5cc, 18, 30), reg(0x5cc, 30, 0, m1=1),
            reg(0x5cc, 16, 0, m1=1), mem(0x84, 0, 18, mode=4)])
        self.reach["instruction_addresses"] = [hex(0x500000+p) for p in
            (0x40, 0x44, 0x48, 0x50, 0x54, 0x100, 0x104, 0x108, 0x10c, 0x200, 0x204, 0x208, 0x20c)]
        self.update_hashes(config)
        return config

    def test_saved_object_alias_across_decoded_returning_leaf(self):
        config = self.leaf_fixture()
        rows = self.updates(config)
        self.assertEqual([r["address"] for r in rows], ["0x00000180"])
        row = rows[0]
        self.assertEqual(row["object_base_register"], 4)
        self.assertEqual(row["definition_address"], "0x00000048")
        self.assertEqual(row["source_seed_provenance"], self.update_roots)
        self.assertEqual(row["source_sha256"], hashlib.sha256(self.program[0x40:0x54]).hexdigest())
        leaf = row["leaf_calls"][0]
        self.assertEqual(leaf["call_address"], "0x00000044")
        self.assertEqual(leaf["source_address"], "0x00000100")
        self.assertEqual(leaf["source_size"], 16)
        self.assertEqual(leaf["clobbered_registers"], [16, 18, 30])
        self.assertEqual(leaf["source_sha256"], hashlib.sha256(self.program[0x100:0x110]).hexdigest())
        # Copy an alias again, and preserve the original constant's definition.
        self.install(0x50, [reg(0x5cc, 7, 4), reg(0x5cc, 22, 20), mem(0x92, 22, 7, 4)])
        self.reach["instruction_addresses"].append("0x500058")
        self.update_hashes(config)
        row = self.updates(config)[0]
        self.assertEqual((row["object_base_register"], row["value_register"]), (7, 22))
        self.assertEqual(row["definition_address"], "0x00000048")

    def test_leaf_rejects_register_clobbers_including_wide_and_paired_results(self):
        for word in (reg(0x5cc, 4, 0, m1=1), mem(0x90, 4, 10, 0),
                     mem(0x98, 5, 10, 0), mem(0xb0, 7, 10, 0), reg(0x670, 3, 20, 21)):
            with self.subTest(word=word):
                config = self.leaf_fixture()
                self.install(0x108, [word])
                self.update_hashes(config)
                self.assertEqual(self.updates(config), [])
        # Both halves of emul and the rounded memory-load footprint also kill
        # callback constants saved before a leaf, even when Object survives.
        for word in (reg(0x5cc, 20, 0, m1=1), reg(0x670, 19, 22, 23),
                     mem(0x98, 21, 10, 0), mem(0xb0, 23, 10, 0)):
            with self.subTest(constant_clobber=word):
                config = self.leaf_fixture()
                self.install(0x44, [mem(0x8c, 20, mode=12), 0x500180, 0x0b0000b4])
                self.reach["instruction_addresses"].remove("0x500048")
                self.reach["instruction_addresses"].append("0x50004c")
                self.install(0x108, [word])
                self.update_hashes(config)
                self.assertEqual(self.updates(config), [])

    def test_leaf_requires_exact_saved_link_return_and_no_nested_control(self):
        for address, word in ((0x100, reg(0x5cc, 18, 29)),
                (0x108, reg(0x5cc, 18, 0, m1=1)), (0x108, mem(0x98, 19, 10, 0)),
                (0x108, 0x08000004), (0x108, 0x0b000078), (0x108, 0x09000078),
                (0x108, 0x0a000000), (0x108, 0x42000000),
                (0x108, reg(0x670, 31, 20, 21)),
                (0x10c, mem(0x84, 0, 30, mode=4)), (0x10c, mem(0x84, 0, 18, 4))):
            with self.subTest(address=address, word=word):
                config = self.leaf_fixture()
                self.install(address, [word])
                self.update_hashes(config)
                self.assertEqual(self.updates(config), [])

    def test_leaf_requires_reached_bytes_and_rejects_known_interior_entries(self):
        for pc in (0x100, 0x104, 0x108, 0x10c):
            config = self.leaf_fixture()
            self.reach["instruction_addresses"].remove(hex(0x500000+pc))
            self.assertEqual(self.updates(config), [])
        for pc in (0x104, 0x108, 0x10c):
            config = self.leaf_fixture()
            self.reach["seeds"].append(hex(0x500000+pc))
            self.assertEqual(self.updates(config), [])
        config = self.leaf_fixture()
        self.install(0x20, [0x080000e8])  # direct external edge to leaf interior
        self.reach["instruction_addresses"].append("0x500020")
        self.update_hashes(config)
        self.assertEqual(self.updates(config), [])
        for target in (0x102, 0x400):
            config = self.leaf_fixture()
            self.install(0x44, [0x0b000000 | (target-0x44)])
            self.update_hashes(config)
            self.assertEqual(self.updates(config), [])

    def test_bal_implicit_link_clobber_and_unknown_call_are_not_abi_preserved(self):
        config = self.leaf_fixture()
        self.install(0x40, [reg(0x5cc, 30, 16)])
        self.install(0x50, [mem(0x92, 20, 30, 4)])
        # A leaf which changes no register still receives a newly written g14.
        self.install(0x100, [mem(0x84, 0, 30, mode=4)])
        self.update_hashes(config)
        self.assertEqual(self.updates(config), [])
        config = self.leaf_fixture()
        self.install(0x44, [mem(0x85, 30, 10, mode=4)])  # unknown balx
        self.update_hashes(config)
        self.assertEqual(self.updates(config), [])

    def test_multiple_leaf_calls_keep_separate_hashed_provenance(self):
        config = self.leaf_fixture()
        self.install(0x48, [0x0b0000b8, mem(0x8c, 20, mode=12), 0x500180,
                           mem(0x92, 20, 4, 4)])
        self.reach["instruction_addresses"].remove("0x500050")
        self.reach["instruction_addresses"].append("0x50004c")
        self.update_hashes(config)
        row = self.updates(config)[0]
        self.assertEqual([r["call_address"] for r in row["leaf_calls"]], ["0x00000044", "0x00000048"])
        self.assertEqual(row["leaf_calls"][0]["source_sha256"], row["leaf_calls"][1]["source_sha256"])

    def test_leaf_integer_flags_do_not_write_gprs_and_all_clobbers_are_recorded(self):
        config = self.leaf_fixture()
        body = [reg(0x5cc, 18, 30), reg(0x5cc, 30, 0, m1=1),
            mem(0x90, 17, mode=12), 0x200000,
            mem(0x8c, 21, mode=12), 0x24681234, reg(0x670, 22, 21, 17),
            reg(0x5ae, 4, 31, 22, m1=1) | (1 << 13),  # unused dst/m3
            reg(0x5b0, 20, 23, 23), reg(0x590, 20, 22, 20),
            reg(0x58c, 20, 31, 20, m1=1), reg(0x5cc, 16, 20),
            mem(0x92, 20, mode=12), 0x200000, mem(0x84, 0, 18, mode=4)]
        self.install(0x100, body)
        self.reach["instruction_addresses"] = [pc for pc in self.reach["instruction_addresses"]
            if not 0x500100 <= int(pc, 16) < 0x50013c]
        self.reach["instruction_addresses"] += [hex(0x500000+p) for p in
            (0x100, 0x104, 0x108, 0x110, 0x118, 0x11c, 0x120, 0x124, 0x128, 0x12c, 0x130, 0x138)]
        self.update_hashes(config)
        leaf = self.updates(config)[0]["leaf_calls"][0]
        self.assertEqual(leaf["source_size"], 60)
        self.assertEqual(leaf["clobbered_registers"], [16, 17, 18, 20, 21, 22, 23, 30])
        self.assertEqual(leaf["source_sha256"], hashlib.sha256(self.program[0x100:0x13c]).hexdigest())
        # A seed at a displacement word is also a forbidden interior entry.
        self.reach["seeds"].append("0x500114")
        self.assertEqual(self.updates(config), [])

    def test_rounded_wide_load_cannot_preserve_original_object_base(self):
        for word in (mem(0x98, 17, 4, 0), mem(0xb0, 19, 4, 0)):
            config = self.update_fixture()
            self.install(0x48, [word])
            self.update_hashes(config)
            self.assertEqual(self.updates(config), [])
        config = self.callback_fixture()
        self.install(0x94, [mem(0x98, 17, 4, 0)])
        self.callback_hashes(config)
        with self.assertRaises(ValueError):
            table_seeds(self.program, config, self.relocations, self.tool)

    def callback_hashes(self, config):
        config["program_sha256"] = hashlib.sha256(self.program).hexdigest()
        table = config["tables"][0]
        table["table_sha256"] = hashlib.sha256(self.program[0x300:0x300+table["entry_count"]*4]).hexdigest()
        body = table["code_evidence"][0]
        start = int(body["program_offset"], 16)
        body["sha256"] = hashlib.sha256(self.program[start:start+body["size"]]).hexdigest()

    def compare_fixture(self):
        config = self.callback_fixture()
        words = [reg(0x5a0, 0, 22, 2, m2=1), mem(0xb2, 16, 31, 80), mem(0x92, 20, 31, 112),
            0x11000054, mem(0x90, 20, mode=14, index=22, scale=2), 0x500300,
            mem(0x84, 0, 20, mode=4)]
        self.install(0x100, words)
        targets = [0x500180, 0x5001c0, 0x500200]
        self.install(0x300, targets)
        table = config["tables"][0]
        table.update(kind="compare_dispatch", entry_count=3, targets=[hex(v) for v in targets],
            rejected_index_target="0x500160", code_evidence=[{"role": "body", "program_offset": "0x100",
                "runtime_address": "0x500100", "size": len(words)*4}])
        self.callback_hashes(config)
        return config

    def test_unsigned_comparison_preserved_across_frame_spills(self):
        config = self.compare_fixture()
        rows = table_seeds(self.program, config, self.relocations, self.tool)
        self.assertEqual([r["address"] for r in rows], ["0x00000180", "0x000001c0", "0x00000200"])

    def test_unsigned_comparison_clobbers_branch_and_alias_mutations(self):
        mutations = {0x100: reg(0x5a1, 0, 22, 2, m2=1),
            0x104: reg(0x590, 22, 22, 1, m2=1), 0x108: reg(0x5a0, 0, 20, 2, m2=1),
            0x10c: 0x16000054, 0x110: mem(0x90, 20, mode=14, index=21, scale=2),
            0x118: mem(0x84, 0, 21, mode=4)}
        for address, word in mutations.items():
            with self.subTest(address=address):
                config = self.compare_fixture()
                self.install(address, [word])
                self.callback_hashes(config)
                with self.assertRaises(ValueError):
                    table_seeds(self.program, config, self.relocations, self.tool)
        config = self.compare_fixture()
        config["tables"][0]["rejected_index_target"] = "0x500164"
        with self.assertRaises(ValueError):
            table_seeds(self.program, config, self.relocations, self.tool)

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
