#!/usr/bin/env python3
"""Build and exercise native Wayland WSI with headless Weston and software Vulkan.

Requires Linux, the pinned SDL checkout, CMake/C++, Weston, Wayland development
packages, Vulkan headers/loader, a CPU Vulkan ICD and Khronos validation layers.
Captures contain original synthetic inputs. This is not a physical PC gate.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SDL_REVISION = 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def terminate(process):
    if process and process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--sdl-source', type=Path, default=ROOT / 'extern/sdl3')
    parser.add_argument('--icd', type=Path, required=True, help='CPU Vulkan ICD JSON')
    args = parser.parse_args()
    output = args.output.resolve()
    compile_dir = args.build_dir.resolve() if args.build_dir else output / 'compile'
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory below ignored build/')
    if compile_dir == ROOT / 'build' or not compile_dir.is_relative_to(ROOT / 'build'):
        parser.error('build directory must be below ignored build/')
    if not sys.platform.startswith('linux'):
        parser.error('this software WSI gate requires Linux')
    sdl = args.sdl_source.resolve()
    revision = subprocess.check_output(['git', '-C', str(sdl), 'rev-parse', 'HEAD'], text=True).strip()
    diff = subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--'])
    if revision != SDL_REVISION or diff:
        parser.error('SDL must match the clean project pin')
    icd = args.icd.resolve(strict=True)
    icd_config = json.loads(icd.read_text())
    driver = Path(icd_config['ICD']['library_path'])
    if not driver.is_absolute():
        # Resolve the loader's actual soname choice rather than guessing a distro library directory.
        loaded_driver = ctypes.CDLL(str(driver))
        mapped = {Path(line.split()[-1]).resolve() for line in Path('/proc/self/maps').read_text().splitlines()
                  if line.split()[-1].startswith('/') and Path(line.split()[-1]).name == driver.name}
        if len(mapped) != 1:
            parser.error('could not identify the loaded ICD library for provenance')
        driver = mapped.pop()
    driver = driver.resolve(strict=True)
    paths = [ROOT / p for p in (
        'src/app/native/vulkan/model2_vulkan.h', 'src/app/native/vulkan/model2_vulkan.cpp',
        'src/app/native/vulkan/vulkan_presenter.h', 'src/app/native/vulkan/vulkan_presenter.cpp',
        'src/app/gpu/model2_frame.h', 'src/app/gpu/model2_frame.cpp', 'src/app/gpu/model2_resources.h',
        'src/app/gpu/shaders_gen.h', 'src/app/gpu/crt_shaders_gen.h', 'src/app/gpu/crt_settings.h',
        'src/runtime/video.h', 'src/runtime/video.cpp', 'src/runtime/raster.h', 'src/runtime/raster.cpp',
        'tests/model2_native_fixture.h', 'tests/test_native_vulkan_wayland.cpp',
        'tests/test_native_vulkan_wayland.py', 'platform/segarally/native-vulkan-model2/CMakeLists.txt')]
    before = {str(p.relative_to(ROOT)): digest(p) for p in paths}
    output.mkdir(parents=True)
    result = {'status': 'failed', 'inputs': before, 'sdl_revision': revision, 'commands': [],
              'host': {'platform': platform.platform(), 'machine': platform.machine(),
                       'container': Path('/.dockerenv').exists(), 'dri_present': Path('/dev/dri').exists()},
              'icd': {'path': str(icd), 'sha256': digest(icd),
                      'library': str(driver), 'library_sha256': digest(driver)},
              'scope': 'Native Wayland wl_surface and Vulkan swapchain under headless Weston/pixman. '
                       'SDL GPU and X11 disabled. CPU Vulkan required. Captures are raw offscreen images. '
                       'No compositor screenshots, game execution, physical display/input/audio, vendor GPU '
                       'or desktop compositor compatibility claim.'}
    prefixes = ('SDL_', 'SEGARALLY_', 'VK_', 'MESA_', 'LIBGL_', 'GALLIUM_')
    env = {k: v for k, v in os.environ.items() if not k.startswith(prefixes)
           and k not in ('DISPLAY', 'WAYLAND_DISPLAY', 'XDG_RUNTIME_DIR', 'DRI_PRIME')}
    env.update(SDL_VIDEO_DRIVER='wayland', SDL_VIDEODRIVER='wayland',
               VK_DRIVER_FILES=str(icd), VK_ICD_FILENAMES=str(icd))

    def run(name, command, timeout):
        start = time.monotonic()
        with (output / (name + '.stdout')).open('wb') as stdout, (output / (name + '.stderr')).open('wb') as stderr:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
                if code:
                    raise subprocess.CalledProcessError(code, command)
            finally:
                terminate(process)
                result['commands'].append({'name': name, 'command': command, 'seconds': time.monotonic() - start,
                                           'returncode': process.returncode})
    compositor = None
    try:
        for name, command in (
                ('cmake-version', ['cmake', '--version']), ('compiler-version', ['c++', '--version']),
                ('weston-version', ['weston', '--version']),
                ('dependency-versions', ['pkg-config', '--modversion', 'wayland-client', 'wayland-protocols', 'xkbcommon', 'vulkan'])):
            run(name, command, 15)
        run('configure', ['cmake', '-S', str(ROOT / 'platform/segarally/native-vulkan-model2'), '-B', str(compile_dir),
                          '-DCMAKE_BUILD_TYPE=Release', '-DSEGARALLY_VULKAN_WAYLAND=ON',
                          '-DSEGARALLY_VULKAN_PRESENTATION=OFF', '-DSEGARALLY_VULKAN_SDL_REFERENCE=OFF',
                          '-DSDL_WAYLAND_LIBDECOR=OFF', '-DSEGARALLY_SDL_SOURCE=' + str(sdl)], 600)
        run('build', ['cmake', '--build', str(compile_dir), '--target', 'test_native_vulkan_wayland', '--parallel', '2'], 900)
        cache = compile_dir / 'CMakeCache.txt'
        cache_text = cache.read_text()
        for setting in ('SDL_GPU:BOOL=OFF', 'SDL_X11:BOOL=OFF', 'SDL_WAYLAND:BOOL=ON'):
            assert setting in cache_text, setting
        configs = list((compile_dir / 'sdl3').rglob('SDL_build_config.h'))
        assert len(configs) == 1, configs
        config_text = configs[0].read_text()
        for define in ('#define SDL_GPU_DISABLED 1', '#define SDL_VIDEO_DRIVER_WAYLAND 1'):
            assert define in config_text, define
        assert '#define SDL_VIDEO_DRIVER_X11 1' not in config_text
        result['build_configuration'] = {'cache_sha256': digest(cache), 'header_sha256': digest(configs[0])}
        binary = compile_dir / 'test_native_vulkan_wayland'
        result['executable_sha256'] = digest(binary)
        run('dependencies', ['ldd', str(binary)], 15)
        with tempfile.TemporaryDirectory(prefix='sr-wl-') as runtime:
            env.update(XDG_RUNTIME_DIR=runtime, WAYLAND_DISPLAY='sr-fixture')
            result['runtime_environment'] = {k: env[k] for k in ('SDL_VIDEO_DRIVER', 'SDL_VIDEODRIVER',
                                                               'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'WAYLAND_DISPLAY')}
            command = ['weston', '--backend=headless', '--renderer=pixman', '--shell=desktop', '--idle-time=0',
                       '--no-config', '--width=1280', '--height=1024', '--socket=sr-fixture',
                       '--log=' + str(output / 'weston.log')]
            result['compositor_command'] = command
            with (output / 'weston.stdout').open('wb') as stdout, (output / 'weston.stderr').open('wb') as stderr:
                compositor = subprocess.Popen(command, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
                try:
                    deadline = time.monotonic() + 15
                    while not (Path(runtime) / 'sr-fixture').exists():
                        if compositor.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError('headless Weston did not create its private Wayland socket')
                        time.sleep(0.05)
                    run('wayland', [str(binary), str(output)], 120)
                    assert compositor.poll() is None, 'compositor died during fixture'
                finally:
                    terminate(compositor)
                    result['compositor_returncode_after_stop'] = compositor.returncode
        logs = (output / 'wayland.stdout').read_text() + (output / 'wayland.stderr').read_text()
        assert 'Vulkan CPU device:' in logs
        assert not any(token in logs for token in ('Validation Error', 'VUID-', 'SYNC-HAZARD'))
        result['metrics'] = json.loads((output / 'metrics.json').read_text())
        assert result['metrics']['video_driver'] == 'wayland' and result['metrics']['presentations'] == 68
        assert result['metrics']['validation_errors'] == 0
        header = b'P6\n496 384\n255\n'
        for name in ('before-pause.ppm', 'after-pause.ppm', 'restored.ppm'):
            data = (output / name).read_bytes()
            assert data.startswith(header) and len(data) == len(header) + 496 * 384 * 3
        assert digest(output / 'before-pause.ppm') == digest(output / 'after-pause.ppm')
        assert digest(output / 'before-pause.ppm') != digest(output / 'restored.ppm')
        assert before == {str(p.relative_to(ROOT)): digest(p) for p in paths}
        assert digest(icd) == result['icd']['sha256'] and digest(driver) == result['icd']['library_sha256']
        assert subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--']) == diff
        result['status'] = 'passed'
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        terminate(compositor)
        result['artifacts'] = {p.name: digest(p) for p in sorted(output.iterdir()) if p.is_file() and p.name != 'result.json'}
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(output / 'result.json')


if __name__ == '__main__':
    main()
