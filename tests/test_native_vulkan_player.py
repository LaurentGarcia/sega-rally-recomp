#!/usr/bin/env python3
"""Compare bounded real-game SDL/direct Vulkan runs using explicitly supplied local data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--data', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--visible', action='store_true', help='Present actual windows (use xvfb-run on headless Linux)')
    parser.add_argument('--timeout', type=int, default=900)
    args = parser.parse_args()
    output, executable, data = args.output.resolve(), args.executable.resolve(), args.data.resolve()
    if output.exists() or output == ROOT / 'build' or not output.is_relative_to(ROOT / 'build'):
        parser.error('output must be a new directory below ignored build/')
    if not executable.is_file() or not data.is_dir():
        parser.error('a playable executable and personally imported data are required')
    paths = [p for folder in ('src', 'config', 'platform/segarally', 'tools/srally_boot')
             for p in (ROOT / folder).rglob('*') if p.is_file() and p.suffix != '.pyc']
    paths += [Path(__file__).resolve(), executable]
    paths += [p for p in data.iterdir() if p.is_file()]
    before = {str(p): sha(p) for p in sorted(set(paths))}
    output.mkdir(parents=True)
    result = {'status': 'failed', 'inputs': before, 'visible': args.visible, 'runs': {},
              'scope': 'Same executable and native machine, fixed driving inputs, sound/clock/save/replay parity, '
                       'and final raw frame. Software Vulkan is not hardware performance or full arcade certification.'}
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SDL_', 'SEGARALLY_'))}
    env['SDL_GPU_DRIVER'] = 'vulkan'

    def execute(arm):
        directory = output / arm
        directory.mkdir()
        (directory / 'saves').mkdir()
        command = [str(executable), '--data', str(data), '--save-dir', str(directory / 'saves'),
                   '--unthrottled', '--sound', '--instruction-boundary-hook', '--crt', '--frames', '2600',
                   '--instructions', '600000000', '--input-replay', str(ROOT / 'config/segarally_lancia_drive.inputs'),
                   '--audio-capture', str(directory / 'audio.wav'), '--capture', str(directory / 'native.ppm'),
                   '--record-inputs', str(directory / 'recording'), '--log', str(directory / 'host.log')]
        if not args.visible:
            command += ['--hidden']
        if arm == 'sdl':
            command += ['--sdl-gpu']
        else:
            command += ['--native-vulkan', '--vulkan-validation']
        if arm == 'direct-accurate':
            command += ['--accurate-textures', '--reference-capture', str(directory / 'reference.ppm')]
        row = {'command': command}
        result['runs'][arm] = row
        start = time.monotonic()
        with (directory / 'stdout.txt').open('wb') as stdout, (directory / 'stderr.txt').open('wb') as stderr:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
            try:
                code = process.wait(timeout=args.timeout)
                row['returncode'] = code
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
                row['wall_seconds_not_a_benchmark'] = time.monotonic() - start
        text = (directory / 'host.log').read_text()
        logs = text + (directory / 'stderr.txt').read_text()
        assert not any(word in logs for word in ('Native frontier:', 'Validation Error', 'VUID-', 'SYNC-HAZARD'))
        terminal = re.search(r'Native host exit: frames=(\d+) instructions=(\d+) cycles=(\d+) pc=([0-9a-f]+) result=(\d+)', text)
        assert terminal and terminal[1] == '2600' and terminal[5] == '0', arm
        row['terminal'] = list(terminal.groups())
        assert 'events=19 applied=15 pending=4 manual_takeover=0' in text
        row['events'] = [line for line in text.splitlines() if line.startswith('Input replay event:')]
        if arm != 'sdl':
            assert 'GPU selected=direct-vulkan ' in text
            present = re.search(r'Direct Vulkan presentation: submissions=(\d+) presented_frames=(\d+) swapchains=(\d+) model2_pipelines=(\d+) crt_pipelines=(\d+) validation_errors=(\d+)', text)
            assert present and list(map(int, present.groups()[3:])) == [4, 1, 0]
            assert int(present[2]) >= 2500 if args.visible else int(present[2]) == 0
            row['presentation'] = list(map(int, present.groups()))
        else:
            assert 'GPU selected=vulkan ' in text
        print('Completed', arm, flush=True)

    try:
        for arm in ('sdl', 'direct', 'direct-accurate'):
            execute(arm)
        reference = output / 'sdl'
        for arm in ('direct', 'direct-accurate'):
            directory = output / arm
            row, baseline = result['runs'][arm], result['runs']['sdl']
            assert row['terminal'] == baseline['terminal'] and row['events'] == baseline['events']
            for name in ('audio.wav', 'saves/eeprom.bin', 'saves/backup.bin', 'recording/observations.txt',
                         'recording/eeprom.initial.bin', 'recording/backup.initial.bin',
                         'recording/eeprom.final.bin', 'recording/backup.final.bin'):
                actual, expected = directory / name, reference / name
                assert actual.read_bytes() == expected.read_bytes(), (arm, name)
            actual = (directory / 'recording/resumes.txt').read_text().splitlines()
            expected = (reference / 'recording/resumes.txt').read_text().splitlines()
            assert actual[0] == expected[0] and actual[1].split()[2:] == expected[1].split()[2:] and actual[2:] == expected[2:]
            expected_image = reference / 'native.ppm' if arm == 'direct' else directory / 'reference.ppm'
            assert (directory / 'native.ppm').read_bytes() == expected_image.read_bytes(), (arm, 'final image')
        assert before == {str(p): sha(p) for p in sorted(set(paths))}, 'source/executable/input mutation during run'
        result.update(status='passed', native_audio_saves_inputs_and_execution_equal=True,
                      direct_image_equals_sdl=True, accurate_image_equals_cpu=True)
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        result['artifacts'] = {str(p.relative_to(output)): sha(p) for p in output.rglob('*') if p.is_file()}
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(output / 'result.json')


if __name__ == '__main__':
    main()
