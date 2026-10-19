#!/usr/bin/env python3
"""Build a personal native iOS app or archive; never install, launch or upload it."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import signal
import subprocess
import sys
import time

from segarally_mac import DEPENDENCIES

ROOT = Path(__file__).resolve().parents[1]


def output_path(path: Path) -> Path:
    output = (ROOT / path).resolve()
    if output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        raise ValueError('output must be a new child of the ignored build/ directory')
    if output.exists():
        raise ValueError('output already exists; choose a new directory to preserve apps and saves')
    return output


def validate(args) -> None:
    if args.signing == 'development' and not re.fullmatch(r'[A-Z0-9]{10}', args.team or ''):
        raise ValueError('development signing requires a ten-character --team identifier')
    if args.signing == 'unsigned' and (args.team or args.allow_provisioning_updates):
        raise ValueError('unsigned builds do not use a team or provisioning updates')
    if args.unsigned_ipa and args.signing != 'unsigned':
        raise ValueError('--unsigned-ipa requires unsigned signing mode')
    if args.export_method and (not args.archive or args.signing != 'development'):
        raise ValueError('IPA export requires --archive and development signing')
    if not re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+', args.bundle_id):
        raise ValueError('bundle identifier must use reverse-domain components')
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', args.version):
        raise ValueError('version must contain one to three decimal components')
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', args.build_number):
        raise ValueError('build number must contain one to three decimal components')


def build_plan(args, output: Path) -> list[dict]:
    native = args.native_build.resolve() if args.native_build else output / 'native'
    host = output / 'ios'
    def step(name, argv, timeout=1800):
        return {'name': name, 'command': list(map(str, argv)), 'timeout_seconds': timeout}
    def script(name, *values):
        return [sys.executable, ROOT / name, *values]
    plan = []
    if args.roms:
        plan.extend([
            step('main', script('scripts/segarally_recompile.py', '--roms', args.roms.resolve(),
                 '--build-dir', native, '--jobs', args.jobs, '--seed-profile', 'lift-labels',
                 '--indexed-dispatch', '--instruction-boundary-hook')),
            step('tgp', script('scripts/segarally_tgp.py', '--roms', args.roms.resolve(), '--build-dir', native)),
            step('video', script('scripts/segarally_video.py', '--roms', args.roms.resolve(), '--build-dir', native)),
            step('sound-import', script('scripts/segarally_sound.py', '--roms', args.roms.resolve(), '--build-dir', native)),
            step('sound', script('scripts/segarally_sound_recompile.py', '--build-dir', native, '--timed')),
        ])
    configure = ['cmake', '-S', ROOT / 'platform/segarally', '-B', host, '-G', 'Xcode',
        '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphoneos',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0',
        f'-DSEGARALLY_NATIVE_BUILD={native}', f'-DSEGARALLY_SOUND_BUILD={native}',
        '-DSEGARALLY_SOUND=ON', '-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON',
        f'-DSEGARALLY_NATIVE_METAL={"ON" if args.graphics == "metal" else "OFF"}',
        '-DSEGARALLY_INPUT_RECORDING=OFF', f'-DSEGARALLY_IOS_BUNDLE_ID={args.bundle_id}',
        f'-DSEGARALLY_IOS_VERSION={args.version}', f'-DSEGARALLY_IOS_BUILD_NUMBER={args.build_number}',
        f'-DSEGARALLY_IOS_SIGNING={"ON" if args.signing == "development" else "OFF"}']
    if args.team:
        configure.append(f'-DSEGARALLY_IOS_TEAM={args.team}')
    plan.append(step('configure', configure))
    command = ['xcodebuild', '-project', host / 'SegaRallyNative.xcodeproj',
        '-scheme', 'segarally_native', '-configuration', 'Release',
        '-destination', f'platform=iOS,id={args.device}' if args.device else 'generic/platform=iOS',
        '-jobs', args.jobs]
    if args.allow_provisioning_updates:
        command.append('-allowProvisioningUpdates')
    if args.signing == 'unsigned':
        command.extend(['CODE_SIGNING_ALLOWED=NO', 'CODE_SIGNING_REQUIRED=NO'])
    if args.archive:
        command.extend(['-archivePath', output / 'SegaRallyNative.xcarchive', 'archive'])
    else:
        command.append('build')
    plan.append(step('archive' if args.archive else 'build', command))
    if args.signing == 'development':
        app = (output / 'SegaRallyNative.xcarchive/Products/Applications/segarally_native.app'
               if args.archive else host / 'Release-iphoneos/segarally_native.app')
        plan.append(step('signature', ['codesign', '--verify', '--deep', '--strict', app], 60))
    if args.unsigned_ipa:
        app = (output / 'SegaRallyNative.xcarchive/Products/Applications/segarally_native.app'
               if args.archive else host / 'Release-iphoneos/segarally_native.app')
        payload = output / 'unsigned/Payload'
        plan.append(step('stage-unsigned-ipa', ['ditto', '--norsrc', '--noextattr', '--noqtn',
                         app, payload / 'Sega Rally Native.app']))
        plan.append(step('unsigned-ipa', ['ditto', '-c', '-k', '--norsrc', '--noextattr', '--noqtn',
                         '--keepParent', payload, output / 'SegaRallyNative-unsigned.ipa']))
    if args.export_method:
        export = ['xcodebuild', '-exportArchive', '-archivePath', output / 'SegaRallyNative.xcarchive',
                  '-exportPath', output / 'export', '-exportOptionsPlist', output / 'ExportOptions.plist']
        if args.allow_provisioning_updates:
            export.append('-allowProvisioningUpdates')
        plan.append(step('export', export))
    return plan


def run_step(step: dict, log: Path) -> None:
    with log.open('wb') as file:
        child = subprocess.Popen(step['command'], cwd=ROOT, stdout=file,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = child.wait(timeout=step['timeout_seconds'])
            if code:
                raise subprocess.CalledProcessError(code, step['command'])
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


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--roms', type=Path, help='Personal verified ZIP archive; generate fresh native inputs')
    inputs.add_argument('--native-build', type=Path, help='Existing audited generation in this checkout')
    parser.add_argument('--output', type=Path, required=True, help='New output directory below build/')
    parser.add_argument('--signing', choices=['unsigned', 'development'], default='unsigned')
    parser.add_argument('--graphics', choices=['metal', 'sdl'], default='metal',
                        help='Direct Metal by default; SDL GPU Metal is the comparison build')
    parser.add_argument('--team', help='Personal Apple development team identifier')
    parser.add_argument('--bundle-id', default='com.lgarciacarro.segarally.native')
    parser.add_argument('--device', help='Optional device UDID used only as a build destination')
    parser.add_argument('--version', default='0.1.0')
    parser.add_argument('--build-number', default='1')
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=4, metavar='1..8')
    parser.add_argument('--archive', action='store_true', help='Create an xcarchive instead of only an app')
    parser.add_argument('--unsigned-ipa', action='store_true', help='Wrap an unsigned app in a local IPA for later personal signing')
    parser.add_argument('--export-method', choices=['debugging', 'release-testing', 'app-store-connect'],
                        help='Export a signed archive locally; destination is always export, never upload')
    parser.add_argument('--allow-provisioning-updates', action='store_true', help='Explicitly allow Xcode provisioning changes')
    parser.add_argument('--plan', action='store_true', help='Print commands without building or writing output')
    args = parser.parse_args(argv)
    try:
        validate(args)
        output = output_path(args.output)
        plan = build_plan(args, output)
        if args.plan:
            print(json.dumps(plan, indent=2))
            return 0
        if platform.system() != 'Darwin':
            raise ValueError('iOS builds require macOS with full Xcode')
        for command in ['cmake', 'git', 'xcodebuild', 'xcrun', 'codesign', 'ditto']:
            if not shutil.which(command):
                raise ValueError(f'missing required tool: {command}')
        for name, revision in DEPENDENCIES.items():
            dependency = ROOT / 'extern' / name
            actual = subprocess.check_output(['git', '-C', str(dependency), 'rev-parse', 'HEAD'], text=True).strip()
            if actual != revision:
                raise ValueError(f'extern/{name} must use pinned revision {revision}')
            if subprocess.check_output(['git', '-C', str(dependency), 'diff', 'HEAD', '--'], text=True):
                raise ValueError(f'extern/{name} has tracked modifications')
        if args.roms and not args.roms.is_file():
            raise ValueError('personal archive does not exist')
        if args.native_build:
            native = args.native_build.resolve()
            if not native.is_relative_to(ROOT / 'build') or not (native / 'coverage.json').is_file():
                raise ValueError('native generation must be audited inside this checkout\'s build/')
        output.mkdir(parents=True, exist_ok=False)
        (output / 'logs').mkdir()
        if args.export_method:
            with (output / 'ExportOptions.plist').open('wb') as file:
                plistlib.dump({'method': args.export_method, 'destination': 'export',
                              'teamID': args.team, 'signingStyle': 'automatic'}, file)
        changes = subprocess.check_output(['git', 'diff', 'HEAD', '--'], cwd=ROOT)
        report = {'source_diff_sha256': hashlib.sha256(changes).hexdigest(),
                  'entrypoint_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  'signing': args.signing, 'graphics': args.graphics,
                  'steps': [], 'installed': False, 'launched': False, 'uploaded': False,
                  'scope': 'Personal iOS build/package only; no gameplay or physical audio validation.'}
        destination = output / 'ios-build-result.json'
        try:
            for i, step in enumerate(plan, 1):
                print(f'[{i}/{len(plan)}] {step["name"]}', flush=True)
                row = dict(step, status='running')
                report['steps'].append(row)
                destination.write_text(json.dumps(report, indent=2) + '\n')
                start = time.monotonic()
                try:
                    run_step(step, output / 'logs' / f'{i:02d}-{step["name"]}.log')
                    row['status'] = 'passed'
                finally:
                    row['wall_seconds'] = time.monotonic() - start
            report['status'] = 'built'
            print(f'Build report: {destination}')
        except BaseException as error:
            report.update(status='failed', error=f'{type(error).__name__}: {error}')
            report['steps'][-1]['status'] = 'failed'
            raise
        finally:
            destination.write_text(json.dumps(report, indent=2) + '\n')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'segarally_ios: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print('segarally_ios: interrupted; failed output and logs preserved', file=sys.stderr)
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
