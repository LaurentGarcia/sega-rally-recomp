"""Exercise packaging arguments without codesign, LaunchServices, or a game."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / "platform/segarally/macos/launch.py"


class RecordingLauncherTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="launcher-recording-", dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name)
        (self.build / "segarally_native").write_bytes(b"synthetic executable")
        assets = self.build / "assets"
        assets.mkdir()
        images = {}
        for name in ("program", "main_data", "tgp_tables", "copro_data", "polygons", "textures"):
            data = name.encode()
            (assets / f"{name}.bin").write_bytes(data)
            images[name] = {"file": f"{name}.bin", "sha256": hashlib.sha256(data).hexdigest()}
        (assets / "assets.json").write_text(json.dumps({"romset": "srallycb", "images": images}))

    def run_launcher(self, recording, active=False, selection=None):
        args = [str(LAUNCHER), "--build-dir", str(self.build), "--record-inputs", str(recording)]
        if selection:
            args += ["--input-replay", str(selection)]
        calls = []

        def command(argv, **kwargs):
            calls.append(argv)
            status = (0 if active else 1) if argv[0] == "pgrep" else 0
            return subprocess.CompletedProcess(argv, status, "123\n" if argv[0] == "pgrep" and active else "", "")

        with patch.object(sys, "argv", args), patch("subprocess.run", command), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            try:
                runpy.run_path(str(LAUNCHER), run_name="__main__")
            except SystemExit as result:
                return result.code, calls
        return 0, calls

    def test_new_recording_preserves_selection_and_passes_exact_path(self):
        recording = self.build / "capture"
        selection = self.build / "selection.inputs"
        selection.write_text("segarally-input-v1\n0 ff 8f 00 ff 80 00 00 ff ff ff ff ff\n")
        status, calls = self.run_launcher(recording, selection=selection)
        self.assertEqual(status, 0)
        opened = next(c for c in calls if c[:2] == ["open", "-n"])
        self.assertEqual(opened[opened.index("--record-inputs") + 1], str(recording))
        bundled = Path(opened[opened.index("--input-replay") + 1])
        self.assertEqual(bundled.read_bytes(), selection.read_bytes())
        self.assertFalse(recording.exists())  # The host owns exclusive creation.

    def test_existing_app_refuses_new_recording_instead_of_ignoring_flag(self):
        status, calls = self.run_launcher(self.build / "capture", active=True)
        self.assertEqual(status, 2)
        self.assertTrue(all(c[0] == "pgrep" for c in calls))

    def test_existing_directory_is_preserved(self):
        recording = self.build / "capture"
        recording.mkdir()
        marker = recording / "keep"
        marker.write_text("preserved")
        status, calls = self.run_launcher(recording)
        self.assertEqual(status, 2)
        self.assertEqual(marker.read_text(), "preserved")
        self.assertTrue(all(c[0] == "pgrep" for c in calls))

    def test_recording_outside_ignored_build_is_rejected(self):
        status, calls = self.run_launcher(Path(tempfile.gettempdir()) / "outside-capture")
        self.assertEqual(status, 2)
        self.assertTrue(all(c[0] == "pgrep" for c in calls))


if __name__ == "__main__":
    unittest.main()
