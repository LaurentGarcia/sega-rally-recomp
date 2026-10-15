#!/usr/bin/env python3
"""Build a ROM-free, ad-hoc-signed Apple Silicon importer. Never launches it."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command):
    subprocess.run(list(map(str, command)), check=True, timeout=180)

def verify_dmg(path, attempts=3):
    # DiskImages can briefly retain a just-created image on hosted Macs.
    # Retry only its explicit transient resource error; checksum failures fail.
    for attempt in range(1, attempts + 1):
        result = subprocess.run(['hdiutil', 'verify', str(path)], capture_output=True,
                                text=True, timeout=180)
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        (path.parent / f'verify-{attempt}.log').write_text(result.stdout + result.stderr)
        if result.returncode == 0:
            return attempt
        if 'Resource temporarily unavailable' not in result.stderr or attempt == attempts:
            raise subprocess.CalledProcessError(result.returncode, result.args,
                                                result.stdout, result.stderr)
        time.sleep(attempt)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--dmg', action='store_true')
    args = parser.parse_args()
    out = args.output.expanduser().resolve()
    if out.exists():
        parser.error('output must be a new directory; previous packages are preserved')
    if sys.platform != 'darwin':
        parser.error('requires macOS and Apple Command Line Tools')
    source_hashes = {name: sha(HERE / name) for name in ('ImporterApp.swift', 'coordinator.py', 'package.py')}
    out.mkdir(parents=True)
    app = out / 'Sega Rally Local Builder.app'
    contents = app / 'Contents'
    binary = contents / 'MacOS/segarally_importer'
    resources = contents / 'Resources'
    binary.parent.mkdir(parents=True)
    resources.mkdir()
    sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    run(['xcrun', 'swiftc', '-parse-as-library', '-O', '-target', 'arm64-apple-macos13.0', '-sdk', sdk, '-module-cache-path', out / 'swift-cache',
         HERE / 'ImporterApp.swift', '-framework', 'AppKit', '-framework', 'SwiftUI', '-o', binary])
    shutil.copyfile(HERE / 'coordinator.py', resources / 'coordinator.py')
    # Only original bootstrap sources and their project license, never the game
    # source tree, curated screenshots, generated programs or game images.
    source = resources / 'BootstrapSource'
    source.mkdir()
    for name in ('ImporterApp.swift', 'coordinator.py', 'package.py'):
        shutil.copyfile(HERE / name, source / name)
    shutil.copyfile(ROOT / 'LICENSE', resources / 'LICENSE.txt')
    (resources / 'README.txt').write_text('ROM-free local builder. Apple Silicon / macOS 13+.\n'
        'Ad-hoc signed; not notarized. No game files or game screenshots included.\n'
        'Requires Git, Python 3.10+, CMake 3.25+ and Apple Command Line Tools.\n'
        'Downloads pinned public source and dependencies only after you choose your own ZIP.\n'
        'All game verification, generation and compilation run locally. No ROM upload.\n')
    with (contents / 'Info.plist').open('wb') as f:
        plistlib.dump({'CFBundleExecutable': binary.name, 'CFBundleIdentifier': 'org.segarally.native.local-builder',
          'CFBundleName': 'Sega Rally Local Builder', 'CFBundlePackageType': 'APPL',
          'CFBundleShortVersionString': '0.1.0', 'CFBundleVersion': '1', 'LSMinimumSystemVersion': '13.0',
          'NSHighResolutionCapable': True, 'NSHumanReadableCopyright': 'See Resources/LICENSE.txt.'}, f)
    run(['codesign', '--force', '--sign', '-', app])
    run(['codesign', '--verify', '--deep', '--strict', app])
    inventory = {str(p.relative_to(app)): {'size': p.stat().st_size, 'sha256': sha(p)}
                 for p in sorted(app.rglob('*')) if p.is_file()}
    result = {'schema': 1, 'app': str(app), 'architecture': 'arm64', 'notarized': False,
              'game_payload_included': False, 'launched': False, 'files': inventory,
              'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'source_files': {name: sha(HERE / name) for name in ('ImporterApp.swift', 'coordinator.py', 'package.py')}}
    if args.dmg:
        stage = out / 'dmg-stage'
        stage.mkdir()
        shutil.copytree(app, stage / app.name, symlinks=True)
        (stage / 'Applications').symlink_to('/Applications')
        dmg = out / 'Sega-Rally-Local-Builder-macOS-arm64.dmg'
        run(['hdiutil', 'create', '-volname', 'Sega Rally Local Builder', '-srcfolder', stage,
             '-format', 'UDZO', '-ov', dmg])
        result['dmg_verify_attempts'] = verify_dmg(dmg)
        result['dmg'] = {'path': str(dmg), 'size': dmg.stat().st_size, 'sha256': sha(dmg)}
    if source_hashes != {name: sha(HERE / name) for name in source_hashes}:
        raise RuntimeError('Bootstrap sources changed during packaging; preserved output is not a candidate')
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(out / 'result.json')

if __name__ == '__main__':
    main()
