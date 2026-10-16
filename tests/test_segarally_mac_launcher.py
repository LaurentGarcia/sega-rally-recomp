#!/usr/bin/env python3
"""Exercise Mac packaging with synthetic assets and no OS subprocesses."""
import contextlib
import hashlib
import io
import json
import plistlib
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
sys.path.insert(0,str(ROOT/"platform/segarally"))
import build_icons


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
        (self.build / "licenses").mkdir()
        for name in ("Project-BSD-3-Clause.txt", "THIRD_PARTY.md", "SDL.txt", "ImGui.txt", "ImGui-embedded-fonts.txt", "Source-notices.txt"):
            (self.build / "licenses" / name).write_text("synthetic notice " + name)
        self.icon = self.build / "original-fixture.icns"
        self.icon.write_bytes(b"original synthetic icon fixture")
        self.calls = []
        self.sign_checks = None

    def tearDown(self):
        self.temp.cleanup()

    def tree(self):
        return {str(p.relative_to(self.build)): p.read_bytes() if p.is_file() else None
                for p in self.build.rglob("*")}

    def run_launcher(self, *arguments, active=False, process_error=None):
        def fake_run(command, **kwargs):
            self.calls.append((command, kwargs))
            if command[0] == "pgrep":
                self.assertEqual(command, ["pgrep", "-f", re.escape(str(
                    self.app / "Contents/MacOS/segarally_native"))])
                self.assertEqual(kwargs, {"capture_output": True, "text": True})
                return subprocess.CompletedProcess(command, process_error if process_error is not None else (0 if active else 1),
                                                   stdout="12345\n" if active else "",
                                                   stderr="process query failed" if process_error is not None else "")
            if command[0] == "codesign":
                self.assertEqual((self.resources/"AppIcon.icns").read_bytes(),self.icon.read_bytes())
                with (self.app/"Contents/Info.plist").open("rb") as file:
                    self.assertEqual(plistlib.load(file)["CFBundleIconFile"],"AppIcon.icns")
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

        with patch.object(build_icons,"build_icons",return_value=self.icon), patch.object(sys, "argv", [str(LAUNCHER), "--build-dir", str(self.build), *arguments]), \
             patch("subprocess.run", side_effect=fake_run), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            try:
                runpy.run_path(str(LAUNCHER), run_name="__main__")
            except SystemExit as exit:
                return exit.code
        return 0

    def test_packages_only_declared_images_and_replaces_stale_resources(self):
        old_assets = self.resources / "assets"
        old_assets.mkdir(parents=True)
        (old_assets / "obsolete.bin").write_bytes(b"stale")
        (self.build / "assets" / "private-diagnostic.bin").write_bytes(b"not declared")
        (self.resources / "licenses").mkdir()
        (self.resources / "licenses" / "obsolete.txt").write_bytes(b"stale notice")
        def check_package():
            expected = {"assets.json"} | {name.name for name in (self.build / "assets").glob("*.bin")
                                        if name.name != "private-diagnostic.bin"}
            self.assertEqual({p.name for p in old_assets.iterdir()}, expected)
            self.assertEqual({p.name for p in (self.resources / "licenses").iterdir()},
                             {p.name for p in (self.build / "licenses").iterdir()})
            for p in (self.build / "licenses").iterdir():
                self.assertEqual((self.resources / "licenses" / p.name).read_bytes(), p.read_bytes())
        self.sign_checks = check_package
        self.assertEqual(self.run_launcher("--no-launch"), 0)

    def test_symlink_image_rejected_before_mutation(self):
        image = self.build / "assets/program.bin"
        target = self.build / "outside.bin"
        image.rename(target)
        image.symlink_to(target)
        before = self.tree()
        self.assertEqual(self.run_launcher("--no-launch"), 2)
        self.assertEqual(self.tree(), before)
        self.assertFalse(self.calls)

    def test_missing_required_notice_rejected_before_mutation(self):
        (self.build / "licenses/Source-notices.txt").unlink()
        before = self.tree()
        self.assertEqual(self.run_launcher("--no-launch"), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_missing_embedded_font_notice_rejected_before_mutation(self):
        (self.build / "licenses/ImGui-embedded-fonts.txt").unlink()
        before = self.tree()
        self.assertEqual(self.run_launcher("--no-launch"), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_no_sound_package_omits_old_sound_files(self):
        audit_path = self.build / "assets/assets.json"
        audit = json.loads(audit_path.read_text())
        audit.pop("sound")
        for name in ("sound_program", "sound_samples"):
            audit["images"].pop(name)
        audit_path.write_text(json.dumps(audit))
        self.assertEqual(self.run_launcher("--no-launch"), 0)
        self.assertFalse((self.resources / "assets/sound_program.bin").exists())
        self.assertFalse((self.resources / "assets/sound_samples.bin").exists())
        self.assertTrue((self.build / "assets/sound_program.bin").exists())

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
        self.assertEqual(self.calls[-1][0][4:], ["--player", "--save-dir", str(self.build / "live-app-save"),
            "--log", str(self.build / "live-app.log"), "--unthrottled", "--sound",
            "--instruction-boundary-hook", "--input-replay", str(self.bundled.resolve())])

    def test_fullscreen_flag_preserved(self):
        self.assertEqual(self.run_launcher("--fullscreen"), 0)
        self.assertIn("--fullscreen", self.calls[-1][0])

    def test_paced_audio_diagnostics_forwarded(self):
        self.assertEqual(self.run_launcher("--sound", "--audio-diagnostics"), 0)
        self.assertIn("--audio-diagnostics", self.calls[-1][0])

    def test_invalid_audio_diagnostics_rejected_before_packaging(self):
        before = self.tree()
        for arguments in (("--audio-diagnostics",),
                          ("--audio-diagnostics", "--sound", "--unthrottled")):
            self.assertEqual(self.run_launcher(*arguments), 2)
            self.assertEqual(self.tree(), before)
        self.assertEqual(self.calls, [])

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

    def test_process_query_errors_preserve_package_and_never_launch(self):
        self.resources.mkdir(parents=True)
        self.bundled.write_bytes(b"preserved signed replay")
        before = self.tree()
        for code in (2, 3, -9):
            for arguments in ((), ("--no-launch",)):
                with self.subTest(code=code, arguments=arguments):
                    self.calls.clear()
                    self.assertEqual(self.run_launcher(*arguments, process_error=code), 2)
                    self.assertEqual(self.tree(), before)
                    self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])

    def test_no_launch_signs_and_records_exact_future_arguments(self):
        self.sign_checks = lambda: self.assertEqual(self.bundled.read_bytes(), self.data)
        self.assertEqual(self.run_launcher("--no-launch", "--sound", "--crt", "--accurate-textures",
            "--instruction-boundary-hook", "--input-replay", str(self.replay)), 0)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep", "codesign"])
        future = json.loads((self.build / "launch-command.json").read_text())
        self.assertEqual(future, {"executed": False, "command": ["open", "-n", str(self.app),
            "--args", "--player", "--save-dir", str(self.build / "live-app-save"),
            "--log", str(self.build / "live-app.log"), "--sound", "--crt", "--accurate-textures",
            "--instruction-boundary-hook", "--input-replay", str(self.bundled)]})
        self.assertFalse((self.build / "live-app-save").exists())
        self.assertFalse((self.build / "live-app.log").exists())

    def test_no_launch_rejects_recording_before_mutation(self):
        before = self.tree()
        self.assertEqual(self.run_launcher("--no-launch", "--record-inputs", str(self.build / "recording")), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual(self.calls, [])

    def test_no_launch_refuses_running_app_without_mutation_or_activation(self):
        self.resources.mkdir(parents=True)
        self.bundled.write_bytes(b"preserved signed replay")
        before = self.tree()
        self.assertEqual(self.run_launcher("--no-launch", active=True), 2)
        self.assertEqual(self.tree(), before)
        self.assertEqual([c[0][0] for c in self.calls], ["pgrep"])


if __name__ == "__main__":
    unittest.main()
