#!/usr/bin/env python3
"""Reject unsafe sound linkage before executing a shared-clock machine."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import segarally_boot as boot


class SoundLinkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.build = self.root / "build/synthetic"
        self.native = self.build / "sound/native-timed"
        self.native.mkdir(parents=True)
        (self.root / "config").mkdir()
        specification = {"romset": "srallycb", "images": {"audiocpu": "program-hash", "samples": "sample-hash"}}
        (self.root / "config/segarally_sound_boot.json").write_text(json.dumps(specification))
        self.sources = {}
        for name in ("tools/m2sndrecomp/main.cpp", "src/m68k/timing.cpp", "src/runtime/snd_cpu.cpp",
                     "src/runtime/snd_cpu.h", "src/runtime/snd_sched.h"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// synthetic current source\n")
            self.sources[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        self.generated = self.native / "snd_gen.cpp"
        self.generated.write_text("// synthetic generated test fixture\n")
        self.audit = {"romset": "srallycb", "board": "model2a", "sound_chip_attached": True,
            "native_scsp_startup_reached": True, "images_sha256": specification["images"],
            "sources_sha256": self.sources, "generated_sha256": hashlib.sha256(self.generated.read_bytes()).hexdigest(),
            "generation": {"board": "model2a", "timing_mode": "model2a-boundary", "live_code_guard": True,
                           "missing_timing": 0, "missing_templates": 0}}
        self.images = {name: self.build / (name + ".bin") for name in specification["images"]}
        # The separate importer tests validate actual asset layout/hashes; this
        # fixture isolates whether the runner can accidentally link wrong code.
        self.addCleanup(patch.stopall)
        patch.object(boot, "ROOT", self.root).start()
        patch("segarally_sound_recompile.verified_images", return_value=self.images).start()

    def link(self):
        (self.native / "audit.json").write_text(json.dumps(self.audit))
        return boot.sound_inputs(self.build, "srallycb")

    def test_valid_generation_uses_audited_program_and_assets(self):
        paths, generated = self.link()
        self.assertEqual(paths, [self.images["audiocpu"], self.images["samples"]])
        self.assertEqual(generated, self.generated)

    def test_rejects_untimed_unguarded_and_partial_generation(self):
        for key, value in (("timing_mode", "none"), ("live_code_guard", False),
                           ("missing_timing", 1), ("missing_templates", 1)):
            with self.subTest(key=key):
                original = self.audit["generation"][key]
                self.audit["generation"][key] = value
                with self.assertRaises(ValueError): self.link()
                self.audit["generation"][key] = original

    def test_rejects_changed_cpu_source_and_missing_provenance(self):
        path = self.root / "src/runtime/snd_cpu.cpp"
        path.write_text("// changed timing behavior\n")
        with self.assertRaises(ValueError): self.link()
        self.sources.pop("src/runtime/snd_cpu.cpp")
        with self.assertRaises(ValueError): self.link()

    def test_rejects_modified_generated_output(self):
        self.generated.write_text("// stale or modified generation\n")
        with self.assertRaises(ValueError): self.link()

    def test_rejects_unaudited_startup_and_wrong_revision(self):
        self.audit["native_scsp_startup_reached"] = False
        with self.assertRaises(ValueError): self.link()
        self.audit["native_scsp_startup_reached"] = True
        self.audit["romset"] = "different"
        with self.assertRaises(ValueError): self.link()

    def test_rejects_generated_symlink_outside_build(self):
        outside = self.root / "outside.cpp"
        outside.write_bytes(self.generated.read_bytes())
        self.generated.unlink()
        self.generated.symlink_to(outside)
        with self.assertRaises(ValueError): self.link()


if __name__ == "__main__":
    unittest.main()
