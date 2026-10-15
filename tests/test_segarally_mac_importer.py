#!/usr/bin/env python3
"""ROM-free local-import safety checks; no network, signing, game or GUI."""
import importlib.util
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('importer', ROOT / 'platform/segarally/macos/importer/coordinator.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class FakeRunner(m.Runner):
    def __init__(self):
        super().__init__(notify=lambda *args, **kwargs: None)
        self.calls = []
    def run(self, args, **kwargs):
        self.calls.append((list(map(str, args)), kwargs))
        if 'rev-parse' in args: return m.SOURCE_REVISION + '\n'
        return ''

class ImportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='importer checks ')
        self.root = Path(self.tmp.name).resolve()
        self.runner = FakeRunner()
        self.coordinator = m.Coordinator(self.root / 'imports', self.runner)
    def tearDown(self): self.tmp.cleanup()
    def rom(self):
        p = self.root / 'my own ; $(touch nope).zip'
        with zipfile.ZipFile(p, 'w') as z: z.writestr('synthetic.bin', b'public invented fixture')
        return p
    def app(self, session):
        app = session / 'source/build/player/mac/Sega Rally Native.app'
        assets = app / 'Contents/Resources/assets'
        assets.mkdir(parents=True)
        exe = app / 'Contents/MacOS/segarally_native'
        exe.parent.mkdir(); exe.write_bytes(b'invented executable')
        images = {}
        for i in range(8):
            p = assets / f'{i}.bin'; p.write_bytes(bytes([i]))
            images[str(i)] = {'file': p.name, 'size': 1, 'sha256': m.digest(p)}
        m.write_new(assets / 'assets.json', {'images': images})
        report = {'status': 'packaged', 'source_revision': m.SOURCE_REVISION,
                  'source_diff_sha256': m.hashlib.sha256(b'').hexdigest(),
                  'dependencies': {k:v[1] for k,v in m.DEPENDENCIES.items()},
                  'executable_sha256': m.digest(exe), 'assets_sha256': m.digest(assets / 'assets.json')}
        m.write_new(session / 'source/build/player/build-result.json', report)
        return app
    def test_pins_match_production_pipeline(self):
        pipeline = ROOT / 'scripts/segarally_mac.py'
        import ast
        tree = ast.parse(pipeline.read_text())
        pins = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                    and any(isinstance(t, ast.Name) and t.id == 'DEPENDENCIES' for t in n.targets))
        self.assertEqual(pins, {k:v[1] for k,v in m.DEPENDENCIES.items()})
        self.assertEqual(len(m.SOURCE_REVISION), 40)
    def test_prerequisites_fail_before_workspace_or_rom_access(self):
        with patch.object(m, 'prerequisites', side_effect=m.ImportFailure('Install CMake')):
            with self.assertRaisesRegex(m.ImportFailure, 'Install CMake'):
                self.coordinator.import_rom(self.root / 'missing.zip')
        self.assertFalse(self.coordinator.root.exists())
    def test_invalid_archive_before_workspace(self):
        p = self.root / 'no.zip'; p.write_text('not a ZIP')
        with patch.object(m, 'prerequisites', return_value={}):
            with self.assertRaises(m.ImportFailure): self.coordinator.import_rom(p)
        self.assertFalse(self.coordinator.root.exists())
    def test_dependency_parent_created_and_argv_not_shell(self):
        dest = self.root / 'a ; $(bad)' / 'extern/sdl3'
        self.coordinator.clone(m.SOURCE_URL, m.SOURCE_REVISION, dest)
        self.assertTrue(dest.parent.is_dir())
        self.assertEqual(self.runner.calls[0][0], ['git','init','-q',str(dest)])
        self.assertIn(m.SOURCE_REVISION, self.runner.calls[2][0])
        self.assertFalse((self.root / 'bad').exists())
    def test_existing_download_not_overwritten(self):
        dest = self.root / 'old'; dest.mkdir(); (dest/'saved').write_bytes(b'preserve')
        with self.assertRaises(m.ImportFailure): self.coordinator.clone(m.SOURCE_URL, m.SOURCE_REVISION, dest)
        self.assertEqual((dest/'saved').read_bytes(), b'preserve')
        self.assertEqual(self.runner.calls, [])
    def test_wrong_download_revision_rejected(self):
        self.runner.run = lambda args, **kwargs: 'wrong' if 'rev-parse' in args else ''
        with self.assertRaisesRegex(m.ImportFailure, 'required version'):
            self.coordinator.clone(m.SOURCE_URL, m.SOURCE_REVISION, self.root/'new')
    def test_failed_retry_isolated_and_original_rom_preserved(self):
        rom = self.rom(); before = m.digest(rom)
        with patch.object(m, 'prerequisites', return_value={}), patch.object(self.coordinator, 'clone', side_effect=m.ImportFailure('network unavailable')):
            for _ in range(2):
                with self.assertRaisesRegex(m.ImportFailure, 'network unavailable'): self.coordinator.import_rom(rom)
        sessions = list(self.coordinator.root.iterdir())
        self.assertEqual(len(sessions), 2)
        for s in sessions:
            self.assertEqual(m.digest(s/'personal-input.zip'), before)
            self.assertEqual(json.loads((s/'failed.json').read_text())['status'], 'failed')
            self.assertFalse((s/'completed.json').exists())
        self.assertEqual(m.digest(rom), before)
    def test_cancelled_import_never_publishes_complete(self):
        def cancel(*args): self.runner.stop.set(); raise m.Cancelled()
        with patch.object(m, 'prerequisites', return_value={}), patch.object(self.coordinator, 'clone', side_effect=cancel):
            with self.assertRaises(m.Cancelled): self.coordinator.import_rom(self.rom())
        s = next(self.coordinator.root.iterdir())
        self.assertEqual(json.loads((s/'failed.json').read_text())['status'], 'cancelled')
        self.assertFalse((s/'completed.json').exists())
    def test_success_receipt_only_after_verified_app(self):
        original = self.runner.run
        def fake_build(args, **kwargs):
            if args[0] == sys.executable:
                self.app(Path(kwargs['cwd']).parent)
                return ''
            return original(args, **kwargs)
        def fake_clone(url, revision, path): path.mkdir(parents=True)
        self.runner.run = fake_build
        with patch.object(m, 'prerequisites', return_value={}), patch.object(self.coordinator, 'clone', side_effect=fake_clone):
            session = self.coordinator.import_rom(self.rom())
        receipt = json.loads((session/'completed.json').read_text())
        self.assertEqual(receipt['source_revision'],m.SOURCE_REVISION)
        self.assertEqual(receipt['executable_sha256'],m.digest(Path(receipt['app'])/'Contents/MacOS/segarally_native'))
        self.assertFalse((session/'failed.json').exists())
    def test_verify_app_binds_eight_payloads(self):
        session = self.root/'session'; app = self.app(session)
        self.assertEqual(self.coordinator.verify_app(session), app)
        self.assertEqual(self.runner.calls[-1][0][:4], ['/usr/bin/codesign','--verify','--deep','--strict'])
        (app/'Contents/Resources/assets/3.bin').write_bytes(b'changed')
        with self.assertRaises(m.ImportFailure): self.coordinator.verify_app(session)
    def test_verify_rejects_payload_path_escape(self):
        s = self.root/'session'; app = self.app(s)
        ap = app/'Contents/Resources/assets/assets.json'
        j = json.loads(ap.read_text()); j['images']['0']['file']='../../../../outside'; ap.write_text(json.dumps(j))
        rp = s/'source/build/player/build-result.json'; r=json.loads(rp.read_text()); r['assets_sha256']=m.digest(ap); rp.write_text(json.dumps(r))
        with self.assertRaises(m.ImportFailure): self.coordinator.verify_app(s)
    def test_launch_rejects_other_import_root(self):
        with self.assertRaises(m.ImportFailure): self.coordinator.launch(self.root)
        self.assertEqual(self.runner.calls, [])
    def test_launch_requires_receipt_and_isolated_saves(self):
        s = self.coordinator.root/'session'; app=self.app(s)
        with self.assertRaises(FileNotFoundError): self.coordinator.launch(s)
        m.write_new(s/'completed.json', {'source_revision':m.SOURCE_REVISION, 'app':str(app),
                    'executable_sha256':m.digest(app/'Contents/MacOS/segarally_native')})
        self.coordinator.launch(s)
        call = self.runner.calls[-1][0]
        self.assertEqual(call[:3], ['/usr/bin/open','-n',str(app)])
        self.assertEqual(call[3:], ['--args','--save-dir',str(s/'player-saves'),'--sound','--instruction-boundary-hook'])
    def test_active_game_prevents_second_launch(self):
        s = self.coordinator.root/'session'; app=self.app(s)
        m.write_new(s/'completed.json', {'source_revision':m.SOURCE_REVISION,'app':str(app),
                    'executable_sha256':m.digest(app/'Contents/MacOS/segarally_native')})
        original=self.runner.run
        self.runner.run=lambda args, **kwargs: '12345\n' if args[0]=='/usr/bin/pgrep' else original(args, **kwargs)
        with self.assertRaisesRegex(m.ImportFailure,'Close the running'): self.coordinator.launch(s)
        self.assertFalse(any(call[0][0]=='/usr/bin/open' for call in self.runner.calls))
        self.assertFalse((s/'player-saves').exists())
    def test_save_symlink_never_launched(self):
        s = self.coordinator.root/'session'; app=self.app(s)
        m.write_new(s/'completed.json', {'source_revision':m.SOURCE_REVISION,'app':str(app),
                    'executable_sha256':m.digest(app/'Contents/MacOS/segarally_native')})
        (s/'player-saves').symlink_to(self.root)
        with self.assertRaisesRegex(m.ImportFailure,'save folder'): self.coordinator.launch(s)
        self.assertFalse(any(call[0][0]=='/usr/bin/open' for call in self.runner.calls))
    def test_environment_excludes_overrides(self):
        with patch.dict(os.environ, {'SDL_GPU_DRIVER':'bad','SEGARALLY_SAVE_DIR':'old','GIT_DIR':'old','PYTHONPATH':'bad'}):
            env=m.environment()
        for name in ('SDL_GPU_DRIVER','SEGARALLY_SAVE_DIR','GIT_DIR','PYTHONPATH'): self.assertNotIn(name,env)
        self.assertEqual(env['GIT_TERMINAL_PROMPT'],'0')

class ProcessTests(unittest.TestCase):
    def test_literal_arguments_and_spaces(self):
        r=m.Runner(notify=lambda *args, **kwargs:None)
        value='name with spaces ; $(echo unsafe)'
        self.assertEqual(r.run([sys.executable,'-c','import sys; print(sys.argv[1])',value],quiet=True).strip(),value)
    def test_cancel_reaps_child_group(self):
        r=m.Runner(notify=lambda *args, **kwargs:None)
        timer=threading.Timer(.25,r.stop.set); timer.start()
        start=time.monotonic()
        try:
            with self.assertRaises(m.Cancelled): r.run([sys.executable,'-c','import time; time.sleep(30)'],quiet=True)
        finally: timer.cancel()
        self.assertLess(time.monotonic()-start,7)
    def test_cooperative_pipeline_cleans_separate_child_group(self):
        with tempfile.TemporaryDirectory() as td:
            marker=Path(td)/'cleaned'
            # Mirrors the unchanged production pipeline's BaseException cleanup
            # of a build step started in its own session.
            script='''import os,signal,subprocess,sys,time
p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'],start_new_session=True)
try:
 p.wait()
except BaseException:
 os.killpg(p.pid,signal.SIGTERM); p.wait()
 open(sys.argv[1],'w').write('reaped')
 raise
'''
            r=m.Runner(notify=lambda *args, **kwargs:None)
            timer=threading.Timer(.5,r.stop.set); timer.start()
            try:
                with self.assertRaises(m.Cancelled): r.run([sys.executable,'-c',script,marker],quiet=True,cooperative=True)
            finally: timer.cancel()
            self.assertEqual(marker.read_text(),'reaped')
    def test_timeout_preserves_failure(self):
        r=m.Runner(notify=lambda *args, **kwargs:None)
        with self.assertRaisesRegex(m.ImportFailure,'too long'):
            r.run([sys.executable,'-c','import time; time.sleep(30)'],timeout=.1,quiet=True)

if __name__=='__main__': unittest.main()
