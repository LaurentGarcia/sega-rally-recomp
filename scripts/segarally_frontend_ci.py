#!/usr/bin/env python3
"""Compile the real Rally frontend without ROMs, using the tested CI toolchain.

Only original link fixtures are supplied. No game or redistributable player is
produced, and nothing is uploaded. Requires pinned dependencies from setup.py.
"""
import argparse
import json
import re
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def cache(path):
    result = {}
    for line in path.read_text().splitlines():
        if line and not line.startswith(('#', '//')) and ':' in line and '=' in line:
            key, value = line.split('=', 1)
            result[key.split(':', 1)[0]] = value
    return result

def toolchain(build):
    result = cache(build/'CMakeCache.txt')
    # Visual Studio does not promise compiler paths as cache entries. CMake's
    # generated compiler metadata records the actual selected compiler for all
    # generators; read it as data, never execute its CMake commands.
    for language in ('C','CXX'):
        files = list((build/'CMakeFiles').glob(f'*/CMake{language}Compiler.cmake'))
        if len(files) != 1:
            raise ValueError(f'Expected one actual {language} compiler metadata file in {build}')
        prefix = f'CMAKE_{language}_COMPILER'
        fields = dict(re.findall(r'^set\((CMAKE_'+language+r'_COMPILER(?:_ID|_VERSION)?) "([^"\n]*)"\)\s*$',
                                 files[0].read_text(),re.MULTILINE))
        for key in (prefix,prefix+'_ID',prefix+'_VERSION'):
            if not fields.get(key): raise ValueError(f'Missing actual compiler metadata: {key}')
        if result.get(prefix) and result[prefix] != fields[prefix]:
            raise ValueError(f'Compiler cache and actual metadata disagree: {prefix}')
        result.update(fields)
    return result

def verify_toolchain(base,actual):
    keys = ('CMAKE_GENERATOR','CMAKE_GENERATOR_TOOLSET','CMAKE_GENERATOR_PLATFORM',
            'CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER_ID','CMAKE_CXX_COMPILER_ID',
            'CMAKE_C_COMPILER_VERSION','CMAKE_CXX_COMPILER_VERSION')
    for key in keys:
        if actual.get(key,'') != base.get(key,''): raise RuntimeError(f'Frontend toolchain differs: {key}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain-build', type=Path, default=ROOT/'build')
    parser.add_argument('--output', type=Path, default=ROOT/'build/rally-frontend')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() or not out.is_relative_to(ROOT/'build'):
        parser.error('output must be a new directory beneath ignored build/')
    base = toolchain(args.toolchain_build)
    command = ['cmake','-S',str(ROOT/'platform/segarally'),'-B',str(out),'-G',base['CMAKE_GENERATOR'],
               '-DCMAKE_BUILD_TYPE=Release','-DSEGARALLY_FRONTEND_COMPILE_ONLY=ON',
               '-DSEGARALLY_SOUND=ON','-DSEGARALLY_INPUT_RECORDING=ON','-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON']
    for key in ('CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_GENERATOR_INSTANCE','CMAKE_MAKE_PROGRAM'):
        if base.get(key): command.append(f'-D{key}={base[key]}')
    for key, flag in (('CMAKE_GENERATOR_PLATFORM','-A'),('CMAKE_GENERATOR_TOOLSET','-T')):
        if base.get(key): command.extend([flag,base[key]])
    subprocess.run(command,cwd=ROOT,check=True,timeout=300)
    actual = toolchain(out)
    verify_toolchain(base,actual)
    subprocess.run(['cmake','--build',str(out),'--config','Release','--parallel','2','--target',
                    'segarally_frontend_compile','test_segarally_audio','test_segarally_controls',
                    'test_segarally_display_preferences','test_segarally_audio_output_capture',
                    'test_segarally_input_replay','test_segarally_platform_files','test_segarally_rom_import'],cwd=ROOT,check=True,timeout=900)
    subprocess.run(['ctest','--test-dir',str(out),'-C','Release','--output-on-failure'],cwd=ROOT,check=True,timeout=300)
    (out/'frontend-ci-result.json').write_text(json.dumps({'status':'passed',
        'scope':'ROM-free real frontend compile/link and host tests only; not playable or Vulkan/device validation',
        'toolchain':{k:actual[k] for k in ('CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER_ID','CMAKE_CXX_COMPILER_ID','CMAKE_C_COMPILER_VERSION','CMAKE_CXX_COMPILER_VERSION','CMAKE_GENERATOR')},
        'fixture':'tests/fixtures/segarally_frontend_link.cpp','uploaded':False},indent=2)+'\n')

if __name__=='__main__': main()
