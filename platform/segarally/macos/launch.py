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
parser.add_argument("--unthrottled",action="store_true",help="Disable board-time pacing for diagnostics")
parser.add_argument("--input-replay",type=Path,help="Optional cabinet input replay, then manual control")
parser.add_argument("--fullscreen",action="store_true",help="Launch in desktop fullscreen")
args=parser.parse_args()
build=args.build_dir.resolve()
if not build.is_relative_to(ROOT/"build"):
    parser.error("build directory must be inside the ignored project build directory")
try:
    executable=build/"segarally_native"
    if not executable.is_file():raise ValueError("build the native Mac host first")
    assets=build/"assets"
    audit=json.loads((assets/"assets.json").read_text())
    if audit["romset"]!="srallycb" or set(audit["images"])!={"program","main_data","tgp_tables","copro_data","polygons","textures"}:
        raise ValueError("matching verified native host assets required")
    for name,entry in audit["images"].items():
        if entry["file"]!=name+".bin" or hashlib.sha256((assets/entry["file"]).read_bytes()).hexdigest()!=entry["sha256"]:
            raise ValueError(f"{name} differs from the verified host package")
    app=build/"Sega Rally Native.app"
    binary=app/"Contents/MacOS";binary.mkdir(parents=True,exist_ok=True)
    active=subprocess.run(["pgrep","-f",re.escape(str(binary/"segarally_native"))],capture_output=True,text=True)
    if active.returncode==0:
        subprocess.run(["open",str(app)],check=True)
        print(f"Activated existing native app, PID(s) {active.stdout.strip()}")
        raise SystemExit(0)
    shutil.copy2(executable,binary/"segarally_native")
    # SDL_GetBasePath points at Contents/Resources for a macOS app bundle.
    resources=app/"Contents/Resources";resources.mkdir(parents=True,exist_ok=True)
    shutil.copytree(assets,resources/"assets",dirs_exist_ok=True)
    with (app/"Contents/Info.plist").open("wb") as file:
        plistlib.dump({"CFBundleIdentifier":"com.lgarciacarro.segarally.macnative",
            "CFBundleName":"Sega Rally Native","CFBundleDisplayName":"Sega Rally Native",
            "CFBundlePackageType":"APPL","CFBundleExecutable":"segarally_native",
            "CFBundleShortVersionString":"0.1","CFBundleVersion":"1",
            "NSHighResolutionCapable":True,"NSPrincipalClass":"NSApplication"},file)
    subprocess.run(["codesign","--force","--sign","-",str(app)],check=True)
    startup=["--save-dir",str(build/"live-app-save"),"--log",str(build/"live-app.log")]
    if args.unthrottled:startup.append("--unthrottled")
    if args.fullscreen:startup.append("--fullscreen")
    if args.input_replay:startup += ["--input-replay",str(args.input_replay.resolve())]
    subprocess.run(["open","-n",str(app),"--args",*startup],check=True)
    print(f"Opened {app}; startup log {build/'live-app.log'}")
except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
    parser.exit(2,f"launch: {error}\n")
