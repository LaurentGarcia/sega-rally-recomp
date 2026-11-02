#!/usr/bin/env python3
"""Run a Play Qt preview with an isolated, explicitly sized presentation.

The raw mode matches the logical 496:384 image. The 4:3 mode is a separate
presentation choice, not a calibration of physical PS2/arcade CRT pixels.
This changes emulator presentation only; the supplied ELF is not modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def existing(value: str, directory: bool = False) -> Path:
    path = Path(value).expanduser().resolve(strict=True)
    if not (path.is_dir() if directory else path.is_file()):
        raise ValueError(f"Expected {'directory' if directory else 'file'}: {path}")
    return path


def prepare(args: argparse.Namespace) -> tuple[Path, list[str], dict]:
    emulator = existing(args.emulator)
    elf = existing(args.elf)
    host = existing(args.host, directory=True)
    if not os.access(emulator, os.X_OK):
        raise ValueError("The emulator must be executable")
    if not 120 <= args.height <= 2160 or not 0 <= args.status_bar_height <= 128:
        raise ValueError("Height must be 120..2160; status-bar height must be 0..128")
    numerator, denominator = (31, 24) if args.mode == "raw" else (4, 3)
    if args.height * numerator % denominator:
        raise ValueError("Height must give an integer canvas width at the selected aspect")
    if args.seconds != 0 and not 5 <= args.seconds <= 300:
        raise ValueError("Duration must be 0 (until closed) or 5..300 seconds")
    width = args.height * numerator // denominator
    raw_output = Path(args.output).expanduser().absolute()
    if raw_output.is_symlink():
        raise ValueError("Output must not be a symlink")
    output = raw_output.resolve()
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError("Output must be a new or empty directory")
    if output == host or output in host.parents:
        raise ValueError("Output must not contain the existing host directory")
    output.mkdir(parents=True, exist_ok=True)
    data = output / "Play Data Files"
    data.mkdir()
    config = ET.Element("Config")

    def preference(name: str, kind: str, value: object) -> None:
        ET.SubElement(config, "Preference", Name=name, Type=kind, Value=str(value))

    preference("renderer.presentationmode", "integer", 0)  # Pinned Play FILL.
    preference("video.gshandler", "integer", 1 if args.renderer == "vulkan" else 0)
    for name, value in (
        ("renderer.widescreen", "false"),
        ("renderer.ramreads.enabled", "true"),
        ("ui.pausewhenfocuslost", "false"),
        ("ui.showexitconfirmation", "false"),
        ("iop.fileio.stdlogging", "true"),
        ("ps2.limitframerate", "true"),
    ):
        preference(name, "boolean", value)
    preference("ps2.host.directory.v2", "path", host)
    for device, name in (
        ("rom0", "ps2.rom0.directory.v2"),
        ("mc0", "ps2.mc0.directory.v2"),
        ("mc1", "ps2.mc1.directory.v2"),
        ("hdd", "ps2.hdd.directory"),
    ):
        folder = data / "vfs" / device
        folder.mkdir(parents=True)
        preference(name, "path", folder)
    preference("ps2.arcaderoms.directory", "path", data / "arcaderoms")
    ET.indent(config)
    config_path = data / "config.xml"
    ET.ElementTree(config).write(config_path, encoding="unicode")
    (output / "portable.txt").write_text("Isolated Play developer preview\n")
    geometry = f"{width}x{args.height + args.status_bar_height}+40+40"
    command = [str(emulator), "-qwindowgeometry", geometry, "--elf", str(elf)]
    report = {
        "status": "prepared_extent_unverified",
        "scope": __doc__.strip(),
        "mode": args.mode,
        "intended_canvas_points": [width, args.height],
        "status_bar_height_points": args.status_bar_height,
        "qt_client_geometry": geometry,
        "command": command,
        "cwd": str(output),
        "host_directory": str(host),
        "emulator_sha256": digest(emulator),
        "elf_sha256": digest(elf),
        "initial_config_sha256": digest(config_path),
        "launcher_sha256": digest(Path(__file__)),
        "note": "Qt geometry excludes the title bar but includes the status bar. OS sizing, HiDPI and platform styles can change the resulting canvas; measure it before claiming aspect parity.",
    }
    with (output / "preview.json").open("x") as target:
        json.dump(report, target, indent=2)
        target.write("\n")
    return output, command, report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", required=True, help="Existing Play Qt executable")
    parser.add_argument("--elf", required=True, help="Existing locally built ELF")
    parser.add_argument("--host", required=True, help="Existing host: directory; no assets are copied")
    parser.add_argument("--output", required=True, help="New or empty private output directory")
    parser.add_argument("--mode", choices=("raw", "4:3"), default="raw")
    parser.add_argument("--height", type=int, default=576, help="GS content height in Qt logical points")
    parser.add_argument("--status-bar-height", type=int, default=27, help="Qt status bar and divider logical points; measured 27 on the tested macOS profile")
    parser.add_argument("--renderer", choices=("vulkan", "opengl"), default="vulkan")
    parser.add_argument("--seconds", type=int, default=0, help="0 waits until the window closes; 5..300 bounds the run")
    parser.add_argument("--prepare-only", action="store_true", help="Write isolated config and exact command without launching")
    args = parser.parse_args()
    try:
        output, command, report = prepare(args)
        print(json.dumps(report, indent=2), flush=True)
        if args.prepare_only:
            return 0
        start = time.monotonic()
        process = None
        try:
            with (output / "emulator.log").open("xb") as log:
                process = subprocess.Popen(command, cwd=output, stdin=subprocess.DEVNULL,
                                           stdout=log, stderr=subprocess.STDOUT)
                report["pid"] = process.pid
                try:
                    if args.seconds:
                        process.wait(timeout=args.seconds)
                    else:
                        process.wait()
                except subprocess.TimeoutExpired:
                    report["stopped_at_duration"] = True
                except KeyboardInterrupt:
                    report["stopped_by_interrupt"] = True
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=5)
        finally:
            report["elapsed_seconds"] = time.monotonic() - start
            report["returncode"] = process.returncode if process else None
            report["status"] = "observed_extent_unverified" if process else "launch_failed"
            report["final_config_sha256"] = digest(output / "Play Data Files/config.xml")
            with (output / "runtime.json").open("x") as target:
                json.dump(report, target, indent=2)
                target.write("\n")
        if report.get("stopped_by_interrupt"):
            return 130
        return 0 if process and (process.returncode == 0 or report.get("stopped_at_duration")) else 1
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.error(str(error))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
