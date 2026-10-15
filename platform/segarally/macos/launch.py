#!/usr/bin/env python3
"""Package the built native host and launch it independently through LaunchServices."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[3]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir",type=Path,default=ROOT/"build/segarally-mac")
parser.add_argument("--no-launch",action="store_true",help="Package and sign only; record the launch command without opening a window")
parser.add_argument("--unthrottled",action="store_true",help="Disable board-time pacing for diagnostics")
parser.add_argument("--input-replay",type=Path,help="Optional cabinet input replay, then manual control")
parser.add_argument("--record-inputs",type=Path,help="Create a new recording bundle for exact same-executable offline replay")
parser.add_argument("--fullscreen",action="store_true",help="Launch in desktop fullscreen")
parser.add_argument("--sound",action="store_true",help="Enable the audited diagnostic SCSP sound board")
parser.add_argument("--audio-diagnostics",action="store_true",help="Log queue and wall timing during paced sound playback")
parser.add_argument("--crt",action="store_true",help="Start with the adjustable Arcade CRT display effect")
parser.add_argument("--original-display",action="store_true",help="Use the original display defaults for this session")
parser.add_argument("--accurate-textures",action="store_true",help="Enable the optional more accurate native texture interpolation")
parser.add_argument("--instruction-boundary-hook",action="store_true",help="Enable audited exact resident-chunk execution")
args=parser.parse_args()
if args.no_launch and args.record_inputs:
    parser.error("--no-launch and --record-inputs cannot be combined; recording requires a launched session")
if args.crt and args.original_display:
    parser.error("--crt and --original-display cannot be combined")
if args.audio_diagnostics and (not args.sound or args.unthrottled):
    parser.error("--audio-diagnostics requires paced --sound playback")
build=args.build_dir.resolve()
if not build.is_relative_to(ROOT/"build"):
    parser.error("build directory must be inside the ignored project build directory")
try:
    executable=build/"segarally_native"
    if not executable.is_file():raise ValueError("build the native Mac host first")
    assets=build/"assets"
    audit=json.loads((assets/"assets.json").read_text())
    expected={"program","main_data","tgp_tables","copro_data","polygons","textures"}
    if "sound" in audit:expected|={"sound_program","sound_samples"}
    if audit["romset"]!="srallycb" or set(audit["images"])!=expected:
        raise ValueError("matching verified native host assets required")
    if args.sound and "sound" not in audit:
        raise ValueError("--sound requires a host built with SEGARALLY_SOUND=ON and verified sound assets")
    if args.instruction_boundary_hook and audit.get("instruction_boundary_hook",{}).get("version")!=1:
        raise ValueError("--instruction-boundary-hook requires an audited hook-enabled host")
    if assets.is_symlink() or (assets/"assets.json").is_symlink():
        raise ValueError("verified assets must be regular files, not symbolic links")
    for name,entry in audit["images"].items():
        if entry["file"]!=name+".bin" or (assets/entry["file"]).is_symlink() or hashlib.sha256((assets/entry["file"]).read_bytes()).hexdigest()!=entry["sha256"]:
            raise ValueError(f"{name} differs from the verified host package")
    app=build/"Sega Rally Native.app"
    binary=app/"Contents/MacOS"
    active=subprocess.run(["pgrep","-f",re.escape(str(binary/"segarally_native"))],capture_output=True,text=True)
    if active.returncode not in (0,1):
        raise ValueError(f"cannot check whether the app is running (pgrep exit {active.returncode}): {active.stderr.strip()}")
    if active.returncode==0:
        if args.no_launch:raise ValueError("close the active app before packaging with --no-launch")
        if args.record_inputs:raise ValueError("close the active app before starting a new recorded session; recording requires startup input and saves")
        subprocess.run(["open",str(app)],check=True)
        print(f"Activated existing native app, PID(s) {active.stdout.strip()}")
        raise SystemExit(0)
    license_source=build/"licenses"
    required_notices={"Project-BSD-3-Clause.txt", "THIRD_PARTY.md", "SDL.txt", "ImGui.txt", "ImGui-embedded-fonts.txt", "Source-notices.txt"}
    if license_source.is_symlink() or any(not (license_source/name).is_file() or
            (license_source/name).is_symlink() for name in required_notices):
        raise ValueError("rebuild the host to retain required distribution notices before packaging")
    recording_path=None
    if args.record_inputs:
        recording_path=args.record_inputs.resolve()
        if not recording_path.is_relative_to(ROOT/"build") or recording_path.exists():
            raise ValueError("recording directory must be new and inside ignored build/")
    replay_bytes=None
    if args.input_replay:
        replay_path=args.input_replay.resolve()
        if not replay_path.is_file():raise ValueError("input replay must be a regular file")
        with replay_path.open("rb") as file:replay_bytes=file.read(1024*1024+1)
        if len(replay_bytes)>1024*1024:raise ValueError("input replay exceeds 1 MiB")
    binary.mkdir(parents=True,exist_ok=True)
    shutil.copy2(executable,binary/"segarally_native")
    # SDL_GetBasePath points at Contents/Resources for a macOS app bundle.
    resources=app/"Contents/Resources";resources.mkdir(parents=True,exist_ok=True)
    # Only the audited manifest and declared images belong in the package.
    # A reused host directory can contain stale sound images or diagnostic files.
    packaged_assets=resources/"assets"
    if packaged_assets.is_symlink():packaged_assets.unlink()
    elif packaged_assets.exists():shutil.rmtree(packaged_assets)
    packaged_assets.mkdir()
    for name in ("assets.json", *(entry["file"] for entry in audit["images"].values())):
        shutil.copy2(assets/name,packaged_assets/name)
    packaged_licenses=resources/"licenses"
    if packaged_licenses.is_symlink():packaged_licenses.unlink()
    elif packaged_licenses.exists():shutil.rmtree(packaged_licenses)
    if (build/"licenses").is_dir():
        shutil.copytree(build/"licenses",packaged_licenses)
    # LaunchServices apps read their own signed resources at startup. Do not
    # depend on a separate workspace file being accessible before SDL starts.
    bundled_replay=resources/"input-replay.inputs"
    bundled_replay.unlink(missing_ok=True)
    if replay_bytes is not None:bundled_replay.write_bytes(replay_bytes)
    with (app/"Contents/Info.plist").open("wb") as file:
        plistlib.dump({"CFBundleIdentifier":"com.lgarciacarro.segarally.macnative",
            "CFBundleName":"Sega Rally Native","CFBundleDisplayName":"Sega Rally Native",
            "CFBundlePackageType":"APPL","CFBundleExecutable":"segarally_native",
            "CFBundleShortVersionString":"0.1","CFBundleVersion":"1",
            "NSHighResolutionCapable":True,"NSPrincipalClass":"NSApplication"},file)
    subprocess.run(["codesign","--force","--sign","-",str(app)],check=True)
    startup=["--player","--save-dir",str(build/"live-app-save"),"--log",str(build/"live-app.log")]
    if args.unthrottled:startup.append("--unthrottled")
    if args.fullscreen:startup.append("--fullscreen")
    if args.sound:startup.append("--sound")
    if args.audio_diagnostics:startup.append("--audio-diagnostics")
    if args.crt:startup.append("--crt")
    if args.original_display:startup.append("--original-display")
    if args.accurate_textures:startup.append("--accurate-textures")
    if args.instruction_boundary_hook:startup.append("--instruction-boundary-hook")
    if replay_bytes is not None:startup += ["--input-replay",str(bundled_replay)]
    if recording_path is not None:startup += ["--record-inputs",str(recording_path)]
    launch=["open","-n",str(app),"--args",*startup]
    if args.no_launch:
        command_path=build/"launch-command.json"
        command_path.write_text(json.dumps({"command":launch,"executed":False},indent=2)+"\n")
        print(f"Packaged {app} without launch; future command {command_path}")
    else:
        subprocess.run(launch,check=True)
        print(f"Opened {app}; startup log {build/'live-app.log'}")
except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
    parser.exit(2,f"launch: {error}\n")
