#!/usr/bin/env python3
"""ROM-free build-plan, preservation and local-package regression checks."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('desktop',ROOT/'scripts/segarally_desktop.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DesktopTests(unittest.TestCase):
 def test_explicit_compiler_path_and_audited_modes(self):
  plan=m.build_plan(Path('/personal files/game.zip'),ROOT/'build/new',2,'C:/Program Files/LLVM/bin/clang++.exe')
  self.assertEqual(len(plan),8)
  cmd=plan[0][1]
  self.assertIn('--instruction-boundary-hook',cmd);self.assertIn('--indexed-dispatch',cmd)
  self.assertEqual(m.shlex.split(cmd[cmd.index('--compiler')+1]),['C:/Program Files/LLVM/bin/clang++.exe'])
  self.assertFalse(any('open' in cmd or 'start' in cmd for _,cmd,_ in plan))
 def test_output_preserves_existing(self):
  with tempfile.TemporaryDirectory() as td,patch.object(m,'ROOT',Path(td).resolve()):
   old=m.ROOT/'build/old';old.mkdir(parents=True);(old/'save').write_bytes(b'keep')
   with self.assertRaises(ValueError):m.output_path(old)
   self.assertEqual((old/'save').read_bytes(),b'keep')
 def test_output_cannot_escape_ignored_build(self):
  for path in (ROOT,ROOT/'build',ROOT/'outside'):
   with self.assertRaises(ValueError):m.output_path(path)
 def test_plan_never_downloads_or_builds(self):
  with tempfile.TemporaryDirectory() as td,patch.object(m,'ROOT',Path(td).resolve()),patch.object(m.platform,'system',return_value='Linux'),patch.object(m.subprocess,'Popen',side_effect=AssertionError('execution')):
   rom=m.ROOT/'personal.zip';rom.write_bytes(b'synthetic plan only')
   self.assertEqual(m.main(['--roms',str(rom),'--output',str(m.ROOT/'build/new'),'--plan']),0)
   self.assertFalse((m.ROOT/'build').exists())
 def test_missing_dependencies_require_explicit_download(self):
  with tempfile.TemporaryDirectory() as td,patch.object(m,'ROOT',Path(td)),patch.object(m.subprocess,'run',side_effect=AssertionError('network')):
   with self.assertRaisesRegex(ValueError,'fetch-dependencies'):m.dependencies(False)
 def test_package_is_local_and_contains_notices(self):
  with tempfile.TemporaryDirectory() as td,patch.object(m.platform,'system',return_value='Linux'):
   out=Path(td);host=out/'host';host.mkdir();exe=host/'segarally_native';exe.write_bytes(b'invented fixture')
   (host/'assets').mkdir();(host/'assets/fixture').write_bytes(b'invented payload')
   (host/'licenses').mkdir();(host/'licenses/LICENSE').write_bytes(b'synthetic license fixture')
   result=m.package(out)
   self.assertEqual(result['files']['assets/fixture']['sha256'],m.digest(host/'assets/fixture'))
   with zipfile.ZipFile(result['archive']) as z:
    self.assertIn('Sega Rally Native/licenses/LICENSE',z.namelist())
    self.assertIn('Sega Rally Native/launch.sh',z.namelist())
   self.assertTrue((host/'segarally_native').is_file())
if __name__=='__main__':unittest.main()
