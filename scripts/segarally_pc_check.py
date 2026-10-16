#!/usr/bin/env python3
"""Record a bounded Windows/Linux player check without rebuilding the game.

Python 3.10+ is needed for this diagnostic tool only. The player itself needs no
development tools. Outputs include private game captures and stay local.
"""
from __future__ import annotations
import argparse
import array
import ctypes
import ctypes.util
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shlex
import signal
import struct
import subprocess
import sys
import time


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def vulkan_inventory():
    """Query loader-reported device names/types; no surface, GPU work or SDK."""
    result = {'devices': [], 'scope': 'Vulkan loader inventory, not proof of the GPU selected by SDL or a physical display.'}
    instance = ctypes.c_void_p()
    library = None
    try:
        if os.name == 'nt':
            library = ctypes.WinDLL('vulkan-1.dll')
        else:
            library = ctypes.CDLL(ctypes.util.find_library('vulkan') or 'libvulkan.so.1')

        class Application(ctypes.Structure):
            _fields_ = [('sType', ctypes.c_uint32), ('pNext', ctypes.c_void_p),
                        ('name', ctypes.c_char_p), ('version', ctypes.c_uint32),
                        ('engine', ctypes.c_char_p), ('engineVersion', ctypes.c_uint32),
                        ('apiVersion', ctypes.c_uint32)]

        class InstanceInfo(ctypes.Structure):
            _fields_ = [('sType', ctypes.c_uint32), ('pNext', ctypes.c_void_p),
                        ('flags', ctypes.c_uint32), ('application', ctypes.POINTER(Application)),
                        ('layerCount', ctypes.c_uint32), ('layers', ctypes.c_void_p),
                        ('extensionCount', ctypes.c_uint32), ('extensions', ctypes.c_void_p)]

        library.vkCreateInstance.argtypes = [ctypes.POINTER(InstanceInfo), ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
        library.vkCreateInstance.restype = ctypes.c_int32
        library.vkDestroyInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        library.vkDestroyInstance.restype = None
        library.vkEnumeratePhysicalDevices.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_void_p)]
        library.vkEnumeratePhysicalDevices.restype = ctypes.c_int32
        library.vkGetPhysicalDeviceProperties.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        library.vkGetPhysicalDeviceProperties.restype = None
        app = Application(0, None, b'Sega Rally player check', 1, None, 0, 1 << 22)
        info = InstanceInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)
        code = library.vkCreateInstance(ctypes.byref(info), None, ctypes.byref(instance))
        if code:
            raise RuntimeError('vkCreateInstance returned ' + str(code))
        count = ctypes.c_uint32()
        code = library.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), None)
        if code or not 0 < count.value <= 128:
            raise RuntimeError('Invalid Vulkan device enumeration: ' + str(code))
        devices = (ctypes.c_void_p * count.value)()
        capacity = count.value
        code = library.vkEnumeratePhysicalDevices(instance, ctypes.byref(count), devices)
        if code or count.value > capacity:
            raise RuntimeError('Vulkan inventory changed during enumeration: ' + str(code))
        for device in devices[:count.value]:
            # Vulkan 1.0's fixed VkPhysicalDeviceProperties begins with five
            # uint32 fields and a 256-byte name. Oversized, 64-bit-aligned storage
            # retains the complete properties structure without copying its
            # unused implementation limits into this diagnostic tool.
            storage = (ctypes.c_uint64 * 1024)()
            library.vkGetPhysicalDeviceProperties(device, ctypes.byref(storage))
            data = bytes(storage)
            api, driver, vendor, ident, kind = struct.unpack_from('=5I', data)
            result['devices'].append({'name': data[20:276].split(b'\0', 1)[0].decode('utf-8', 'replace'),
                                      'type': {0: 'other', 1: 'integrated', 2: 'discrete', 3: 'virtual', 4: 'cpu'}.get(kind, 'unknown'),
                                      'vendor_id': vendor, 'device_id': ident,
                                      'api_version': api, 'driver_version': driver})
    except (OSError, AttributeError, RuntimeError) as error:
        result['error'] = str(error)
    finally:
        if instance.value and library is not None:
            library.vkDestroyInstance(instance, None)
    return result


def bounded_vulkan_inventory():
    options = {'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == 'nt' else {'start_new_session': True}
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--vulkan-probe'],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, **options)
    try:
        stdout, stderr = process.communicate(timeout=15)
        if process.returncode:
            return {'devices': [], 'error': 'Vulkan probe failed: ' + stderr[-2048:]}
        return json.loads(stdout)
    except (subprocess.TimeoutExpired, ValueError) as error:
        if process.poll() is None:
            terminate(process)
        return {'devices': [], 'error': 'Vulkan probe did not complete: ' + str(error)}


def environment(environ):
    container = any(Path(p).exists() for p in ('/.dockerenv', '/run/.containerenv'))
    wine = False
    remote = None
    if os.name == 'nt':
        try:
            wine = hasattr(ctypes.WinDLL('ntdll'), 'wine_get_version')
            remote = bool(ctypes.WinDLL('user32').GetSystemMetrics(0x1000))
        except (OSError, AttributeError):
            pass
    return {'os': platform.system(), 'release': platform.release(), 'architecture': platform.machine(),
            'container_marker': container, 'wine_detected': wine, 'windows_remote_session': remote,
            'overrides': {k: value for k, value in sorted(environ.items())
                          if k.startswith(('SDL_', 'SEGARALLY_', 'VK_', 'MESA_', 'LIBGL_', 'GALLIUM_')) or k in ('DRI_PRIME', 'DISPLAY', 'WAYLAND_DISPLAY')},
            'vulkan': bounded_vulkan_inventory()}


def classify_device(name, inventory):
    lowered = name.lower()
    if any(word in lowered for word in ('llvmpipe', 'lavapipe', 'swiftshader', 'software rasterizer')):
        return 'software'
    types = {device['type'] for device in inventory.get('devices', []) if device['name'] == name}
    if types == {'cpu'}:
        return 'software'
    if types and types <= {'integrated', 'discrete'}:
        return 'driver_reported_hardware'
    if types == {'virtual'}:
        return 'virtual'
    return 'unknown'


def terminate(process):
    if os.name == 'nt':
        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True, timeout=15)
        process.wait(timeout=15)
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()


def execute(command, directory, env, timeout):
    start = time.monotonic()
    options = {'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == 'nt' else {'start_new_session': True}
    with (directory / 'stdout.txt').open('wb') as stdout, (directory / 'stderr.txt').open('wb') as stderr:
        process = subprocess.Popen(command, cwd=directory, env=env, stdout=stdout, stderr=stderr, **options)
        try:
            process.wait(timeout=timeout)
        except BaseException:
            if process.poll() is None:
                terminate(process)
            raise
    return {'command': command, 'returncode': process.returncode,
            'elapsed_seconds_not_a_benchmark': time.monotonic() - start}


def wav_metrics(path):
    if path.stat().st_size > 512 * 1024 * 1024:
        raise ValueError('Audio capture exceeds the bounded diagnostic size')
    data = path.read_bytes()
    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'WAVE' or struct.unpack_from('<I', data, 4)[0] + 8 != len(data):
        raise ValueError('Incomplete WAV capture')
    chunks = {}
    offset = 12
    while offset + 8 <= len(data):
        name, size = struct.unpack_from('<4sI', data, offset)
        offset += 8
        if offset + size > len(data) or name in chunks:
            raise ValueError('Invalid or duplicate WAV chunk')
        chunks[name] = data[offset:offset + size]
        offset += size + (size & 1)
    if offset != len(data) or len(chunks.get(b'fmt ', b'')) < 16 or b'data' not in chunks:
        raise ValueError('Missing WAV format/data')
    kind, channels, rate, byte_rate, alignment, bits = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
    samples = chunks[b'data']
    if kind != 3 or bits != 32 or not 1 <= channels <= 32 or rate == 0 or alignment != channels * 4 or byte_rate != rate * alignment or len(samples) % alignment:
        raise ValueError('Expected complete float32 PCM frames')
    values = array.array('f', samples)
    if sys.byteorder != 'little':
        values.byteswap()
    if not all(math.isfinite(value) for value in values):
        raise ValueError('Non-finite audio capture')
    return {'frames': len(values) // channels, 'channels': channels, 'rate': rate,
            'nonzero_samples': sum(value != 0 for value in values),
            'peak': max((abs(value) for value in values), default=0), 'sha256': sha(path)}


def input_metrics(path):
    lines = path.read_text().splitlines()
    if not lines or lines[0] != 'segarally-host-resume-v1' or not lines[-1].startswith('end '):
        raise ValueError('Input recording is incomplete')
    pins = []
    for line in lines:
        if line.startswith('resume '):
            values = line.split()
            if len(values) != 26 or values[-1] not in ('frame', 'budget'):
                raise ValueError('Unexpected resume record or game frontier')
            pin = tuple(int(value, 16) for value in values[8:20])
            if not all(0 <= value <= 255 for value in pin):
                raise ValueError('Invalid cabinet input byte')
            pins.append(pin)
    if not pins:
        raise ValueError('No cabinet inputs recorded')
    return {'resume_records': len(pins), 'distinct_cabinet_inputs': len(set(pins)),
            'coin': any(not p[0] & 1 for p in pins), 'start': any(not p[0] & 0x40 for p in pins),
            'steer_left': any(p[4] < 112 for p in pins), 'steer_right': any(p[4] > 144 for p in pins),
            'gas': any(p[5] > 32 for p in pins), 'brake': any(p[6] > 32 for p in pins),
            'scope': 'Cabinet pin changes; keyboard, gamepad and injected input cannot be distinguished from this recording alone.'}


def recording_seal(directory, terminal, records):
    """Check the existing recording_bundle.cpp v1 completion contract, no replay."""
    path = directory / 'complete.txt'
    if path.stat().st_size > 8192:
        raise ValueError('Recording seal exceeds its contract bound')
    lines = path.read_text().splitlines()
    if len(lines) != 9 or lines[0] != 'segarally-recording-complete-v1' or lines[-1] != 'end':
        raise ValueError('Incomplete or unknown recording seal')
    files = (('manifest', 'manifest.txt', 8192), ('transcript', 'resumes.txt', 64 * 1024 * 1024),
             ('observations', 'observations.txt', 64 * 1024 * 1024),
             ('eeprom_final', 'eeprom.final.bin', 128), ('backup_final', 'backup.final.bin', 16384))
    bound = {}
    for line, (key, name, limit) in zip(lines[1:6], files):
        match = re.fullmatch(key + r' (\d+) ([0-9a-f]{64})', line)
        actual = directory / name
        if not match or int(match[1]) > limit or actual.stat().st_size != int(match[1]) or sha(actual) != match[2]:
            raise ValueError('Recording seal hash/size mismatch: ' + name)
        bound[name] = match[2]
    if (directory / 'eeprom.final.bin').stat().st_size != 128 or (directory / 'backup.final.bin').stat().st_size != 16384:
        raise ValueError('Recording final save size mismatch')
    if lines[6] != 'records ' + str(records) or not 0 < records <= 250000:
        raise ValueError('Recording seal resume count mismatch')
    match = re.fullmatch(r'terminal (quit|frame_limit|instruction_limit) (\d+) (\d+) (\d+) (\d+) ([0-9a-f]{8})', lines[7])
    if not match or [match[5], match[2], match[3], match[6], '0'] != terminal:
        raise ValueError('Recording seal terminal disagrees with game exit')
    transcript = (directory / 'resumes.txt').read_text().splitlines()
    if transcript[1] != 'context ' + bound['manifest.txt'] + ' 1 1':
        raise ValueError('Transcript context does not bind the completed sound/boundary manifest')
    if transcript[-1] != 'end ' + str(records) + ' ' + ' '.join(match.groups()[1:]):
        raise ValueError('Transcript footer differs from the completion witness')
    observations = (directory / 'observations.txt').read_text().splitlines()
    if observations[0] != 'segarally-recording-observations-v1' or observations[-1] != 'end ' + str(records):
        raise ValueError('Observation completion mismatch')
    if sum(line.startswith('resume ') for line in observations) != records:
        raise ValueError('Observation count differs from completed transcript')
    manifest = (directory / 'manifest.txt').read_text()
    for key, name, size in (('eeprom_initial', 'eeprom.initial.bin', 128), ('backup_initial', 'backup.initial.bin', 16384)):
        actual = directory / name
        if actual.stat().st_size != size or key + ' ' + str(size) + ' ' + sha(actual) not in manifest.splitlines():
            raise ValueError('Recording initial save does not match manifest')
    assets = directory / 'assets.json'
    if 'assets ' + str(assets.stat().st_size) + ' ' + sha(assets) not in manifest.splitlines():
        raise ValueError('Recording asset manifest does not match its identity')
    executable = re.search(r'^executable (\d+) ([0-9a-f]{64})$', manifest, re.M)
    if not executable:
        raise ValueError('Recording executable identity missing')
    return {'sha256': sha(path), 'sealed_artifacts': bound, 'records': records,
            'terminal_kind': match[1], 'recorded_executable_sha256': executable[2],
            'scope': 'Artifact integrity and completion checked; this is not execution through an independent reference machine.'}


def parse_run(directory, renderer, inventory):
    log = (directory / 'host.log').read_text(errors='replace')
    combined = log + (directory / 'stderr.txt').read_text(errors='replace')
    if any(word in combined for word in ('Native frontier:', 'Validation Error', 'VUID-', 'SYNC-HAZARD')):
        raise ValueError('Game frontier or Vulkan validation error; see retained logs')
    match = re.search(r'Native host exit: frames=(\d+) instructions=(\d+) cycles=(\d+) pc=([0-9a-f]+) result=(\d+)', log)
    if not match or match[5] != '0' or int(match[1]) == 0:
        raise ValueError('Missing successful game exit')
    display = re.search(r'Display controls: .*effects=(\d+) accurate_textures=(\d+)', log)
    row = {'terminal': list(match.groups()), 'crt_initially_enabled': bool(display and display[1] == '1'),
           'accurate_textures': bool(display and display[2] == '1')}
    if renderer == 'native':
        match = re.search(r'GPU selected=direct-vulkan device=(.*?) validation=(\d+);', log)
        if not match:
            raise ValueError('Direct Vulkan was not selected')
        row['selected_device'] = match[1]
        row['device_classification'] = classify_device(match[1], inventory)
        row['validation_layer_enabled'] = match[2] == '1'
        match = re.search(r'Direct Vulkan presentation: submissions=(\d+) presented_frames=(\d+) swapchains=(\d+) model2_pipelines=(\d+) crt_pipelines=(\d+) validation_errors=(\d+)', log)
        if not match or int(match[2]) == 0 or int(match[6]) != 0:
            raise ValueError('Missing successful native window presentation')
        row['presentation'] = list(map(int, match.groups()))
    else:
        if 'GPU selected=vulkan ' not in log:
            raise ValueError('SDL Vulkan was not selected')
        row['device_classification'] = 'unknown'
        row['device_note'] = 'The released SDL host logs its API but not its selected physical device.'
    match = re.search(r'Controller: (.*?) enabled=(\d+);', log)
    row['controller_at_start'] = match[1] if match else 'unknown'
    row['inputs'] = input_metrics(directory / 'recording/resumes.txt')
    row['recording_seal'] = recording_seal(directory / 'recording', row['terminal'], row['inputs']['resume_records'])
    row['native_audio'] = wav_metrics(directory / 'native.wav')
    row['logical_output_audio'] = wav_metrics(directory / 'output.wav')
    events = (directory / 'output.wav.events.txt').read_text()
    match = re.search(r'^end (\d+) (\d+) (\d+) (\d+) (\d+) ', events, re.M)
    if not match or any(int(value) for value in match.groups()[2:]):
        raise ValueError('Output audio capture incomplete, overflowed or changed format')
    for key in ('driver', 'device'):
        match = re.search('^' + key + r' (.*)$', events, re.M)
        row['audio_' + key] = match[1] if match else 'unknown'
    match = re.search(r'Native audio: .*chip_dropped=(\d+) host_dropped=(\d+) queued=\d+ device_shortfall_requests=(\d+)', log)
    if not match:
        raise ValueError('Missing native audio diagnostics')
    row['audio_delivery'] = dict(zip(('chip_dropped', 'host_dropped', 'device_shortfall_requests'), map(int, match.groups())))
    image = (directory / 'native.ppm').read_bytes()
    match = re.match(rb'P6\n(\d+) (\d+)\n255\n', image)
    if not match or len(image) - match.end() != int(match[1]) * int(match[2]) * 3:
        raise ValueError('Incomplete raw frame capture')
    row['raw_image_sha256'] = sha(directory / 'native.ppm')
    row['raw_image_dimensions'] = [int(match[1]), int(match[2])]
    row['saves'] = {}
    for name in ('eeprom', 'backup'):
        path = directory / 'saves' / (name + '.bin')
        if not path.is_file() or not path.stat().st_size or path.read_bytes() != (directory / 'recording' / (name + '.final.bin')).read_bytes():
            raise ValueError('Final save does not match the completed recording')
        row['saves'][name] = sha(path)
    row['clean_exit_and_save_finalization'] = True
    return row


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True, help='Released segarally_native executable, not a launcher script')
    data = parser.add_mutually_exclusive_group()
    data.add_argument('--rom', type=Path, help='Personally owned supported ZIP; import into the new diagnostic directory')
    data.add_argument('--data', type=Path, help='Already verified game image directory')
    parser.add_argument('--output', type=Path, required=True, help='New directory; existing saves and results are preserved')
    parser.add_argument('--renderers', choices=('both', 'native', 'sdl'), default='both')
    parser.add_argument('--interactive', action='store_true', help='Record your keyboard/controller session instead of scripted driving')
    parser.add_argument('--input-replay', type=Path, default=Path(__file__).resolve().parents[1] / 'config/segarally_lancia_drive.inputs')
    parser.add_argument('--frames', type=int, help='Default 2800 scripted or 7200 interactive; 120..18000')
    parser.add_argument('--timeout', type=int, default=600, help='Wall-clock limit per process, seconds')
    parser.add_argument('--validation', action='store_true', help='Require installed Khronos synchronization validation for native Vulkan')
    parser.add_argument('--expect-hardware', action='store_true', help='Require a selected native device reported as integrated/discrete, not software/virtual/unknown')
    parser.add_argument('--audio-driver', help='Explicit SDL audio driver for diagnostic environments, e.g. dummy; recorded in result')
    args = parser.parse_args(argv)
    if platform.system() not in ('Windows', 'Linux'):
        parser.error('Run this check on the target Windows/Linux PC; macOS execution is not PC validation')
    player = args.player.expanduser().resolve()
    output = args.output.expanduser().resolve()
    frames = args.frames or (7200 if args.interactive else 2800)
    if not player.is_file() or output.exists() or not output.parent.is_dir() or not 120 <= frames <= 18000 or not 10 <= args.timeout <= 3600:
        parser.error('Use an existing player, a new output directory under an existing parent, and bounded frames/timeout')
    if args.expect_hardware and args.renderers == 'sdl':
        parser.error('--expect-hardware requires native or both renderers; the released SDL host does not identify its selected device')
    replay = args.input_replay.expanduser().resolve()
    if not args.interactive and not replay.is_file():
        parser.error('Supply --input-replay pointing to config/segarally_lancia_drive.inputs')
    game_data = args.data.expanduser().resolve() if args.data else player.parent / 'assets'
    rom = args.rom.expanduser().resolve() if args.rom else None
    if (rom and not rom.is_file()) or (not rom and not game_data.is_dir()):
        parser.error('Supply your own --rom ZIP or --data directory (personal packages can use adjacent assets)')
    output.mkdir()
    env = os.environ.copy()
    env['SDL_GPU_DRIVER'] = 'vulkan'
    if args.audio_driver:
        env['SDL_AUDIO_DRIVER'] = args.audio_driver
    result = {'status': 'failed', 'environment': environment(env), 'runs': {}, 'findings': [],
              'mode': 'interactive' if args.interactive else 'scripted', 'frame_limit': frames,
              'player_sha256': sha(player), 'runner_sha256': sha(Path(__file__)),
              'audio_driver_override': args.audio_driver, 'physical_speaker_verified': False,
              'scope': 'Bounded local executable, Vulkan presentation, cabinet input, PCM, logical audio delivery and save evidence. No physical speaker, controller-source, full-race or arcade certification is inferred.'}
    if not args.interactive:
        result['replay_sha256'] = sha(replay)
    if rom:
        result['rom_sha256'] = sha(rom)
    try:
        if rom:
            directory = output / 'import'
            directory.mkdir()
            result['import'] = execute([str(player), '--player', '--rom', str(rom), '--import-only', '--save-dir', str(directory / 'saves')], directory, env, args.timeout)
            if result['import']['returncode']:
                raise ValueError('ROM import failed; see import logs')
            cache = directory / 'saves/imported-game'
            relative = Path((cache / 'current.txt').read_text().strip())
            if relative.is_absolute() or len(relative.parts) != 1 or relative.name in ('.', '..'):
                raise ValueError('Invalid import cache reference')
            game_data = cache / relative
        manifest = json.loads((game_data / 'assets.json').read_text())
        identities = {}
        for name, item in manifest['images'].items():
            path = game_data / item['file']
            if path.parent != game_data or path.stat().st_size != item['size'] or sha(path) != item['sha256']:
                raise ValueError('Asset identity mismatch: ' + name)
            identities[item['file']] = sha(path)
        if len(identities) != 8:
            raise ValueError('Expected eight verified game images')
        result['game_image_sha256'] = identities
        renderers = ('sdl', 'native') if args.renderers == 'both' else (args.renderers,)
        for renderer in renderers:
            directory = output / renderer
            directory.mkdir()
            command = [str(player), '--player', '--data', str(game_data), '--save-dir', str(directory / 'saves'),
                       '--native-vulkan' if renderer == 'native' else '--sdl-gpu',
                       '--frames', str(frames), '--instructions', str(frames * 300000 + 100000000),
                       '--audio-diagnostics', '--audio-capture', str(directory / 'native.wav'),
                       '--audio-output-capture', str(directory / 'output.wav'), '--capture', str(directory / 'native.ppm'),
                       '--record-inputs', str(directory / 'recording'), '--log', str(directory / 'host.log')]
            if not args.interactive:
                command += ['--input-replay', str(replay)]
            if renderer == 'native' and args.validation:
                command += ['--vulkan-validation']
            print('Starting', renderer, 'with sound and CRT; ' + ('drive with your controls, open F1 Settings, and exit with Escape when finished.' if args.interactive else 'allow the scripted driving run to finish.'), flush=True)
            row = execute(command, directory, env, args.timeout)
            result['runs'][renderer] = row
            if row['returncode']:
                raise ValueError(renderer + ' player exited unsuccessfully')
            row.update(parse_run(directory, renderer, result['environment']['vulkan']))
            if row['recording_seal']['recorded_executable_sha256'] != result['player_sha256']:
                raise ValueError('Recording names a different executable')
            if not row['crt_initially_enabled']:
                result['findings'].append(renderer + ': CRT was not enabled at startup')
            if not args.interactive and int(row['terminal'][0]) != frames:
                result['findings'].append(renderer + ': scripted frame budget did not finish')
            if any(row['audio_delivery'].values()):
                result['findings'].append(renderer + ': audio drops or device input shortfalls occurred; inspect audio_delivery and host.log')
            if not row['native_audio']['nonzero_samples'] or not row['logical_output_audio']['nonzero_samples']:
                result['findings'].append(renderer + ': native or logical output audio is silent')
            if args.interactive and not all(row['inputs'][key] for key in ('coin', 'start', 'steer_left', 'steer_right', 'gas', 'brake')):
                result['findings'].append(renderer + ': coin/start/steer/gas/brake were not all observed; repeat the manual control session if needed')
            if renderer == 'native' and args.expect_hardware and row['device_classification'] != 'driver_reported_hardware':
                result['findings'].append('native: selected Vulkan device is software, virtual or unverified')
        if not args.interactive and len(renderers) == 2:
            a, b = result['runs']['sdl'], result['runs']['native']
            comparisons = {'execution': a['terminal'] == b['terminal'], 'native_pcm': a['native_audio']['sha256'] == b['native_audio']['sha256'],
                           'final_saves': a['saves'] == b['saves'], 'final_raw_image': a['raw_image_sha256'] == b['raw_image_sha256'],
                           'recorded_observations': sha(output / 'sdl/recording/observations.txt') == sha(output / 'native/recording/observations.txt')}
            result['same_machine_comparisons'] = comparisons
            result['findings'].extend('Renderer comparison differs: ' + key for key, equal in comparisons.items() if not equal)
        if sha(player) != result['player_sha256'] or (rom and sha(rom) != result['rom_sha256']) or any(sha(game_data / name) != value for name, value in identities.items()):
            raise ValueError('Player or input changed during check')
        if not args.interactive and sha(replay) != result['replay_sha256']:
            raise ValueError('Input replay changed during check')
        if sha(Path(__file__)) != result['runner_sha256']:
            raise ValueError('Diagnostic script changed during check')
        result['status'] = 'completed_with_findings' if result['findings'] else 'passed'
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, KeyboardInterrupt) as error:
        result['error'] = str(error) or type(error).__name__
    finally:
        (output / 'operator-observations.txt').write_text(
            'Optional physical observations; these are not automatically verified.\n'
            'GPU/driver and monitor used:\nSound heard without glitches:\nCRT visible:\n'
            'Keyboard/controller model and bindings tried:\nF1 pause/settings and binding changes:\n'
            'Window resize/focus loss/controller disconnect behavior:\nEscape clean exit:\n')
        result['artifact_sha256'] = {str(p.relative_to(output)): sha(p) for p in sorted(output.rglob('*')) if p.is_file()}
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(result['status'] + ': ' + str(output / 'result.json'))
    return 0 if result['status'] == 'passed' else 2


if __name__ == '__main__':
    if sys.argv[1:] == ['--vulkan-probe']:
        print(json.dumps(vulkan_inventory()))
    else:
        raise SystemExit(main())
