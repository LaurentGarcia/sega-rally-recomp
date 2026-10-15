#!/usr/bin/env python3
"""Optional ROM-free Metal regression; preparation never opens a GPU.

From a clean checkout with the pinned extern/sdl3 source present:
  python3 tests/test_gpu_reciprocal.py --prepare --output build/reciprocal-test
  python3 tests/test_gpu_reciprocal.py --run --output build/reciprocal-test

Preparation needs CMake, a C++20 compiler, dxc and spirv-cross on PATH.
Alternatively --tool-container NAME uses an EXISTING Linux tool container;
--dxc and --spirv-cross name its executables. It never pulls/downloads tools.
Run is explicit, macOS Metal only: two draws/readbacks, 30-second watchdog.
"""
from pathlib import Path
import argparse, ast, hashlib, json, os, signal, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]


def identity(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def write(path, value):
    with path.open('x') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def sdl_provenance():
    """Read the tracked setup pin without importing/executing setup.py.

    Accept pristine SDL or exactly the documented one-hunk Vulkan patch.
    This is source identity; it does not establish device correctness.
    """
    setup = ROOT/'scripts/setup.py'
    assignments = [node for node in ast.parse(setup.read_text()).body
                   if isinstance(node, ast.Assign)
                   and any(isinstance(t, ast.Name) and t.id == 'SDL3' for t in node.targets)]
    assert len(assignments) == 1, 'single canonical SDL3 setup pin required'
    url, expected = ast.literal_eval(assignments[0].value)
    sdl = ROOT/'extern/sdl3'
    revision = subprocess.check_output(['git', '-C', str(sdl), 'rev-parse', 'HEAD'], text=True).strip()
    assert revision == expected, 'SDL HEAD differs from tracked setup pin'
    status = subprocess.check_output(['git', '-C', str(sdl), 'status', '--porcelain', '--untracked-files=normal'], text=True)
    diff = subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--binary'])
    scope = 'pristine pinned source'
    patch = ROOT/'patches/sdl3/0001-vulkan-application-name.patch'
    if status or diff:
        relative = 'src/gpu/vulkan/SDL_gpu_vulkan.c'
        names = subprocess.check_output(['git', '-C', str(sdl), 'diff', 'HEAD', '--name-only'], text=True).splitlines()
        assert names == [relative] and all(line[3:] == relative for line in status.splitlines()), 'SDL changes outside documented patch scope'
        lines = patch.read_text().splitlines(keepends=True)
        assert sum(line.startswith('@@') for line in lines) == 1, 're-review changed SDL patch format'
        body = lines[next(i for i, line in enumerate(lines) if line.startswith('@@'))+1:]
        old = ''.join(line[1:] for line in body if line.startswith((' ', '-'))).encode()
        new = ''.join(line[1:] for line in body if line.startswith((' ', '+'))).encode()
        original = subprocess.check_output(['git', '-C', str(sdl), 'show', 'HEAD:'+relative])
        assert original.count(old) == 1, 'documented patch context differs from pinned SDL'
        assert (sdl/relative).read_bytes() == original.replace(old, new, 1), 'SDL content differs from documented patch'
        scope = 'exact documented Vulkan application-name patch'
    return {'revision': revision, 'expected_setup_revision': expected, 'upstream_url': url,
            'setup_source': identity(setup), 'third_party_document': identity(ROOT/'THIRD_PARTY.md'),
            'documented_patch': identity(patch), 'porcelain_status': status,
            'tracked_diff': {'bytes': len(diff), 'sha256': hashlib.sha256(diff).hexdigest(), 'utf8': diff.decode()},
            'scope': scope}


def prepare(args):
    out = args.output
    out.mkdir(parents=True, exist_ok=False)
    sources = [ROOT/'tests/test_gpu_reciprocal.cpp', ROOT/'tests/test_gpu_reciprocal.hlsl',
               ROOT/'src/app/gpu/normal_reciprocal.hlsl', ROOT/'src/app/gpu_device.h',
               Path(__file__).resolve()]
    before = {str(p): identity(p) for p in sources}
    commands = []

    def call(argv):
        commands.append(argv)
        (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        process = subprocess.Popen(argv, cwd=ROOT, start_new_session=True)
        try:
            process.wait(timeout=180)
            if process.returncode:
                raise subprocess.CalledProcessError(process.returncode, argv)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()

    def query(argv):
        result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, timeout=30)
        return {'argv': argv, 'exit_code': result.returncode,
                'stdout': result.stdout, 'stderr': result.stderr}

    provenance = {'SDL': sdl_provenance(),
                  'host_compiler': query(['/usr/bin/c++', '--version']),
                  'cmake': query(['cmake', '--version'])}

    staged = out/'input'
    (staged/'app/gpu').mkdir(parents=True)
    (staged/'fixture.hlsl').write_bytes(sources[1].read_bytes())
    (staged/'app/gpu/normal_reciprocal.hlsl').write_bytes(sources[2].read_bytes())
    shaders = out/'shaders'
    shaders.mkdir()
    remote = '/tmp/segarally-reciprocal-'+hashlib.sha256(str(out).encode()).hexdigest()[:16]
    tool_input, tool_output = str(staged), str(shaders)
    if args.tool_container:
        call(['docker', 'exec', args.tool_container, 'mkdir', remote])
        call(['docker', 'cp', str(staged), args.tool_container+':'+remote+'/input'])
        call(['docker', 'exec', args.tool_container, 'mkdir', remote+'/shaders'])
        tool_input, tool_output = remote+'/input', remote+'/shaders'
    prefix = ['docker', 'exec'] if args.tool_container else []
    if args.tool_container and args.tool_lib_dir:
        prefix += ['-e', 'LD_LIBRARY_PATH='+args.tool_lib_dir]
    if args.tool_container:
        prefix += [args.tool_container]
        provenance['container_image'] = query(['docker', 'inspect', '--format', '{{.Image}}', args.tool_container])
    provenance['dxc'] = query(prefix+[args.dxc, '--version'])
    provenance['spirv_cross'] = query(prefix+[args.spirv_cross, '--revision'])
    write(out/'tool-provenance.json', provenance)
    for entry, profile in [('vs_arithmetic', 'vs_6_0'), ('ps_arithmetic', 'ps_6_0'), ('ps_baseline', 'ps_6_0')]:
        base = tool_output+'/'+entry
        # Same semantic flags as scripts/build_shaders.py; common HLSL only.
        call(prefix+[args.dxc, '-spirv', '-fspv-target-env=vulkan1.0', '-fspv-entrypoint-name=main',
                     '-I', tool_input, '-T', profile, '-E', entry, '-Fo', base+'.spv', tool_input+'/fixture.hlsl'])
        call(prefix+[args.dxc, '-I', tool_input, '-T', profile, '-E', entry, '-Fo', base+'.dxil', tool_input+'/fixture.hlsl'])
        call(prefix+[args.spirv_cross, base+'.spv', '--msl', '--msl-version', '20100', '--output', base+'.metal'])
    if args.tool_container:
        call(['docker', 'cp', args.tool_container+':'+remote+'/shaders/.', str(shaders)])
    # A standalone SDL-only CMake project avoids ROM/generator/host dependencies
    # and leaves the repository's CMake configuration unchanged.
    project = out/'project'
    project.mkdir()
    quote = lambda p: str(p).replace('\\', '/').replace('"', '\\"')
    (project/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
project(reciprocal_regression LANGUAGES C CXX)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("'''+quote(ROOT/'extern/sdl3')+'" "'+quote(out/'host/sdl3')+'''" EXCLUDE_FROM_ALL SYSTEM)
add_executable(test_gpu_reciprocal "'''+quote(sources[0])+'''")
target_compile_features(test_gpu_reciprocal PRIVATE cxx_std_20)
target_compile_options(test_gpu_reciprocal PRIVATE -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off)
target_include_directories(test_gpu_reciprocal PRIVATE "'''+quote(ROOT/'src')+'''")
target_link_libraries(test_gpu_reciprocal PRIVATE SDL3::SDL3-static "-framework Metal" "-framework QuartzCore")
''')
    call(['cmake', '-S', str(project), '-B', str(out/'host'), '-DCMAKE_BUILD_TYPE=Release',
          '-DCMAKE_CXX_COMPILER=/usr/bin/c++', '-DCMAKE_C_COMPILER=/usr/bin/cc'])
    call(['cmake', '--build', str(out/'host'), '--target', 'test_gpu_reciprocal', '--parallel', '2'])
    assert before == {str(p): identity(p) for p in sources}, 'source changed during preparation'
    binary = out/'host/test_gpu_reciprocal'
    assert provenance['SDL'] == sdl_provenance(), 'SDL source changed during preparation'
    artifacts = [binary, out/'tool-provenance.json', *sorted(shaders.iterdir())]
    write(out/'prepared.json', {'status': 'PREPARED_NOT_EXECUTED', 'sources': before,
          'artifacts': {str(p): identity(p) for p in artifacts}, 'binary': str(binary),
          'scope': 'Sampled public-input arithmetic; no game, scene, cost or other-backend execution claim'})
    print('Prepared only:', out/'prepared.json')


def run(args):
    out = args.output
    prepared = json.loads((out/'prepared.json').read_text())

    def verify():
        for group in ['sources', 'artifacts']:
            for path, expected in prepared[group].items():
                assert identity(Path(path)) == expected, path

    verify()
    result = out/'run-01'
    result.mkdir(exist_ok=False)
    command = [prepared['binary'], str(out), str(result/'readback')]
    write(result/'invocation.json', {'argv': command, 'timeout_seconds': 30})
    process = None
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('SDL_', 'SEGARALLY_'))}
    try:
        with (result/'stdout.txt').open('xb') as stdout, (result/'stderr.txt').open('xb') as stderr:
            process = subprocess.Popen(command, cwd=ROOT, env=environment,
                                       stdout=stdout, stderr=stderr, start_new_session=True)
            process.wait(timeout=30)
        assert process.returncode == 0, 'GPU fixture failed; no retry'
        verify()
        print((result/'readback/result.json').read_text())
    finally:
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        write(result/'process.json', {'returncode': None if process is None else process.returncode,
              'scope': 'Watchdog/correctness only; not a performance result'})
        verify()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare', action='store_true')
    mode.add_argument('--run', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dxc', default='dxc')
    parser.add_argument('--spirv-cross', default='spirv-cross')
    parser.add_argument('--tool-container')
    parser.add_argument('--tool-lib-dir', help='optional LD_LIBRARY_PATH inside an existing tool container')
    args = parser.parse_args()
    args.output = args.output.resolve()
    if sys.platform != 'darwin':
        parser.error('macOS Metal fixture only')
    if args.tool_lib_dir and not args.tool_container:
        parser.error('--tool-lib-dir requires --tool-container')
    if not args.output.is_relative_to(ROOT/'build'):
        parser.error('output must be inside this checkout build/')
    prepare(args) if args.prepare else run(args)


if __name__ == '__main__':
    main()
