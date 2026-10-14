#!/usr/bin/env python3
"""Exercise Mac packaging with synthetic assets and no OS subprocesses."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / "platform/segarally/macos/launch.py"


class MacLauncher(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="mac-launcher-test-", dir=ROOT / "build")
        self.build = Path(self.temp.name)
        (self.build / "segarally_native").write_bytes(b"synthetic executable")
        assets = self.build / "assets"
        assets.mkdir()
        audit = {"romset": "srallycb", "images": {}, "sound": {},
                 "instruction_boundary_hook": {"version": 1}}
        for name in ("program", "main_data", "tgp_tables", "copro_data", "polygons",
                     "textures", "sound_program", "sound_samples"):
            data = ("synthetic " + name).encode()
            (assets / (name + ".bin")).write_bytes(data)
            audit["images"][name] = {"file": name + ".bin",
                                     "sha256": hashlib.sha256(data).hexdigest()}
        (assets / "assets.json").write_text(json.dumps(audit))
        self.app = self.build / "Sega Rally Native.app"
        self.resources = self.app / "Contents/Resources"
        self.bundled = self.resources / "input-replay.inputs"
        self.replay = self.build / "authorized replay.inputs"
        self.data = b"# exact bytes, including CRLF\r\nsegarally-input-v1\r\n"
        self.replay.write_bytes(self.data)
        self.calls = []
        self.sign_checks = None

    def tearDown(self):
        self.temp.cleanup()

    def tree(self):
        return {str(p.relative_to(self.build)): p.read_bytes() if p.is_file() else None
                for p in self.build.rglob("*")}

    def run_launcher(self, *arguments, active=False):
        def fake_run(command, **kwargs):
            self.calls.append((command, kwargs))
            if command[0] == "pgrep":
                self.assertEqual(command, ["pgrep", "-f", re.escape(str(
                    self.app / "Contents/MacOS/segarally_native"))])
                self.assertEqual(kwargs, {"capture_output": True, "text": True})
                return subprocess.CompletedProcess(command, 0 if active else 1,
                                                   stdout="12345\n" if active else "", stderr="")
            if command[0] == "codesign":
                self.assertEqual(command, ["codesign", "--force", "--sign", "-", str(self.app)])
                self.assertEqual(kwargs, {"check": True})
                self.assertEqual((self.app / "Contents/MacOS/segarally_native").read_bytes(),
                                 b"synthetic executable")
                self.assertTrue((self.resources / "assets/assets.json").is_file())
                self.assertTrue((self.app / "Contents/Info.plist").is_file())
                if self.sign_checks:
                    self.sign_checks()
            else:
                self.assertEqual(command[0], "open", "unexpected OS subprocess requested")
                self.assertEqual(kwargs, {"check": True})
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")

        with patch.object(sys, "argv", [str(LAUNCHER), "--build-dir", str(self.build), *arguments]), \
             patch("subprocess.run", side_effect=fake_run), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            try:
                runpy.run_path(str(LAUNCHER), run_name="__main__")
            except SystemExit as exit:
                return exit.code
        return 0

    def test_replay_is_exact_and_inside_bundle_before_signing(self):
        self.sign_checks = lambda: self.assertEqual(self.bundled.read_bytes(), self.data)
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay)), 0)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep", "codesign", "open"])
        launch = self.calls[-1][0]
        self.assertEqual(launch[:4], ["open", "-n", str(self.app), "--args"])
        self.assertEqual(launch[-2:], ["--input-replay", str(self.bundled.resolve())])
        self.assertNotIn(str(self.replay), launch)
        self.assertEqual(self.replay.read_bytes(), self.data)

    def test_flags_and_save_log_paths_preserved_without_fullscreen(self):
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay), "--sound",
                                         "--instruction-boundary-hook", "--unthrottled"), 0)
        self.assertEqual(self.calls[-1][0][4:], ["--save-dir", str(self.build / "live-app-save"),
            "--log", str(self.build / "live-app.log"), "--unthrottled", "--sound",
            "--instruction-boundary-hook", "--input-replay", str(self.bundled.resolve())])

    def test_fullscreen_flag_preserved(self):
        self.assertEqual(self.run_launcher("--fullscreen"), 0)
        self.assertIn("--fullscreen", self.calls[-1][0])

    def test_missing_replay_rejected_before_mutation_or_launch(self):
        before = self.tree()
        self.assertEqual(self.run_launcher("--input-replay", str(self.build / "missing.inputs")), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_oversized_replay_rejected_before_mutation_or_launch(self):
        self.replay.write_bytes(b"x" * (1024 * 1024 + 1))
        before = self.tree()
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay)), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_size_limit_accepts_exactly_one_mib(self):
        self.replay.write_bytes(b"x" * (1024 * 1024))
        self.sign_checks = lambda: self.assertEqual(self.bundled.read_bytes(), self.replay.read_bytes())
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay)), 0)

    def test_directory_replay_rejected_without_package_mutation(self):
        before = self.tree()
        self.assertEqual(self.run_launcher("--input-replay", str(self.build / "assets")), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_new_replay_replaces_stale_symlink_without_changing_source(self):
        self.resources.mkdir(parents=True)
        self.bundled.symlink_to(self.replay)
        self.sign_checks = lambda: self.assertFalse(self.bundled.is_symlink())
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay)), 0)
        self.assertEqual(self.bundled.read_bytes(), self.data)
        self.assertEqual(self.replay.read_bytes(), self.data)

    def test_stale_replay_removed_before_signing_when_not_requested(self):
        self.resources.mkdir(parents=True)
        self.bundled.write_bytes(b"stale")
        self.sign_checks = lambda: self.assertFalse(self.bundled.exists())
        self.assertEqual(self.run_launcher(), 0)
        self.assertFalse(self.bundled.exists())
        self.assertNotIn("--input-replay", self.calls[-1][0])

    def test_running_app_activated_without_package_mutation(self):
        (self.app / "Contents/MacOS").mkdir(parents=True)
        self.resources.mkdir(parents=True)
        self.bundled.write_bytes(b"existing signed replay")
        (self.app / "Contents/MacOS/segarally_native").write_bytes(b"existing signed executable")
        before = self.tree()
        self.assertEqual(self.run_launcher("--input-replay", str(self.replay), active=True), 0)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep", "open"])
        self.assertEqual(self.calls[-1][0], ["open", str(self.app)])


if __name__ == "__main__":
    unittest.main()
