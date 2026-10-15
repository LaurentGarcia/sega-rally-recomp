#!/usr/bin/env python3
"""Local first-import build coordinator. Never sends ROMs or build outputs."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import zipfile

SOURCE_URL = 'https://github.com/LaurentGarcia/sega-rally-recomp.git'
SOURCE_REVISION = '6c22e241286b9e936395c47039a0403225e53ac8'
DEPENDENCIES = {
    'sdl3': ('https://github.com/libsdl-org/SDL.git', 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'),
    'imgui': ('https://github.com/ocornut/imgui.git', 'f1cc2ae15e53a861a874c3034aae6798fde194ab'),
    'lzma': ('https://github.com/ip7z/7zip.git', '0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17'),
}
DEFAULT_ROOT = Path.home() / 'Library/Application Support/Sega Rally Importer/Imports'
MAX_ZIP = 1024 * 1024 * 1024
MAX_LOG = 16 * 1024 * 1024
PHASES = {'main': 'Preparing the main game program', 'tgp': 'Preparing geometry',
          'video': 'Preparing graphics', 'sound': 'Preparing sound',
          'configure': 'Preparing the Mac build', 'compile': 'Building the Mac app',
          'tests': 'Checking the build', 'package': 'Packaging your app',
          'signature': 'Verifying your app'}

class Cancelled(Exception):
    pass

class ImportFailure(Exception):
    pass

def emit(kind, **fields):
    print(json.dumps(dict(kind=kind, **fields)), flush=True)

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()

def write_new(path, value):
    with Path(path).open('x') as f:
        json.dump(value, f, indent=2)
        f.write('\n')

def environment():
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('SDL_', 'SEGARALLY_', 'GIT_')) and k not in ('PYTHONHOME', 'PYTHONPATH')}
    paths = ['/opt/homebrew/bin', '/usr/local/bin', '/usr/bin', '/bin', '/usr/sbin', '/sbin']
    paths += env.get('PATH', '').split(os.pathsep)
    env['PATH'] = os.pathsep.join(dict.fromkeys(p for p in paths if p))
    env['PYTHONNOUSERSITE'] = '1'
    env['GIT_TERMINAL_PROMPT'] = '0'
    return env

class Runner:
    def __init__(self, notify=emit, stop=None):
        self.notify = notify
        self.stop = stop or threading.Event()
        self.env = environment()
        self.log = None
        self.log_bytes = 0

    def output(self, text):
        raw = text.encode('utf-8', errors='replace')
        if self.log is not None:
            self.log_bytes += len(raw)
            if self.log_bytes > MAX_LOG:
                raise ImportFailure('The build log exceeded its safety limit. Your files are preserved.')
            self.log.write(raw)
            self.log.flush()
        self.notify('log', text=text)
        for line in text.splitlines():
            match = re.match(r'\[(\d+)/(\d+)\] (\w+):', line)
            if match:
                self.notify('phase', text=PHASES.get(match[3], 'Preparing your app'),
                            step=int(match[1]), steps=int(match[2]))

    @staticmethod
    def terminate(child, cooperative=False):
        # The pinned pipeline catches KeyboardInterrupt and reaps its active
        # separately grouped build step. Give it that cleanup path first.
        # Ordinary commands and any remaining same-group descendants are reaped.
        try:
            os.killpg(child.pid, signal.SIGINT if cooperative else signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait()
        # Parent may exit before a descendant which ignored TERM.
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass

    def run(self, args, cwd=None, timeout=180, quiet=False, cooperative=False, accepted_codes=(0,)):
        if self.stop.is_set():
            raise Cancelled()
        child = subprocess.Popen([str(a) for a in args], cwd=cwd, env=self.env,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        captured = bytearray()
        start = time.monotonic()
        selector = selectors.DefaultSelector()
        os.set_blocking(child.stdout.fileno(), False)
        selector.register(child.stdout, selectors.EVENT_READ)
        try:
            while True:
                if self.stop.is_set():
                    raise Cancelled()
                if time.monotonic() - start > timeout:
                    raise ImportFailure('This step took too long. Check the log and retry with a new build.')
                for key, _ in selector.select(.1):
                    data = os.read(key.fd, 8192)
                    if data:
                        captured.extend(data)
                        if len(captured) > MAX_LOG:
                            raise ImportFailure('A tool produced too much output.')
                        if not quiet:
                            self.output(data.decode('utf-8', errors='replace'))
                if child.poll() is not None:
                    while True:
                        try:
                            data = os.read(child.stdout.fileno(), 8192)
                        except BlockingIOError:
                            break
                        if not data:
                            break
                        captured.extend(data)
                        if len(captured) > MAX_LOG:
                            raise ImportFailure('A tool produced too much output.')
                        if not quiet:
                            self.output(data.decode('utf-8', errors='replace'))
                    break
            if child.returncode not in accepted_codes:
                raise ImportFailure('A preparation step failed. Open the build log for details; your existing apps are unchanged.')
            return captured.decode('utf-8', errors='replace')
        except BaseException:
            self.terminate(child, cooperative)
            raise
        finally:
            selector.close()
            child.stdout.close()

def prerequisites(runner):
    problems = []
    versions = {'python': sys.version.split()[0]}
    if sys.version_info < (3, 10):
        problems.append('Install Python 3.10 or newer from python.org, then reopen this importer.')
    for tool in ('git', 'cmake', 'ctest', 'c++', 'codesign'):
        if not shutil.which(tool, path=runner.env['PATH']):
            problems.append('Install CMake 3.25 or newer from cmake.org.' if tool in ('cmake', 'ctest')
                            else 'Install Apple Command Line Tools using xcode-select --install in Terminal.')
    if not problems:
        version = runner.run(['cmake', '--version'], quiet=True)
        found = re.search(r'cmake version (\d+)\.(\d+)', version)
        if not found or tuple(map(int, found.groups())) < (3, 25):
            problems.append('CMake 3.25 or newer is required; update CMake from cmake.org.')
        versions['cmake'] = version.splitlines()[0]
        try:
            versions['developer_tools'] = runner.run(['/usr/bin/xcode-select', '-p'], quiet=True).strip()
            runner.run(['/usr/bin/xcrun', '--find', 'clang++'], quiet=True)
            versions['git'] = runner.run(['git', '--version'], quiet=True).strip()
        except ImportFailure:
            problems.append('Install Apple Command Line Tools using xcode-select --install in Terminal. Open Xcode once if it needs license acceptance.')
    if problems:
        raise ImportFailure('\n'.join(dict.fromkeys(problems)))
    return versions

class Coordinator:
    def __init__(self, root=DEFAULT_ROOT, runner=None):
        self.root = Path(root).expanduser()
        self.runner = runner or Runner()

    def clone(self, url, revision, path):
        if path.exists() or path.is_symlink():
            raise ImportFailure('A download destination already exists; choose a new import.')
        path.parent.mkdir(parents=True, exist_ok=True)
        self.runner.run(['git', 'init', '-q', path])
        self.runner.run(['git', '-C', path, 'remote', 'add', 'origin', url])
        self.runner.run(['git', '-C', path, 'fetch', '--depth', '1', '--no-tags', 'origin', revision], timeout=600)
        self.runner.run(['git', '-C', path, 'checkout', '--detach', revision])
        if self.runner.run(['git', '-C', path, 'rev-parse', 'HEAD'], quiet=True).strip() != revision:
            raise ImportFailure('The downloaded source did not match the required version.')
        if self.runner.run(['git', '-C', path, 'status', '--porcelain', '--untracked-files=no'], quiet=True).strip():
            raise ImportFailure('The downloaded source has local modifications.')

    def verify_app(self, session):
        source = session / 'source'
        build = source / 'build/player'
        report = json.loads((build / 'build-result.json').read_text())
        app = build / 'mac/Sega Rally Native.app'
        if report['status'] != 'packaged' or report['source_revision'] != SOURCE_REVISION:
            raise ImportFailure('The game build did not complete successfully.')
        if report['source_diff_sha256'] != hashlib.sha256(b'').hexdigest():
            raise ImportFailure('The game source changed during the build.')
        if report['dependencies'] != {k: v[1] for k, v in DEPENDENCIES.items()}:
            raise ImportFailure('The build dependency versions do not match.')
        if app.is_symlink() or not app.resolve().is_relative_to(session.resolve()):
            raise ImportFailure('The completed app has an unexpected location.')
        exe = app / 'Contents/MacOS/segarally_native'
        assets = app / 'Contents/Resources/assets'
        if digest(exe) != report['executable_sha256'] or digest(assets / 'assets.json') != report['assets_sha256']:
            raise ImportFailure('The completed app changed after packaging.')
        manifest = json.loads((assets / 'assets.json').read_text())
        if len(manifest['images']) != 8:
            raise ImportFailure('The completed app is missing game inputs.')
        for item in manifest['images'].values():
            image = assets / item['file']
            if not image.resolve().is_relative_to(assets.resolve()) or image.is_symlink() or image.stat().st_size != item['size'] or digest(image) != item['sha256']:
                raise ImportFailure('A completed app input failed verification.')
        self.runner.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', app], quiet=True)
        return app

    def import_rom(self, rom):
        self.runner.notify('phase', text='Checking your Mac')
        versions = prerequisites(self.runner)
        rom = Path(rom).expanduser().resolve(strict=True)
        if not rom.is_file() or rom.suffix.lower() != '.zip' or rom.stat().st_size > MAX_ZIP or not zipfile.is_zipfile(rom):
            raise ImportFailure('Choose your own Sega Rally Revision B ZIP archive. Its contents will be verified locally.')
        with zipfile.ZipFile(rom) as archive:
            info = archive.infolist()
            if len(info) > 1024 or sum(i.file_size for i in info) > MAX_ZIP:
                raise ImportFailure('This archive is larger than the supported Sega Rally inputs.')
        if self.root.is_symlink():
            raise ImportFailure('The import folder must not be a symbolic link.')
        self.root.mkdir(parents=True, exist_ok=True)
        session = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=self.root)).resolve()
        self.runner.notify('workspace', path=str(session))
        try:
            with (session / 'import.log').open('xb') as log:
                self.runner.log = log
                original_hash = digest(rom)
                snapshot = session / 'personal-input.zip'
                with rom.open('rb') as src, snapshot.open('xb') as dst:
                    total = 0
                    while chunk := src.read(65536):
                        if self.runner.stop.is_set(): raise Cancelled()
                        total += len(chunk)
                        if total > MAX_ZIP: raise ImportFailure('The selected archive grew beyond the supported limit.')
                        dst.write(chunk)
                if digest(snapshot) != original_hash or digest(rom) != original_hash:
                    raise ImportFailure('The archive changed while it was being copied. Please retry.')
                write_new(session / 'inputs.json', {'source_url': SOURCE_URL, 'source_revision': SOURCE_REVISION,
                          'rom_sha256': original_hash, 'tools': versions, 'dependencies': DEPENDENCIES,
                          'scope': 'Local personal inputs only; no ROM or generated output is uploaded.'})
                self.runner.notify('phase', text='Downloading the pinned source')
                source = session / 'source'
                self.clone(SOURCE_URL, SOURCE_REVISION, source)
                for index, (name, (url, revision)) in enumerate(DEPENDENCIES.items(), 1):
                    self.runner.notify('phase', text=f'Downloading build dependency {index} of 3')
                    self.clone(url, revision, source / 'extern' / name)
                self.runner.notify('phase', text='Building your app locally — this can take several minutes')
                self.runner.run([sys.executable, source / 'scripts/segarally_mac.py', '--roms', snapshot,
                                 '--output', source / 'build/player', '--jobs', '2'], cwd=source, timeout=3600, cooperative=True)
                app = self.verify_app(session)
                if self.runner.stop.is_set(): raise Cancelled()
                write_new(session / 'completed.json', {'source_revision': SOURCE_REVISION, 'app': str(app),
                          'executable_sha256': digest(app / 'Contents/MacOS/segarally_native')})
                self.runner.notify('complete', app=str(app), session=str(session))
                return session
        except BaseException as error:
            write_new(session / 'failed.json', {'status': 'cancelled' if isinstance(error, Cancelled) else 'failed',
                      'message': str(error), 'source_revision': SOURCE_REVISION})
            raise
        finally:
            self.runner.log = None

    def launch(self, session):
        session = Path(session).resolve(strict=True)
        if not session.is_relative_to(self.root.resolve()) or session == self.root.resolve():
            raise ImportFailure('Choose a completed import from this importer.')
        receipt = json.loads((session / 'completed.json').read_text())
        app = self.verify_app(session)
        if receipt['source_revision'] != SOURCE_REVISION or receipt['app'] != str(app) or receipt['executable_sha256'] != digest(app / 'Contents/MacOS/segarally_native'):
            raise ImportFailure('The completed import no longer matches its receipt.')
        saves = session / 'player-saves'
        if saves.is_symlink() or not saves.resolve().is_relative_to(session):
            raise ImportFailure('The save folder has an unexpected location.')
        active = self.runner.run(['/usr/bin/pgrep', '-x', 'segarally_native'], quiet=True, accepted_codes=(0, 1))
        if active.strip():
            raise ImportFailure('Close the running Sega Rally app before launching this copy. Its saves have not been changed.')
        self.runner.run(['/usr/bin/open', '-n', app, '--args', '--player',
                         '--save-dir', saves], quiet=True)
        self.runner.notify('launched')

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-root', type=Path, default=DEFAULT_ROOT)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--check', action='store_true')
    action.add_argument('--import-rom', type=Path)
    action.add_argument('--launch', type=Path)
    args = parser.parse_args(argv)
    stop = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    runner = Runner(stop=stop)
    try:
        if sys.platform != 'darwin':
            raise ImportFailure('This importer requires macOS on Apple Silicon.')
        if args.check:
            emit('ready', tools=prerequisites(runner))
        elif args.import_rom:
            Coordinator(args.work_root, runner).import_rom(args.import_rom)
        else:
            Coordinator(args.work_root, runner).launch(args.launch)
        return 0
    except Cancelled:
        emit('cancelled', text='Cancelled. Logs and partial files are preserved. Retry creates a new build.')
        return 130
    except (OSError, ValueError, KeyError, ImportFailure, subprocess.SubprocessError) as error:
        emit('error', text=str(error))
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
