#!/usr/bin/env python3
"""Verify native-host packaging rejects stale audits and escaped/tampered images."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

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


if __name__=="__main__":unittest.main()
