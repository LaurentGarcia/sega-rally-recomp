#!/usr/bin/env python3
"""Synthetic staging identities; no ROMs, generator, compiler or device run."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

import test_segarally_host_assets as host_assets

stage_data = host_assets.stage_data


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class StagingProvenance(unittest.TestCase):
    def setUp(self):
        self.fixture = host_assets.HostAssets("test_stages_verified_images")
        self.fixture.setUp()
        self.addCleanup(self.fixture.tearDown)
        self.source = self.fixture.source
        self.target = self.fixture.target
        self.audits = self.fixture.audits
        self.audits[0]["reachability"]["chunk_count"] = 1
        (self.source / "gen").mkdir()
        for name in ("chunk_000.cpp", "gen_table.cpp"):
            (self.source / "gen" / name).write_text("// invented synthetic fixture " + name)
        (self.source / "tgp/program.cpp").write_text("// invented TGP source fixture")
        firmware = b"synthetic words!"
        program = bytearray(64)
        program[16:32] = firmware
        struct.pack_into("<I", program, 8, len(firmware) // 4)
        (self.source / "program.bin").write_bytes(program)
        self.audits[0]["images"]["program"]["sha256"] = sha(self.source / "program.bin")
        self.spec = {"offset": "0x10", "count_offset": "0x08", "word_count": 4,
                     "sha256": hashlib.sha256(firmware).hexdigest()}
        self.audits[1]["program"] = dict(self.spec)
        (self.source / "tgp/program.bin").write_bytes(firmware)
        self.manifest = self.fixture.base / "synthetic-tgp-manifest.json"
        self.manifest.write_text(json.dumps({"romset": "srallycb", "program": self.spec}))
        self.manifest_patch = patch.object(stage_data, "TGP_MANIFEST", self.manifest)
        self.manifest_patch.start()
        self.addCleanup(self.manifest_patch.stop)
        for audit in self.audits:
            audit["source"] = {"repository": "synthetic-public-reference", "commit": "fixture-revision",
                               "path": "not-a-local-source-identity"}
        self.write()

    def write(self):
        self.fixture.write_audits()
        (self.source / "reachability.json").write_text(json.dumps(self.audits[0]["reachability"]))

    def stage(self, **kwargs):
        with contextlib.redirect_stdout(io.StringIO()):
            stage_data.stage(self.source, self.target, **kwargs)
        return json.loads((self.target / "assets.json").read_text())

    def rejects(self, pattern):
        with self.assertRaisesRegex(ValueError, pattern):
            self.stage()
        self.assertFalse(self.target.exists(), "rejected provenance copied assets")

    def test_records_actual_artifact_identities_and_limited_claims(self):
        report = self.stage()
        p = report["provenance"]
        self.assertEqual(p["schema"], 1)
        self.assertEqual(p["main"]["coverage_sha256"], sha(self.source / "coverage.json"))
        self.assertEqual(p["main"]["reachability_sha256"], sha(self.source / "reachability.json"))
        self.assertEqual(p["main"]["generated_sha256"], {
            name: sha(self.source / "gen" / name) for name in ("chunk_000.cpp", "gen_table.cpp")})
        self.assertEqual(p["tgp"]["audit_sha256"], sha(self.source / "tgp/audit.json"))
        self.assertEqual(p["tgp"]["generated_sha256"], sha(self.source / "tgp/program.cpp"))
        self.assertEqual(p["tgp"]["firmware_sha256"], self.spec["sha256"])
        self.assertEqual(p["tgp"]["manifest_sha256"], sha(self.manifest))
        self.assertEqual(p["video"]["audit_sha256"], sha(self.source / "video/audit.json"))
        for part in ("main", "tgp"):
            self.assertEqual(p[part]["generation_source_verification"], "identity_only")
            self.assertNotIn("sources_sha256", p[part])
        for part in ("main", "tgp", "video"):
            self.assertEqual(p[part]["reference"], {"repository": "synthetic-public-reference",
                                                  "commit": "fixture-revision"})
        self.assertEqual(len(report["images"]), 6)
        self.assertTrue(report["native_coverage_is_partial"])

    def test_boundary_sources_come_only_from_validated_contract(self):
        contract = {"version": 1, "sources_sha256": {"synthetic/source.h": "a" * 64}}
        with patch.object(stage_data, "verify_boundary_contract", return_value=contract) as verifier:
            report = self.stage(instruction_boundary_hook=True)
        verifier.assert_called_once_with(self.source.resolve(), self.audits[0], verify_objects=False)
        self.assertEqual(report["provenance"]["main"]["generation_source_verification"], "boundary_contract")
        self.assertEqual(report["provenance"]["main"]["sources_sha256"], contract["sources_sha256"])
        self.assertEqual(report["provenance"]["tgp"]["generation_source_verification"], "identity_only")

    def test_unsealed_generated_changes_are_identities_not_authenticated(self):
        (self.source / "gen/chunk_000.cpp").write_text("// another invented source")
        (self.source / "tgp/program.cpp").write_text("// another invented TGP source")
        report = self.stage()["provenance"]
        self.assertEqual(report["main"]["generated_sha256"]["chunk_000.cpp"], sha(self.source / "gen/chunk_000.cpp"))
        self.assertEqual(report["tgp"]["generated_sha256"], sha(self.source / "tgp/program.cpp"))
        self.assertEqual(report["tgp"]["generation_source_verification"], "identity_only")

    def test_stale_reachability_rejects_before_copy(self):
        (self.source / "reachability.json").write_text('{}')
        self.rejects("reachability")

    def test_extra_generated_source_rejects_before_copy(self):
        (self.source / "gen/chunk_001.cpp").write_text("// extra fixture")
        self.rejects("inventory")

    def test_missing_generated_chunk_rejects_before_copy(self):
        (self.source / "gen/chunk_000.cpp").unlink()
        self.rejects("inventory")

    def test_escaped_generated_source_rejects_before_copy(self):
        outside = self.fixture.base / "outside.cpp"
        outside.write_text("// outside fixture")
        path = self.source / "gen/chunk_000.cpp"
        path.unlink(); path.symlink_to(outside)
        self.rejects("escaped")

    def test_escaped_audit_rejects_before_copy(self):
        path = self.source / "tgp/audit.json"
        outside = self.fixture.base / "outside-audit.json"
        outside.write_bytes(path.read_bytes())
        path.unlink(); path.symlink_to(outside)
        self.rejects("outside")

    def test_escaped_tgp_source_rejects_before_copy(self):
        path = self.source / "tgp/program.cpp"
        outside = self.source / "outside-tgp.cpp"
        outside.write_bytes(path.read_bytes())
        path.unlink(); path.symlink_to(outside)
        self.rejects("outside")

    def test_changed_tgp_firmware_rejects_before_copy(self):
        (self.source / "tgp/program.bin").write_bytes(b"wrong firmware")
        self.rejects("firmware artifact")

    def test_tgp_upload_count_checks_actual_verified_program(self):
        path = self.source / "program.bin"
        program = bytearray(path.read_bytes()); struct.pack_into("<I", program, 8, 5)
        path.write_bytes(program)
        self.audits[0]["images"]["program"]["sha256"] = sha(path)
        self.write()
        self.rejects("upload bounds or length")

    def test_tgp_upload_bytes_checked_even_when_image_audit_hash_matches(self):
        path = self.source / "program.bin"
        program = bytearray(path.read_bytes()); program[16] ^= 1
        path.write_bytes(program)
        self.audits[0]["images"]["program"]["sha256"] = sha(path)
        self.write()
        self.rejects("firmware differs")

    def test_changed_firmware_spec_rejects_before_copy(self):
        self.audits[1]["program"]["offset"] = "0x20"
        self.write()
        self.rejects("specification")

    def test_declared_upload_cannot_read_outside_program(self):
        for key, value in (("offset", "0x40"), ("count_offset", "0x40")):
            with self.subTest(key=key):
                spec = dict(self.spec, **{key: value})
                self.audits[1]["program"] = spec
                self.manifest.write_text(json.dumps({"romset": "srallycb", "program": spec}))
                self.write()
                self.rejects("upload bounds or length")

    def test_escaped_tgp_firmware_rejects_before_copy(self):
        path = self.source / "tgp/program.bin"
        outside = self.source / "outside-firmware.bin"
        outside.write_bytes(path.read_bytes())
        path.unlink(); path.symlink_to(outside)
        self.rejects("outside")

    def test_incremental_staging_is_stable_and_rejection_preserves_existing_package(self):
        self.stage()
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.target.iterdir()}
        self.stage()
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.target.iterdir()})
        (self.source / "tgp/program.bin").write_bytes(b"wrong")
        with self.assertRaisesRegex(ValueError, "firmware artifact"):
            self.stage()
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.target.iterdir()})

    def test_optional_legacy_artifacts_are_not_invented(self):
        del self.audits[1]["program"]
        self.audits[0]["reachability"].pop("chunk_count")
        self.write()
        for path in (self.source / "tgp/program.cpp", self.source / "tgp/program.bin",
                     self.source / "reachability.json", self.source / "gen/chunk_000.cpp",
                     self.source / "gen/gen_table.cpp"):
            path.unlink()
        (self.source / "gen").rmdir()
        report = self.stage()["provenance"]
        self.assertNotIn("generated_sha256", report["main"])
        self.assertNotIn("reachability_sha256", report["main"])
        self.assertNotIn("generated_sha256", report["tgp"])
        self.assertNotIn("firmware_sha256", report["tgp"])


if __name__ == "__main__":
    unittest.main()
