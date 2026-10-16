#!/usr/bin/env python3
"""ROM-free SDL/native Metal CRT equivalence; requires macOS and pinned SDL library.

The baseline is the existing CRT fixture with one readback export insertion.
The native test links Foundation/Metal only. Outputs are exclusive and retained.
This is a correctness fixture, not an SDL/native performance benchmark.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdl-library', type=pathlib.Path, required=True)
    parser.add_argument('--sdl-include', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    expected = out / 'sdl'
    actual = out / 'native'
    expected.mkdir()
    actual.mkdir()
    measured = [pathlib.Path(__file__).resolve(), root/'tests/test_native_metal_crt.mm',
                root/'tests/test_gpu_crt.cpp', root/'src/app/native/metal/metal_crt.h',
                root/'src/app/native/metal/metal_crt.mm', root/'src/app/gpu/crt_renderer.h',
                root/'src/app/gpu/crt_renderer.cpp', root/'src/app/gpu/crt_shaders_gen.h',
                root/'src/app/gpu/crt_settings.h', root/'src/app/gpu_device.h', args.sdl_library.resolve()]
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    before = {str(p): digest(p) for p in measured}
    source = (root/'tests/test_gpu_crt.cpp').read_text()
    marker = 'SDL_UnmapGPUTransferBuffer(device,b);SDL_ReleaseGPUTransferBuffer(device,b);return out;'
    assert source.count(marker) == 1, 'baseline readback export seam changed'
    export = '''
        char export_name[64];std::snprintf(export_name,sizeof(export_name),"fixture-%02u.bgra",draws);
        std::ofstream export_file(std::filesystem::path(NATIVE_CRT_EXPORT_DIRECTORY)/export_name,std::ios::binary);
        const uint32_t export_dimensions[2]={uint32_t(tw),uint32_t(th)};
        export_file.write(reinterpret_cast<const char *>(export_dimensions),sizeof(export_dimensions));
        export_file.write(reinterpret_cast<const char *>(out.data()),std::streamsize(out.size()*4));
        require(bool(export_file),"baseline export");
        '''
    (out/'baseline.cpp').write_text(source.replace(marker, export+marker))
    assert (out/'baseline.cpp').read_text().replace(export, '') == source
    flags = ['-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation', '-pedantic', '-I'+str(root/'src')]
    commands = []
    result = {'inputs_before': before, 'commands': commands, 'status': 'FAIL'}
    def run(name, command, timeout):
        start = time.monotonic()
        try:
            proc = subprocess.run(command, capture_output=True, timeout=timeout)
            (out/(name+'.stdout')).write_bytes(proc.stdout)
            (out/(name+'.stderr')).write_bytes(proc.stderr)
            commands.append({'name': name, 'command': command, 'exit': proc.returncode, 'wall_seconds': time.monotonic()-start})
            if proc.returncode:
                raise RuntimeError(name+' failed; preserved stdout/stderr')
        except subprocess.TimeoutExpired as exc:
            (out/(name+'.stdout')).write_bytes(exc.stdout or b'')
            (out/(name+'.stderr')).write_bytes(exc.stderr or b'')
            commands.append({'name': name, 'command': command, 'timeout': timeout})
            raise
    try:
        frameworks = ['CoreMedia', 'CoreVideo', 'Cocoa', 'UniformTypeIdentifiers', 'IOKit', 'ForceFeedback', 'Carbon', 'CoreAudio', 'AudioToolbox', 'AVFoundation', 'Foundation', 'GameController', 'Metal', 'QuartzCore', 'CoreHaptics']
        link = [part for framework in frameworks for part in ['-framework', framework]]
        run('compile-sdl', [args.compiler, *flags, '-I'+str(args.sdl_include.resolve()), '-DNATIVE_CRT_EXPORT_DIRECTORY='+json.dumps(str(expected)), str(out/'baseline.cpp'), str(root/'src/app/gpu/crt_renderer.cpp'), str(args.sdl_library.resolve()), *link, '-o', str(out/'sdl-fixture')], 90)
        run('compile-native', [args.compiler, *flags, '-fobjc-arc', str(root/'tests/test_native_metal_crt.mm'), str(root/'src/app/native/metal/metal_crt.mm'), '-framework', 'Foundation', '-framework', 'Metal', '-o', str(out/'native-fixture')], 90)
        run('sdl', [str(out/'sdl-fixture')], 30)
        run('native', [str(out/'native-fixture'), str(expected), str(actual)], 30)
        linkage = subprocess.check_output(['otool', '-L', str(out/'native-fixture')], text=True)
        (out/'native-linkage.txt').write_text(linkage)
        assert 'SDL' not in linkage, 'native fixture must not link SDL'
        result['status'] = 'PASS'
    finally:
        result['inputs_after'] = {str(p): digest(p) for p in measured}
        result['inputs_unchanged'] = result['inputs_after'] == before
        if not result['inputs_unchanged']:
            result['status'] = 'FAIL'
        result['artifacts'] = {str(p.relative_to(out)): digest(p) for p in out.rglob('*') if p.is_file() and p.name != 'result.json'}
        (out/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(result['status'], out/'result.json')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
