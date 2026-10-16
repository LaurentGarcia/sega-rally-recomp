#!/usr/bin/env python3
"""Compare native hardware blits with SDL Vulkan in independently captured X11 windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output, executable = args.output.resolve(), args.executable.resolve()
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory below ignored build/')
    paths = [ROOT / name for name in (
        'tests/test_native_vulkan_blit.cpp', 'tests/test_native_vulkan_blit.py', 'tests/model2_native_fixture.h',
        'src/app/native/vulkan/model2_vulkan.cpp', 'src/app/native/vulkan/model2_vulkan.h',
        'src/app/native/vulkan/vulkan_presenter.cpp', 'src/app/native/vulkan/vulkan_presenter.h',
        'src/app/gpu/gpu_renderer.cpp', 'src/app/gpu/gpu_renderer.h', 'src/app/gpu/model2_frame.cpp',
        'src/app/gpu/model2_frame.h', 'src/app/gpu/model2_resources.h', 'src/app/gpu/shaders_gen.h',
        'src/app/gpu/crt_shaders_gen.h', 'src/runtime/video.cpp', 'src/runtime/video.h',
        'src/runtime/raster.cpp', 'src/runtime/raster.h')]
    paths.append(executable)
    before = {str(path): sha(path) for path in paths}
    output.mkdir(parents=True)
    result = {'status': 'failed', 'inputs': before, 'commands': [], 'images': [],
              'scope': 'Actual X11/lavapipe window pixels; native vkCmdBlitImage versus pinned SDL Vulkan blits. '
                       'Four odd-size/downscaled nearest/linear cases; no game data or hardware performance claim.'}
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SDL_', 'SEGARALLY_'))}
    env['SDL_VIDEODRIVER'] = 'x11'
    try:
        for arm in ('sdl', 'native'):
            command = ['xvfb-run', '-a', '-s', '-screen 0 1280x1024x24', str(executable), str(output / arm), '--' + arm]
            with (output / (arm + '.stdout')).open('wb') as stdout, (output / (arm + '.stderr')).open('wb') as stderr:
                process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
                try:
                    code = process.wait(timeout=120)
                    result['commands'].append({'command': command, 'returncode': code})
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
            logs = (output / (arm + '.stdout')).read_text() + (output / (arm + '.stderr')).read_text()
            assert not any(word in logs for word in ('Validation Error', 'VUID-', 'SYNC-HAZARD'))
        for width, height in ((497, 385), (317, 245)):
            for filter_name in ('nearest', 'linear'):
                name = f'{width}x{height}-{filter_name}.bgra'
                native, baseline = (output / 'native' / name).read_bytes(), (output / 'sdl' / name).read_bytes()
                assert len(native) == len(baseline) == width * height * 4
                differing = sum(native[i:i+4] != baseline[i:i+4] for i in range(0, len(native), 4))
                result['images'].append({'name': name, 'pixels': width * height, 'differences': differing,
                                         'native_sha256': hashlib.sha256(native).hexdigest(),
                                         'sdl_sha256': hashlib.sha256(baseline).hexdigest()})
        assert all(row['differences'] == 0 for row in result['images']), result['images']
        assert before == {str(path): sha(path) for path in paths}, 'source/executable mutation during run'
        result['status'] = 'passed'
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(output / 'result.json')


if __name__ == '__main__':
    main()
