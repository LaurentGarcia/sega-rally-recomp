"""ROM-free diagnostic integrity checks; no GPU or player execution is implied."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/segarally_pc_check.py'
spec = importlib.util.spec_from_file_location('pc_check', SCRIPT)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


def wav(samples=(0.0, 0.5), channels=2):
    pcm = struct.pack('<' + 'f' * len(samples), *samples)
    fmt = struct.pack('<HHIIHH', 3, channels, 44100, 44100 * channels * 4, channels * 4, 32)
    chunks = b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(pcm)) + pcm
    return b'RIFF' + struct.pack('<I', len(chunks) + 4) + b'WAVE' + chunks


def fixture(root):
    recording = root / 'recording'
    recording.mkdir()
    (root / 'saves').mkdir()
    for name, size in (('eeprom', 128), ('backup', 16384)):
        for version in ('initial', 'final'):
            (recording / (name + '.' + version + '.bin')).write_bytes(bytes(size))
        (root / 'saves' / (name + '.bin')).write_bytes(bytes(size))
    (recording / 'assets.json').write_text('{}\n')
    manifest = 'segarally-recording-v1\nexecutable 1 ' + '1' * 64 + '\n'
    for key, name in (('assets', 'assets.json'), ('eeprom_initial', 'eeprom.initial.bin'), ('backup_initial', 'backup.initial.bin')):
        path = recording / name
        manifest += f'{key} {path.stat().st_size} {check.sha(path)}\n'
    (recording / 'manifest.txt').write_text(manifest)
    (recording / 'resumes.txt').write_text('segarally-host-resume-v1\ncontext ' + check.sha(recording / 'manifest.txt') +
        ' 1 1\ninitial 0 0 0 0 00000420\n'
        'resume 0 1 0 0 0 0 00000420 fe 8f 00 ff 80 ff 00 ff ff ff ff ff 1 2 3 1 00000424 frame\n'
        'end 1 1 2 3 1 00000424\n')
    (recording / 'observations.txt').write_text('segarally-recording-observations-v1\nresume original synthetic observation\nend 1\n')
    seal = 'segarally-recording-complete-v1\n'
    for key, name in (('manifest', 'manifest.txt'), ('transcript', 'resumes.txt'), ('observations', 'observations.txt'),
                      ('eeprom_final', 'eeprom.final.bin'), ('backup_final', 'backup.final.bin')):
        path = recording / name
        seal += f'{key} {path.stat().st_size} {check.sha(path)}\n'
    seal += 'records 1\nterminal frame_limit 1 2 3 1 00000424\nend\n'
    (recording / 'complete.txt').write_text(seal)
    (root / 'host.log').write_text(
        'GPU selected=direct-vulkan device=Test GPU validation=1; game\n'
        'Display controls: F1 opens panel; F6 toggles CRT; effects=1 accurate_textures=1\n'
        'Controller: Test pad enabled=1; F1 opens Settings\n'
        'Native host exit: frames=1 instructions=1 cycles=2 pc=00000424 result=0\n'
        'Direct Vulkan presentation: submissions=1 presented_frames=1 swapchains=1 model2_pipelines=4 crt_pipelines=1 validation_errors=0\n'
        'Native audio: rate=44100 chip_dropped=0 host_dropped=0 queued=0 device_shortfall_requests=0\n')
    (root / 'stderr.txt').write_text('')
    for name in ('native.wav', 'output.wav'):
        (root / name).write_bytes(wav())
    (root / 'output.wav.events.txt').write_text('driver "dummy"\ndevice "Test output"\nend 1 1 0 0 0 0 0 0 0\n')
    (root / 'native.ppm').write_bytes(b'P6\n1 1\n255\n\x10\x20\x30')


class DiagnosticTests(unittest.TestCase):
    def test_hardware_classification_requires_matching_inventory(self):
        inventory = {'devices': [{'name': 'NVIDIA example', 'type': 'discrete'}]}
        self.assertEqual(check.classify_device('NVIDIA example', inventory), 'driver_reported_hardware')
        self.assertEqual(check.classify_device('Other NVIDIA', inventory), 'unknown')
        self.assertEqual(check.classify_device('NVIDIA example', {'devices': []}), 'unknown')

    def test_software_and_virtual_devices_are_not_hardware(self):
        self.assertEqual(check.classify_device('llvmpipe (LLVM)', {}), 'software')
        for kind, expected in (('cpu', 'software'), ('virtual', 'virtual'), ('other', 'unknown')):
            with self.subTest(kind=kind):
                self.assertEqual(check.classify_device('Test', {'devices': [{'name': 'Test', 'type': kind}]}), expected)

    def test_probe_timeout_is_bounded_and_not_hardware(self):
        process = mock.Mock()
        process.communicate.side_effect = subprocess.TimeoutExpired('probe', 15)
        process.poll.return_value = None
        with mock.patch.object(check.subprocess, 'Popen', return_value=process), mock.patch.object(check, 'terminate') as terminate:
            result = check.bounded_vulkan_inventory()
        terminate.assert_called_once_with(process)
        self.assertEqual(result['devices'], [])
        self.assertIn('did not complete', result['error'])

    def test_environment_retains_effective_game_and_driver_overrides(self):
        env = {'SEGARALLY_GPU_POLYGON_SCANLINES': '1', 'SDL_AUDIO_DRIVER': 'dummy', 'VK_DRIVER_FILES': '/icd.json',
               'MESA_VK_DEVICE_SELECT': '123', 'DISPLAY': ':99', 'UNRELATED_SECRET': 'not-recorded'}
        with mock.patch.object(check, 'bounded_vulkan_inventory', return_value={'devices': []}):
            result = check.environment(env)
        self.assertEqual(result['overrides'], {k: v for k, v in env.items() if k != 'UNRELATED_SECRET'})

    def test_wav_rejects_truncation_nonfinite_and_wrong_format(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'audio.wav'
            for data in (wav()[:-1], wav((float('nan'), 0.0)), wav()[:20] + b'\x01\x00' + wav()[22:]):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    check.wav_metrics(path)
            path.write_bytes(wav())
            self.assertEqual(check.wav_metrics(path)['frames'], 1)
            self.assertEqual(check.wav_metrics(path)['nonzero_samples'], 1)

    def test_accurate_textures_does_not_mean_crt_disabled(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture(root)
            row = check.parse_run(root, 'native', {'devices': []})
            self.assertTrue(row['crt_initially_enabled'])
            self.assertTrue(row['accurate_textures'])
            self.assertTrue(row['clean_exit_and_save_finalization'])
            self.assertEqual(row['recording_seal']['records'], 1)

    def test_completed_marker_does_not_hide_artifact_tampering(self):
        for name in ('manifest.txt', 'resumes.txt', 'observations.txt', 'eeprom.final.bin', 'backup.final.bin'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fixture(root)
                path = root / 'recording' / name
                data = path.read_bytes()
                path.write_bytes(bytes([data[0] ^ 1]) + data[1:])
                with self.assertRaises(ValueError):
                    check.recording_seal(root / 'recording', ['1', '1', '2', '00000424', '0'], 1)

    def test_seal_terminal_count_and_initial_seed_are_checked(self):
        for target in ('terminal', 'count', 'seed'):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fixture(root)
                if target == 'seed':
                    (root / 'recording/eeprom.initial.bin').write_bytes(b'x' * 128)
                else:
                    path = root / 'recording/complete.txt'
                    path.write_text(path.read_text().replace('records 1', 'records 2') if target == 'count' else path.read_text().replace('frame_limit 1 2', 'frame_limit 2 2'))
                with self.assertRaises(ValueError):
                    check.recording_seal(root / 'recording', ['1', '1', '2', '00000424', '0'], 1)

    def test_output_overflow_and_validation_errors_fail(self):
        for target in ('output', 'validation'):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fixture(root)
                if target == 'output':
                    path = root / 'output.wav.events.txt'
                    path.write_text(path.read_text().replace('end 1 1 0 0 0', 'end 1 1 1 2 1'))
                else:
                    (root / 'stderr.txt').write_text('Validation Error: VUID-test')
                with self.assertRaises(ValueError):
                    check.parse_run(root, 'native', {})


if __name__ == '__main__':
    unittest.main()
