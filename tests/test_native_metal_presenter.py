#!/usr/bin/env python3
"""Exercise the direct Metal presenter or Settings panel with invented inputs."""
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
    for name in ('sdl-source', 'sdl-library', 'imgui-source', 'imgui-library', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--panel', action='store_true',
                        help='test Settings GPU pixels, context isolation and SDL/native backend transitions')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory under ignored build/')
    pins = [(args.sdl_source, 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'),
            (args.imgui_source, 'f1cc2ae15e53a861a874c3034aae6798fde194ab')]
    for source, revision in pins:
        assert subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() == revision
        assert not subprocess.check_output(['git', '-C', str(source), 'diff', 'HEAD', '--'])
    sources = ['tests/test_native_metal_presenter.cpp', 'src/app/native/metal/metal_presenter.mm',
               'src/app/native/metal/model2_metal.mm', 'src/app/native/metal/metal_crt.mm',
               'src/app/gpu/model2_frame.cpp', 'src/runtime/video.cpp', 'src/runtime/raster.cpp',
               'src/app/segarally_display.cpp', 'src/app/segarally_display_metal.mm', 'src/app/segarally_controls.cpp']
    headers = ['src/app/native/metal/metal_presenter.h', 'src/app/native/metal/model2_metal.h',
               'src/app/native/metal/metal_crt.h', 'src/app/gpu/shaders_gen.h', 'src/app/gpu/crt_shaders_gen.h',
               'src/app/gpu/model2_frame.h', 'src/app/gpu/model2_resources.h', 'src/app/gpu/crt_settings.h',
               'src/app/segarally_display.h', 'src/app/segarally_display_metal.h', 'src/app/segarally_controls.h',
               'src/app/segarally_display_preferences.h', 'src/runtime/video.h', 'src/runtime/raster.h',
               'tests/model2_native_fixture.h', 'tests/test_native_metal_presenter.py']
    if args.panel:
        sources = ['tests/test_segarally_display_metal.mm', 'src/app/segarally_display.cpp',
                   'src/app/segarally_display_metal.mm', 'src/app/segarally_controls.cpp']
        headers = ['src/app/segarally_display.h', 'src/app/segarally_display_metal.h',
                   'src/app/segarally_controls.h', 'src/app/segarally_display_preferences.h',
                   'src/app/gpu_device.h', 'src/app/gpu/crt_settings.h',
                   'tests/test_native_metal_presenter.py']
    external = [args.imgui_source / 'backends/imgui_impl_metal.mm', args.imgui_library, args.sdl_library]
    inputs = [ROOT / p for p in sources + headers] + external
    before = {str(p.resolve()): digest(p) for p in inputs}
    output.mkdir(parents=True)
    result = {'status': 'FAIL', 'inputs_before': before, 'commands': [],
              'fixture': 'panel' if args.panel else 'presenter',
              'metal_validation': True,
              'scope': 'Real Mac window with direct Metal game, CRT, original scaling and Settings. '
                       'Invented inputs, raw captures, resize, pause/redraw and hidden-window lifecycle. No game execution.'}
    if args.panel:
        result['scope'] = ('Settings renders real GPU pixels through direct Metal and SDL Metal in one binary. '
                           'Mouse CRT toggles, persisted preferences, binding isolation, foreign ImGui context '
                           'preservation, backend transitions and teardown. No game execution or presentation claim.')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SDL_', 'SEGARALLY_'))}
    env['MTL_DEBUG_LAYER'] = '1'
    def run(name, command, timeout):
        start = time.monotonic(); record = {'name': name, 'command': command}; result['commands'].append(record)
        with (output / (name + '.stdout')).open('wb') as stdout, (output / (name + '.stderr')).open('wb') as stderr:
            child = subprocess.Popen(command, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
            try:
                record['exit'] = child.wait(timeout)
                if record['exit']:
                    raise RuntimeError(name + ' failed; inspect retained logs')
            except BaseException:
                if child.poll() is None:
                    try:
                        os.killpg(child.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        child.wait(5)
                    except subprocess.TimeoutExpired:
                        os.killpg(child.pid, signal.SIGKILL); child.wait()
                raise
            finally:
                record['wall_seconds'] = time.monotonic() - start
    try:
        frameworks = ['CoreMedia', 'CoreVideo', 'Cocoa', 'UniformTypeIdentifiers', 'IOKit', 'ForceFeedback', 'Carbon',
                      'CoreAudio', 'AudioToolbox', 'AVFoundation', 'Foundation', 'GameController', 'Metal', 'QuartzCore', 'CoreHaptics']
        flags = ['-std=c++20', '-O2', '-mmacosx-version-min=13.0', '-fno-fast-math', '-ffp-contract=off', '-fobjc-arc', '-DSEGARALLY_NATIVE_METAL=1',
                 '-Wall', '-Wextra', '-Werror', '-Wno-unused-private-field', '-Wno-misleading-indentation',
                 '-I' + str(ROOT / 'src'), '-I' + str(args.sdl_source.resolve() / 'include'),
                 '-I' + str(args.imgui_source.resolve())]
        executable = output / ('panel-fixture' if args.panel else 'presenter-fixture')
        run('compile', ['clang++', *flags, *[str(ROOT / p) for p in sources], *map(str, external),
                       *[arg for framework in frameworks for arg in ('-framework', framework)], '-o', str(executable)], 120)
        run('window', [str(executable), str(output / 'panel' if args.panel else output)], 30)
        stderr = (output / 'window.stderr').read_text()
        assert not any(needle in stderr for needle in ('failed assertion', 'Validation Error', 'error:'))
        result['status'] = 'PASS'
    except BaseException as error:
        result['error'] = str(error); raise
    finally:
        result['inputs_after'] = {str(p.resolve()): digest(p) for p in inputs}
        result['inputs_unchanged'] = result['inputs_after'] == before
        if not result['inputs_unchanged']:
            result['status'] = 'FAIL'
        result['artifacts'] = {str(p.relative_to(output)): digest(p) for p in output.rglob('*') if p.is_file()}
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(result['status'], output / 'result.json')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
