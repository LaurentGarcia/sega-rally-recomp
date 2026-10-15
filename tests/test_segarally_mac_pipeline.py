#!/usr/bin/env python3
"""Check candidate-build isolation and fail-closed behavior without game data."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("segarally_mac", ROOT / "scripts/segarally_mac.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class MacPipeline(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.roms = self.root / "personal archive.zip"
        self.roms.write_bytes(b"synthetic input only")
        self.output = self.root / "build/new candidate"
        self.patch = patch.object(pipeline, "ROOT", self.root)
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temp.cleanup()

    def test_existing_output_and_build_root_are_preserved(self):
        self.output.mkdir(parents=True)
        save = self.output / "save.bin"
        save.write_bytes(b"preserve existing save")
        for path in (self.output, self.root / "build", self.root / "elsewhere"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                pipeline.output_path(path)
        self.assertEqual(save.read_bytes(), b"preserve existing save")

    def test_symlink_cannot_escape_build(self):
        (self.root / "build").mkdir()
        (self.root / "outside").mkdir()
        (self.root / "build/link").symlink_to(self.root / "outside", target_is_directory=True)
        with self.assertRaises(ValueError):
            pipeline.output_path(self.root / "build/link/new")
        self.assertFalse((self.root / "outside/new").exists())

    def test_plan_is_read_only_and_preserves_argument_boundaries(self):
        with patch.object(pipeline.subprocess, "run") as run, \
             patch.object(pipeline.subprocess, "Popen") as popen, \
             contextlib.redirect_stdout(io.StringIO()) as stdout:
            self.assertEqual(pipeline.main(["--roms", str(self.roms), "--output",
                                          str(self.output), "--plan"]), 0)
        run.assert_not_called()
        popen.assert_not_called()
        self.assertFalse(self.output.exists())
        plan = json.loads(stdout.getvalue())
        self.assertIn(str(self.roms), plan[0]["command"])
        package = next(row for row in plan if row["name"] == "package")
        self.assertIn("--no-launch", package["command"])
        self.assertIn("--sound", package["command"])
        self.assertIn("--instruction-boundary-hook", package["command"])
        self.assertFalse(any(row["command"][0] == "open" for row in plan))

    def test_first_failed_step_stops_and_records_evidence(self):
        plan = [{"name": "compile", "command": ["compiler"], "timeout_seconds": 1},
                {"name": "package", "command": ["packager"], "timeout_seconds": 1}]
        def fail(step, log):
            log.write_text("synthetic compiler failure\n")
            raise subprocess.CalledProcessError(3, step["command"])
        with patch.object(pipeline, "run_step", side_effect=fail) as run, \
             contextlib.redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
            pipeline.execute(plan, self.output, {"schema": 1})
        self.assertEqual(run.call_count, 1)
        report = json.loads((self.output / "build-result.json").read_text())
        self.assertEqual(report["status"], "failed")
        self.assertEqual(len(report["steps"]), 1)
        self.assertEqual(report["steps"][0]["status"], "failed")
        self.assertIn("compiler failure", (self.output / report["steps"][0]["log"]).read_text())
        self.assertFalse((self.output / "mac").exists())

    def test_interruption_is_not_a_success(self):
        plan = [{"name": "compile", "command": ["compiler"], "timeout_seconds": 1}]
        with patch.object(pipeline, "run_step", side_effect=KeyboardInterrupt), \
             contextlib.redirect_stdout(io.StringIO()), self.assertRaises(KeyboardInterrupt):
            pipeline.execute(plan, self.output, {})
        report = json.loads((self.output / "build-result.json").read_text())
        self.assertEqual(report["status"], "failed")
        self.assertIn("KeyboardInterrupt", report["error"])

    def test_sound_frontier_does_not_reach_packaging(self):
        plan = [{"name": "sound", "command": ["sound-generator"], "timeout_seconds": 1},
                {"name": "package", "command": ["packager"], "timeout_seconds": 1}]
        def frontier(step, log):
            path = self.output / "native/sound/native-timed/audit.json"
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps({"native_scsp_startup_reached": True,
                                       "execution": {"status": "missing_code", "instructions": 17}}))
        with patch.object(pipeline, "run_step", side_effect=frontier) as run, \
             contextlib.redirect_stdout(io.StringIO()), self.assertRaises(ValueError):
            pipeline.execute(plan, self.output, {})
        self.assertEqual(run.call_count, 1)
        self.assertEqual(json.loads((self.output / "build-result.json").read_text())["status"], "failed")

    def test_timeout_reaps_build_process_group(self):
        child = Mock(pid=12345)
        child.wait.side_effect = [subprocess.TimeoutExpired(["compiler"], 1), None]
        child.poll.return_value = None
        step = {"command": ["compiler"], "timeout_seconds": 1}
        with patch.object(pipeline.subprocess, "Popen", return_value=child) as popen, \
             patch.object(pipeline.os, "killpg", create=True) as kill, self.assertRaises(subprocess.TimeoutExpired):
            pipeline.run_step(step, self.root / "compile.log")
        self.assertTrue(popen.call_args.kwargs["start_new_session"])
        kill.assert_called_once_with(12345, pipeline.signal.SIGTERM)
        self.assertEqual(child.wait.call_count, 2)

    def test_success_records_signed_executable_and_future_launch_only(self):
        plan = [{"name": "package", "command": ["packager"], "timeout_seconds": 1}]
        def package(step, log):
            app = self.output / "mac/Sega Rally Native.app/Contents"
            (app / "MacOS").mkdir(parents=True)
            (app / "Resources/assets").mkdir(parents=True)
            (app / "MacOS/segarally_native").write_bytes(b"synthetic executable")
            (app / "Resources/assets/assets.json").write_text("{}")
            (self.output / "mac/launch-command.json").write_text(json.dumps(
                {"command": ["open", "-n", "synthetic app"], "executed": False}))
        with patch.object(pipeline, "run_step", side_effect=package), contextlib.redirect_stdout(io.StringIO()):
            result = pipeline.execute(plan, self.output, {})
        self.assertEqual(result["status"], "packaged")
        self.assertFalse(result["launched"])
        self.assertEqual(len(result["executable_sha256"]), 64)
        self.assertEqual(result["launch_command"], ["open", "-n", "synthetic app"])
        self.assertEqual(json.loads((self.output / "build-result.json").read_text()), result)


if __name__ == "__main__":
    unittest.main()
