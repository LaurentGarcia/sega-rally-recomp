#!/usr/bin/env python3
"""Build and package a sound-enabled Mac candidate from a personal ROM archive."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DEPENDENCIES = {
    "sdl3": "fa2c02bb6e21974a89ea9824bc53c9932abe5f9c",
    "imgui": "f1cc2ae15e53a861a874c3034aae6798fde194ab",
    "lzma": "0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17",
}


def digest(path: Path) -> str:
    with path.open("rb") as source:
        result = hashlib.sha256()
        for block in iter(lambda: source.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def output_path(path: Path) -> Path:
    path = (ROOT / path).resolve()
    base = (ROOT / "build").resolve()
    if path == base or not path.is_relative_to(base):
        raise ValueError("output must be a new child directory inside this checkout's ignored build/")
    if path.exists():
        raise ValueError("output already exists; choose a new directory to preserve builds, recordings and saves")
    return path


def build_plan(roms: Path, output: Path, jobs: int) -> list[dict]:
    native, host = output / "native", output / "mac"
    python = sys.executable
    def script(name, *args):
        return [python, str(ROOT / name), *map(str, args)]
    def step(name, command, timeout=900):
        return {"name": name, "command": command, "timeout_seconds": timeout}
    return [
        step("main", script("scripts/segarally_recompile.py", "--roms", roms,
             "--build-dir", native, "--jobs", jobs, "--seed-profile", "lift-labels",
             "--indexed-dispatch", "--instruction-boundary-hook")),
        step("tgp", script("scripts/segarally_tgp.py", "--roms", roms, "--build-dir", native)),
        step("video", script("scripts/segarally_video.py", "--roms", roms, "--build-dir", native)),
        step("sound-import", script("scripts/segarally_sound.py", "--roms", roms, "--build-dir", native)),
        step("sound", script("scripts/segarally_sound_recompile.py", "--build-dir", native,
             "--timed", "--max-instructions", "1000000")),
        step("configure", ["cmake", "-S", str(ROOT / "platform/segarally"), "-B", str(host),
             "-DCMAKE_BUILD_TYPE=Release", f"-DSEGARALLY_NATIVE_BUILD={native}",
             f"-DSEGARALLY_SOUND_BUILD={native}", "-DSEGARALLY_SOUND=ON",
             "-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON", "-DSEGARALLY_INPUT_RECORDING=ON"]),
        step("compile", ["cmake", "--build", str(host), "--parallel", str(jobs), "--target",
             "segarally_native", "test_segarally_audio", "test_segarally_controls",
             "test_segarally_display_preferences"]),
        step("tests", ["ctest", "--test-dir", str(host), "--output-on-failure"], 300),
        step("package", script("platform/segarally/macos/launch.py", "--build-dir", host,
             "--no-launch", "--sound", "--instruction-boundary-hook",
             "--input-replay", ROOT / "config/segarally_lancia_select.inputs"), 120),
        step("signature", ["codesign", "--verify", "--deep", "--strict",
             str(host / "Sega Rally Native.app")], 60),
    ]


def preflight() -> dict:
    if platform.system() != "Darwin":
        raise ValueError("this entrypoint requires macOS and the Apple command-line toolchain")
    for tool in ("cmake", "ctest", "git", "c++", "codesign", "pgrep"):
        if not shutil.which(tool):
            raise ValueError(f"missing tool: {tool}; see docs/segarally-build-validation.md")
    revisions = {}
    for name, expected in DEPENDENCIES.items():
        directory = ROOT / "extern" / name
        result = subprocess.run(["git", "-C", str(directory), "rev-parse", "HEAD"],
                                check=True, capture_output=True, text=True)
        if result.stdout.strip() != expected:
            raise ValueError(f"extern/{name} must use {expected}; see the build guide")
        dirty = subprocess.run(["git", "-C", str(directory), "status", "--porcelain",
                                "--untracked-files=no"], check=True, capture_output=True, text=True)
        if dirty.stdout.strip():
            raise ValueError(f"extern/{name} has modified tracked files; preserve them before building")
        revisions[name] = expected
    return revisions


def run_step(step: dict, logfile: Path) -> None:
    # Reap the whole build group on timeout/interruption, including compilers
    # launched by a generation script. Never leave a failed build running.
    with logfile.open("wb") as log:
        child = subprocess.Popen(step["command"], cwd=ROOT, stdout=log,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = child.wait(timeout=step["timeout_seconds"])
            if code:
                raise subprocess.CalledProcessError(code, step["command"])
        except BaseException:
            try:
                os.killpg(child.pid, signal.SIGTERM)
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            except ProcessLookupError:
                child.wait()
            raise


def check_sound_startup(output: Path) -> None:
    # The diagnostic generator can intentionally return success for a discovered
    # native frontier. A packaged candidate must instead finish its startup budget.
    audit = json.loads((output / "native/sound/native-timed/audit.json").read_text())
    execution = audit["execution"]
    if (not audit.get("native_scsp_startup_reached") or
            execution.get("status") != "instruction_limit" or
            execution.get("instructions") != 1_000_000):
        raise ValueError("sound startup stopped before its full instruction budget; inspect the sound audit")


def execute(plan: list[dict], output: Path, report: dict) -> dict:
    output.mkdir(parents=True, exist_ok=False)
    logdir = output / "logs"
    logdir.mkdir()
    report.update(status="running", steps=[])
    destination = output / "build-result.json"
    def save():
        destination.write_text(json.dumps(report, indent=2) + "\n")
    save()
    try:
        for index, step in enumerate(plan, 1):
            log = logdir / f"{index:02d}-{step['name']}.log"
            row = dict(step, log=str(log.relative_to(output)), status="running")
            report["steps"].append(row)
            save()
            print(f"[{index}/{len(plan)}] {step['name']}: {log}", flush=True)
            start = time.monotonic()
            try:
                run_step(step, log)
                if step["name"] == "sound":
                    check_sound_startup(output)
                row["status"] = "passed"
            except BaseException as error:
                row.update(status="failed", error=f"{type(error).__name__}: {error}")
                raise
            finally:
                row["wall_seconds"] = time.monotonic() - start
                save()
        app = output / "mac/Sega Rally Native.app"
        report.update(status="packaged", app=str(app), launched=False,
                      executable_sha256=digest(app / "Contents/MacOS/segarally_native"),
                      assets_sha256=digest(app / "Contents/Resources/assets/assets.json"),
                      launch_command=json.loads((output / "mac/launch-command.json").read_text())["command"],
                      scope="Build, host regression checks and signature only; no gameplay or full-course validation.")
        return report
    except BaseException as error:
        report.update(status="failed", error=f"{type(error).__name__}: {error}")
        raise
    finally:
        save()


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, required=True, help="Personal Revision B ROM archive")
    parser.add_argument("--output", type=Path, required=True, help="New directory below build/")
    parser.add_argument("--jobs", type=int, default=4, choices=range(1, 9), metavar="1..8")
    parser.add_argument("--plan", action="store_true", help="Print commands without writing or executing them")
    args = parser.parse_args(argv)
    try:
        roms = args.roms.expanduser().resolve()
        if not roms.is_file():
            raise ValueError("personal ROM archive does not exist")
        output = output_path(args.output)
        plan = build_plan(roms, output, args.jobs)
        if args.plan:
            print(json.dumps(plan, indent=2))
            return 0
        dependencies = preflight()
        revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                  check=True, capture_output=True, text=True).stdout.strip()
        changes = subprocess.run(["git", "diff", "HEAD", "--"], cwd=ROOT,
                                 check=True, capture_output=True).stdout
        report = {"schema": 1, "source_revision": revision,
                  "source_diff_sha256": hashlib.sha256(changes).hexdigest(),
                  "entrypoint_sha256": digest(Path(__file__)),
                  "rom_archive_sha256": digest(roms), "dependencies": dependencies}
        result = execute(plan, output, report)
        print(f"Packaged: {result['app']}")
        print(f"Build report: {output / 'build-result.json'}")
        print(f"Launch: {shlex.join(result['launch_command'])}")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f"segarally_mac: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("segarally_mac: interrupted; failed output and logs preserved", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
