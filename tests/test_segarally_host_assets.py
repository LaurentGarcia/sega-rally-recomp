#!/usr/bin/env python3
"""Verify native-host packaging rejects stale audits and escaped/tampered images."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("stage_data",ROOT/"platform/segarally/stage_data.py")
stage_data=importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage_data)


class HostAssets(unittest.TestCase):
    def setUp(self):
        (ROOT/"build").mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix="host-assets-test-",dir=ROOT/"build")
        self.base=Path(self.temp.name)
        self.source=self.base/"native";self.target=self.base/"assets"
        self.source.mkdir();(self.source/"tgp").mkdir();(self.source/"video").mkdir()
        self.audits=[{"romset":"srallycb","images":{}} for _ in range(3)]
        self.audits[0].update(board="model2a",native_objects_compiled=True,
            reachability={"reference_cycle_accounting":True,"reference_cycle_accounting_version":1})
        self.audits[1]["native_code_compiled"]=True
        self.audits[2]["manifest_sha256"]=hashlib.sha256((ROOT/"config/segarally_video.json").read_bytes()).hexdigest()
        for i,names in enumerate((("program","main_data"),("copro_tgp_tables","copro_data"),("polygons","textures"))):
            for name in names:
                path=self.source/(name+".bin");path.write_bytes(name.encode())
                self.audits[i]["images"][name]={"path":str(path.relative_to(ROOT)),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
        self.write_audits()
    def tearDown(self):self.temp.cleanup()
    def write_audits(self):
        for name,audit in zip(("coverage.json","tgp/audit.json","video/audit.json"),self.audits):
            (self.source/name).write_text(json.dumps(audit))
    def rejects(self):
        self.write_audits()
        with self.assertRaises(ValueError):stage_data.stage(self.source,self.target)
        self.assertFalse(self.target.exists())
    def test_stages_verified_images(self):
        stage_data.stage(self.source,self.target)
        self.assertEqual((self.target/"tgp_tables.bin").read_bytes(),b"copro_tgp_tables")
        report=json.loads((self.target/"assets.json").read_text())
        self.assertEqual(len(report["images"]),6);self.assertTrue(report["native_coverage_is_partial"])
    def test_tampered_image(self):
        (self.source/"program.bin").write_bytes(b"changed");self.rejects()
    def test_escaped_image(self):
        path=self.base/"outside.bin";path.write_bytes(b"program")
        self.audits[0]["images"]["program"]["path"]=str(path.relative_to(ROOT));self.rejects()
    def test_stale_video_manifest(self):
        self.audits[2]["manifest_sha256"]="0"*64;self.rejects()
    def test_untimed_native_code(self):
        self.audits[0]["reachability"]["reference_cycle_accounting"]=False;self.rejects()
    def test_uncompiled_tgp(self):
        self.audits[1]["native_code_compiled"]=False;self.rejects()
    def test_external_destination(self):
        with self.assertRaises(ValueError):stage_data.stage(self.source,ROOT/"assets")

    def test_boundary_staging_records_audited_opt_in_support(self):
        # The independent boundary audit suite verifies source/header/object
        # mutations. This fixture checks packaging consumes that verifier and
        # advertises compile support without enabling the runtime by default.
        with patch.object(stage_data,"verify_boundary_contract",return_value={"version":1}) as verify:
            stage_data.stage(self.source,self.target,instruction_boundary_hook=True)
        verify.assert_called_once_with(self.source.resolve(),self.audits[0],verify_objects=False)
        report=json.loads((self.target/"assets.json").read_text())
        self.assertEqual(report["instruction_boundary_hook"],{"version":1,
            "runtime_enabled_by_default":False,
            "coverage_sha256":hashlib.sha256((self.source/"coverage.json").read_bytes()).hexdigest()})
        self.assertEqual(len(report["images"]),6)

    def test_boundary_build_and_staging_support_must_match(self):
        for contract,enabled in ((None,True),({"version":1},False)):
            with self.subTest(contract=contract,enabled=enabled), \
                 patch.object(stage_data,"verify_boundary_contract",return_value=contract):
                with self.assertRaisesRegex(ValueError,"boundary support must match"):
                    stage_data.stage(self.source,self.target,instruction_boundary_hook=enabled)
                self.assertFalse(self.target.exists())

    def test_rejected_boundary_audit_prevents_any_asset_copy(self):
        with patch.object(stage_data,"verify_boundary_contract",side_effect=ValueError("stale native objects")):
            with self.assertRaises(ValueError):
                stage_data.stage(self.source,self.target,instruction_boundary_hook=True)
        self.assertFalse(self.target.exists())

    def sound_fixture(self):
        source=self.base/"sound-build"
        native=source/"sound/native-timed";native.mkdir(parents=True)
        generated=native/"snd_gen.cpp";generated.write_text("// synthetic native fixture\n")
        (native/"audit.json").write_text('{"synthetic":true}\n')
        images=[source/"audiocpu.bin",source/"samples.bin"]
        for i,path in enumerate(images):path.write_bytes(bytes([i,2,3,4]))
        return source,images,generated

    def test_sound_staging_records_verified_generation_and_both_images(self):
        source,images,generated=self.sound_fixture()
        # Detailed source/import/guard rejection is exercised independently by
        # test_segarally_sound_link.py; this checks the platform's audit handoff.
        with patch.object(stage_data,"sound_inputs",return_value=(images,generated)) as verify:
            stage_data.stage(self.source,self.target,source)
        verify.assert_called_once_with(source.resolve(),"srallycb")
        report=json.loads((self.target/"assets.json").read_text())
        self.assertEqual(len(report["images"]),8)
        self.assertEqual((self.target/"sound_program.bin").read_bytes(),images[0].read_bytes())
        self.assertEqual((self.target/"sound_samples.bin").read_bytes(),images[1].read_bytes())
        self.assertEqual(report["sound"]["generated_sha256"],hashlib.sha256(generated.read_bytes()).hexdigest())
        self.assertEqual(report["sound"]["timing"],"instruction_boundary")
        self.assertFalse(report["sound"]["bus_phase_verified"])

    def test_rejected_sound_audit_prevents_any_asset_copy(self):
        with patch.object(stage_data,"sound_inputs",side_effect=ValueError("stale native generation")):
            with self.assertRaises(ValueError):stage_data.stage(self.source,self.target,self.base/"sound-build")
        self.assertFalse(self.target.exists())

    def test_sound_image_must_remain_inside_its_audited_build(self):
        source,images,generated=self.sound_fixture()
        images[0]=self.source/"program.bin"
        with patch.object(stage_data,"sound_inputs",return_value=(images,generated)):
            with self.assertRaises(ValueError):stage_data.stage(self.source,self.target,source)
        self.assertFalse(self.target.exists())


if __name__=="__main__":unittest.main()
