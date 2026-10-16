#!/usr/bin/env python3
"""Direct Metal versus pinned SDL Metal; synthetic scenes and optional private snapshots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdl-library', type=Path, required=True)
    parser.add_argument('--sdl-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scenes', type=Path)
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory within ignored build/')
    sdl = args.sdl_source.resolve()
    revision = subprocess.check_output(['git', '-C', str(sdl), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c':
        parser.error('SDL source must be the project pin')
    diff = subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--'])
    if diff:
        parser.error('SDL reference source must have no tracked changes')
    measured = [ROOT / name for name in (
        'src/app/native/metal/model2_metal.h', 'src/app/native/metal/model2_metal.mm',
        'src/app/gpu/model2_frame.h', 'src/app/gpu/model2_frame.cpp', 'src/app/gpu/model2_resources.h',
        'src/app/gpu/shaders_gen.h', 'src/app/gpu/m2.hlsl', 'src/app/gpu/normal_reciprocal.hlsl',
        'src/app/gpu/gpu_renderer.h', 'src/app/gpu/gpu_renderer.cpp', 'src/app/gpu_device.h',
        'src/runtime/video.h', 'src/runtime/video.cpp', 'src/runtime/raster.h', 'src/runtime/raster.cpp',
        'src/runtime/model2_priority.h', 'tests/model2_native_fixture.h', 'tests/model2_recorded_fixture.h',
        'tests/test_native_metal_model2.mm', 'tests/test_native_metal_model2.py')]
    measured.append(args.sdl_library.resolve())
    if args.scenes:
        for directory in ('neutral/1634', 'race/2329', 'race/2599'):
            measured.extend(p for p in (args.scenes.resolve() / directory).iterdir() if p.is_file())
    before = {str(p): digest(p) for p in measured}
    output.mkdir(parents=True)
    result = {'status': 'FAIL', 'inputs_before': before, 'commands': [], 'sdl_revision': revision,
              'sdl_diff_sha256': hashlib.sha256(diff).hexdigest(),
              'scope': 'Offscreen direct Metal/SDL Metal equivalence. Native binary links no SDL. '
                       '26 invented scenes plus optional twelve draws of three private historical scenes. '
                       'No full-game, presentation, input, audio or performance claim.'}
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SDL_', 'SEGARALLY_'))}
    env['MTL_DEBUG_LAYER'] = '1'
    def run(name, command, timeout=60):
        start = time.monotonic()
        record = {'name': name, 'command': command}
        result['commands'].append(record)
        with (output / (name + '.stdout')).open('wb') as stdout, (output / (name + '.stderr')).open('wb') as stderr:
            proc = subprocess.Popen(command, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
            try:
                code = proc.wait(timeout=timeout)
                record['exit'] = code
                if code:
                    raise RuntimeError(name + ' failed; see retained stdout/stderr')
            except BaseException:
                if proc.poll() is None:
                    try:
                        os.killpg(proc.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid, signal.SIGKILL)
                        proc.wait()
                raise
            finally:
                record['wall_seconds'] = time.monotonic() - start
    try:
        flags = ['-std=c++20', '-O2', '-fno-fast-math', '-ffp-contract=off', '-Wall', '-Wextra', '-Werror',
                 '-Wno-misleading-indentation', '-Wno-bitwise-op-parentheses', '-Wno-unused-private-field',
                 '-fobjc-arc', '-I' + str(ROOT / 'src')]
        common = [str(ROOT / p) for p in ('tests/test_native_metal_model2.mm', 'src/app/gpu/model2_frame.cpp',
                                        'src/runtime/video.cpp', 'src/runtime/raster.cpp')]
        native = output / 'native-fixture'
        reference = output / 'sdl-fixture'
        run('compile-native', [args.compiler, *flags, '-DNATIVE_METAL_MODEL2=1', *common,
            str(ROOT / 'src/app/native/metal/model2_metal.mm'), '-framework', 'Foundation', '-framework', 'Metal',
            '-o', str(native)], 120)
        frameworks = ['CoreMedia', 'CoreVideo', 'Cocoa', 'UniformTypeIdentifiers', 'IOKit', 'ForceFeedback', 'Carbon',
                      'CoreAudio', 'AudioToolbox', 'AVFoundation', 'Foundation', 'GameController', 'Metal', 'QuartzCore', 'CoreHaptics']
        link = [arg for framework in frameworks for arg in ('-framework', framework)]
        run('compile-sdl', [args.compiler, *flags, '-I' + str(sdl / 'include'), *common,
            str(ROOT / 'src/app/gpu/gpu_renderer.cpp'), str(args.sdl_library.resolve()), *link, '-o', str(reference)], 120)
        for name, executable in (('sdl', reference), ('native', native)):
            (output / name).mkdir()
            run(name, [str(executable), str(output / name), *([str(args.scenes.resolve())] if args.scenes else [])])
        result['images'] = []
        for i in range(38 if args.scenes else 26):
            name = f'scene-{i:02d}.bgra'
            expected = (output / 'sdl' / name).read_bytes()
            actual = (output / 'native' / name).read_bytes()
            scale = 2 if (i < 24 or i >= 26) and i % 2 else 1
            width = 560 if i == 24 else 496
            assert len(actual) == len(expected) == width * 384 * scale * scale * 4
            differences = sum(actual[n:n+4] != expected[n:n+4] for n in range(0, len(actual), 4))
            result['images'].append({'name': name, 'pixels': len(actual) // 4, 'different_pixels': differences,
                                     'native_sha256': digest(output / 'native' / name),
                                     'sdl_sha256': digest(output / 'sdl' / name)})
            assert actual == expected, (name, differences)
        linkage = subprocess.check_output(['otool', '-L', str(native)], text=True)
        (output / 'native-linkage.txt').write_text(linkage)
        symbols = subprocess.check_output(['nm', '-u', str(native)], text=True)
        (output / 'native-symbols.txt').write_text(symbols)
        assert 'SDL' not in linkage and '_SDL_' not in symbols
        assert subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--']) == diff
        result['status'] = 'PASS'
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        result['inputs_after'] = {str(p): digest(p) for p in measured}
        result['inputs_unchanged'] = result['inputs_after'] == before
        if not result['inputs_unchanged']:
            result['status'] = 'FAIL'
        result['artifacts'] = {str(p.relative_to(output)): digest(p) for p in output.rglob('*') if p.is_file()}
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(result['status'], output / 'result.json')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
