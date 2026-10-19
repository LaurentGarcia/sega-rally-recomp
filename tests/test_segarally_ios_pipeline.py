#!/usr/bin/env python3
"""ROM-free guards for personal iOS build, signing and export planning."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import segarally_ios as ios


class IosPipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / 'build').mkdir(exist_ok=True)

    def call(self, *options):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            output = Path(directory) / 'new-build'
            stream = io.StringIO()
            with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(io.StringIO()):
                result = ios.main(['--native-build', str(ROOT / 'build/audited'),
                                   '--output', str(output), '--plan', *options])
            self.assertFalse(output.exists(), 'planning must never create build output')
            return result, json.loads(stream.getvalue()) if result == 0 else None

    def test_unsigned_default_has_no_signing_or_install(self):
        code, plan = self.call()
        self.assertEqual(code, 0)
        arguments = [word for step in plan for word in step['command']]
        self.assertIn('CODE_SIGNING_ALLOWED=NO', arguments)
        self.assertIn('-DSEGARALLY_SOUND=ON', arguments)
        self.assertIn('-DSEGARALLY_NATIVE_METAL=ON', arguments)
        self.assertIn('-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON', arguments)
        self.assertIn('-DSEGARALLY_INPUT_RECORDING=OFF', arguments)
        self.assertFalse(any(word in arguments for word in ['install', 'launch', 'upload', '-allowProvisioningUpdates']))

    def test_sdl_comparison_preserves_game_and_sound_options(self):
        code, plan = self.call('--graphics', 'sdl')
        self.assertEqual(code, 0)
        configure = plan[0]['command']
        self.assertIn('-DSEGARALLY_NATIVE_METAL=OFF', configure)
        self.assertIn('-DSEGARALLY_SOUND=ON', configure)
        self.assertIn('-DSEGARALLY_INSTRUCTION_BOUNDARY_HOOK=ON', configure)
        self.assertIn('CODE_SIGNING_ALLOWED=NO', plan[1]['command'])

    def test_development_requires_team(self):
        self.assertEqual(self.call('--signing', 'development')[0], 1)

    def test_unsigned_rejects_provisioning_changes(self):
        self.assertEqual(self.call('--allow-provisioning-updates')[0], 1)

    def test_signed_archive_has_local_signature_gate(self):
        code, plan = self.call('--signing', 'development', '--team', 'AB12345678', '--archive')
        self.assertEqual(code, 0)
        self.assertEqual([row['name'] for row in plan], ['configure', 'archive', 'signature'])
        self.assertNotIn('-allowProvisioningUpdates', plan[1]['command'])
        self.assertIn('Products/Applications/segarally_native.app', plan[-1]['command'][-1])

    def test_export_requires_signed_archive(self):
        self.assertEqual(self.call('--export-method', 'debugging')[0], 1)
        self.assertEqual(self.call('--archive', '--export-method', 'debugging')[0], 1)

    def test_export_plan_never_uploads(self):
        code, plan = self.call('--archive', '--signing', 'development', '--team', 'AB12345678',
                               '--export-method', 'app-store-connect')
        self.assertEqual(code, 0)
        self.assertEqual(plan[-1]['name'], 'export')
        self.assertIn('-exportArchive', plan[-1]['command'])
        self.assertNotIn('upload', plan[-1]['command'])

    def test_unsigned_ipa_is_local_and_cannot_be_signed_mode(self):
        code, plan = self.call('--unsigned-ipa')
        self.assertEqual(code, 0)
        self.assertEqual([row['name'] for row in plan[-2:]], ['stage-unsigned-ipa', 'unsigned-ipa'])
        self.assertEqual(plan[-1]['command'][0], 'ditto')
        self.assertEqual(self.call('--unsigned-ipa', '--signing', 'development', '--team', 'AB12345678')[0], 1)

    def test_invalid_identifiers_fail_before_execution(self):
        self.assertEqual(self.call('--bundle-id', '../app')[0], 1)
        self.assertEqual(self.call('--build-number', '1\n2')[0], 1)
        self.assertEqual(self.call('--signing', 'development', '--team', 'bad')[0], 1)

    def test_output_cannot_replace_repository_or_prior_build(self):
        with self.assertRaises(ValueError):
            ios.output_path(ROOT)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            with self.assertRaises(ValueError):
                ios.output_path(Path(directory))

    def test_explicit_device_is_build_destination_only(self):
        code, plan = self.call('--device', 'test-device')
        self.assertEqual(code, 0)
        self.assertIn('platform=iOS,id=test-device', plan[1]['command'])
        self.assertEqual(plan[1]['command'][-1], 'build')

    def test_generated_pipeline_uses_same_native_core(self):
        class Args:
            native_build = None
            roms = Path('/personal/srallyc.zip')
            jobs = 4
            bundle_id = 'org.local.rally'
            version = '0.1.0'
            build_number = '1'
            signing = 'unsigned'
            graphics = 'metal'
            team = None
            device = None
            archive = False
            export_method = None
            unsigned_ipa = False
            allow_provisioning_updates = False
        plan = ios.build_plan(Args(), ROOT / 'build/new')
        self.assertEqual([row['name'] for row in plan[:5]], ['main', 'tgp', 'video', 'sound-import', 'sound'])
        self.assertIn('--indexed-dispatch', plan[0]['command'])
        self.assertIn('--instruction-boundary-hook', plan[0]['command'])
        self.assertIn('--timed', plan[4]['command'])


if __name__ == '__main__':
    (ROOT / 'build').mkdir(exist_ok=True)
    unittest.main()
