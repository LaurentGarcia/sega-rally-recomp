#!/usr/bin/env python3
"""Exercise direct Vulkan presentation on an X11 window with lavapipe/Xvfb."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--sdl-source', type=Path, default=ROOT / 'extern/sdl3')
    args = parser.parse_args()
    output = args.output.resolve()
    compile_dir = args.build_dir.resolve() if args.build_dir else output / 'compile'
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory below ignored build/')
    if compile_dir == ROOT / 'build' or not compile_dir.is_relative_to(ROOT / 'build'):
        parser.error('build directory must be below ignored build/')
    if not sys.platform.startswith('linux'):
        parser.error('this software-presentation gate requires Linux/X11/Xvfb')
    sdl = args.sdl_source.resolve()
    revision = subprocess.check_output(['git', '-C', str(sdl), 'rev-parse', 'HEAD'], text=True).strip()
    diff = subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--'])
    if revision != 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c' or diff:
        parser.error('SDL must match the clean project pin')
    paths = [ROOT / p for p in (
        'src/app/native/vulkan/model2_vulkan.h', 'src/app/native/vulkan/model2_vulkan.cpp',
        'src/app/native/vulkan/vulkan_presenter.h', 'src/app/native/vulkan/vulkan_presenter.cpp',
        'src/app/gpu/model2_frame.h', 'src/app/gpu/model2_frame.cpp', 'src/app/gpu/model2_resources.h',
        'src/app/gpu/shaders_gen.h', 'src/app/gpu/crt_shaders_gen.h', 'src/app/gpu/crt_settings.h',
        'src/runtime/video.h', 'src/runtime/video.cpp', 'src/runtime/raster.h', 'src/runtime/raster.cpp',
        'tests/model2_native_fixture.h', 'tests/test_native_vulkan_presenter.cpp',
        'tests/test_native_vulkan_presenter.py', 'platform/segarally/native-vulkan-model2/CMakeLists.txt')]
    before = {str(p.relative_to(ROOT)): digest(p) for p in paths}
    output.mkdir(parents=True)
    result = {'status': 'failed', 'inputs': before, 'sdl_revision': revision, 'commands': [],
              'scope': 'Direct Vulkan software swapchain, CRT, cached frames, resize and overlay. '
                       'SDL GPU disabled. No game execution, hardware performance or device-loss injection.'}
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SDL_', 'SEGARALLY_'))}
    env['SDL_VIDEODRIVER'] = 'x11'

    def run(name, command, timeout):
        start = time.monotonic()
        with (output / (name + '.stdout')).open('wb') as stdout, (output / (name + '.stderr')).open('wb') as stderr:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
                if code:
                    raise subprocess.CalledProcessError(code, command)
            except BaseException:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                raise
            finally:
                result['commands'].append({'name': name, 'command': command, 'seconds': time.monotonic() - start,
                                           'returncode': process.returncode})
    try:
        run('configure', ['cmake', '-S', str(ROOT / 'platform/segarally/native-vulkan-model2'), '-B', str(compile_dir),
                          '-DCMAKE_BUILD_TYPE=Release', '-DSEGARALLY_VULKAN_PRESENTATION=ON',
                          '-DSEGARALLY_VULKAN_SDL_REFERENCE=OFF', '-DSEGARALLY_SDL_SOURCE=' + str(sdl)], 600)
        run('build', ['cmake', '--build', str(compile_dir), '--parallel', '2'], 600)
        cache = (compile_dir / 'CMakeCache.txt').read_text()
        assert 'SDL_GPU:BOOL=OFF' in cache
        binary = compile_dir / 'test_native_vulkan_presenter'
        result['executable_sha256'] = digest(binary)
        run('presentation', ['xvfb-run', '-a', '-s', '-screen 0 1280x1024x24', str(binary), str(output)], 240)
        logs = (output / 'presentation.stdout').read_text() + (output / 'presentation.stderr').read_text()
        assert 'Validation Error' not in logs and 'VUID-' not in logs and 'SYNC-HAZARD' not in logs
        result['metrics'] = json.loads((output / 'metrics.json').read_text())
        result['artifacts'] = {p.name: digest(p) for p in sorted(output.glob('*.bgra'))}
        capture = output / 'raw-game.ppm'
        data = capture.read_bytes()
        assert data.startswith(b'P6\n496 384\n255\n') and len(data) == len(b'P6\n496 384\n255\n') + 496 * 384 * 3
        result['artifacts'][capture.name] = digest(capture)
        assert before == {str(p.relative_to(ROOT)): digest(p) for p in paths}
        assert subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--']) == diff
        result['status'] = 'passed'
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(output / 'result.json')


if __name__ == '__main__':
    main()
