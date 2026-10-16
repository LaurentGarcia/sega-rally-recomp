#!/usr/bin/env python3
"""Read-only compiler identity extraction across CMake cache conventions."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('frontend',ROOT/'scripts/segarally_frontend_ci.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class ToolchainTests(unittest.TestCase):
 def fixture(self,path,cache_compiler=False):
  path.mkdir(parents=True)
  text='CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\nCMAKE_GENERATOR_TOOLSET:INTERNAL=ClangCL\nCMAKE_GENERATOR_PLATFORM:INTERNAL=x64\n'
  if cache_compiler:text+='CMAKE_C_COMPILER:FILEPATH=C:/Program Files/LLVM/bin/clang-cl.exe\n'
  (path/'CMakeCache.txt').write_text(text)
  metadata=path/'CMakeFiles/3.30.0';metadata.mkdir(parents=True)
  for language in ('C','CXX'):
   (metadata/f'CMake{language}Compiler.cmake').write_text(f'set(CMAKE_{language}_COMPILER "C:/Program Files/LLVM/bin/clang-cl.exe")\nset(CMAKE_{language}_COMPILER_ID "Clang")\nset(CMAKE_{language}_COMPILER_VERSION "19.1.0")\n')
 def test_visual_studio_missing_cache_paths(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td)/'build';self.fixture(root);actual=m.toolchain(root)
   self.assertEqual(actual['CMAKE_CXX_COMPILER'],'C:/Program Files/LLVM/bin/clang-cl.exe')
   self.assertEqual(actual['CMAKE_CXX_COMPILER_ID'],'Clang')
   self.assertEqual(actual['CMAKE_CXX_COMPILER_VERSION'],'19.1.0')
 def test_cache_presence_does_not_change_identity(self):
  with tempfile.TemporaryDirectory() as td:
   a,b=Path(td)/'a',Path(td)/'b';self.fixture(a);self.fixture(b,True)
   m.verify_toolchain(m.toolchain(a),m.toolchain(b))
 def test_conflicting_cache_rejects(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td)/'build';self.fixture(root)
   with (root/'CMakeCache.txt').open('a') as f:f.write('CMAKE_C_COMPILER:FILEPATH=other-compiler\n')
   with self.assertRaisesRegex(ValueError,'disagree'):m.toolchain(root)
 def test_changed_compiler_version_and_toolset_reject(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td)/'build';self.fixture(root);a=m.toolchain(root)
   for key in ('CMAKE_CXX_COMPILER','CMAKE_CXX_COMPILER_VERSION','CMAKE_GENERATOR_TOOLSET'):
    b=dict(a);b[key]='other'
    with self.assertRaisesRegex(RuntimeError,key):m.verify_toolchain(a,b)
 def test_missing_or_ambiguous_metadata_rejects(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td)/'build';self.fixture(root)
   (root/'CMakeFiles/3.30.0/CMakeCCompiler.cmake').unlink()
   with self.assertRaises(ValueError):m.toolchain(root)
   (root/'CMakeFiles/3.30.0/CMakeCCompiler.cmake').write_text('set(CMAKE_C_COMPILER "incomplete")\n')
   with self.assertRaisesRegex(ValueError,'Missing actual'):m.toolchain(root)
 def test_macos_renderer_coverage_is_required(self):
  self.assertEqual(m.renderer_features({'SEGARALLY_NATIVE_METAL':'ON','SEGARALLY_NATIVE_VULKAN':'OFF'},'darwin'),
                   {'native_metal':True,'native_vulkan':False,'sdl_gpu':True})
  with self.assertRaisesRegex(RuntimeError,'must be ON'):
   m.renderer_features({'SEGARALLY_NATIVE_METAL':'OFF'},'darwin')
 def test_nonapple_ci_retains_sdl_without_metal(self):
  for platform in ('win32','linux'):
   self.assertEqual(m.renderer_features({'SEGARALLY_NATIVE_METAL':'OFF','SEGARALLY_NATIVE_VULKAN':'OFF'},platform),
                    {'native_metal':False,'native_vulkan':False,'sdl_gpu':True})
   with self.assertRaisesRegex(RuntimeError,'OFF elsewhere'):
    m.renderer_features({'SEGARALLY_NATIVE_METAL':'ON'},platform)
 def test_missing_or_invalid_renderer_cache_rejects(self):
  for actual in ({},{'SEGARALLY_NATIVE_METAL':''},{'SEGARALLY_NATIVE_METAL':'other'}):
   with self.assertRaisesRegex(RuntimeError,'Missing or invalid'):
    m.renderer_features(actual,'darwin')
 def test_direct_vulkan_coverage_is_explicit(self):
  for platform in ('win32','linux'):
   self.assertEqual(m.renderer_features({'SEGARALLY_NATIVE_METAL':'OFF','SEGARALLY_NATIVE_VULKAN':'ON'},platform,True),
                    {'native_metal':False,'native_vulkan':True,'sdl_gpu':True})
   for enabled,requested in (('ON',False),('OFF',True)):
    with self.assertRaisesRegex(RuntimeError,'differs from the requested'):
     m.renderer_features({'SEGARALLY_NATIVE_METAL':'OFF','SEGARALLY_NATIVE_VULKAN':enabled},platform,requested)
 def test_missing_or_invalid_vulkan_cache_rejects(self):
  for value in (None,'','other'):
   actual={'SEGARALLY_NATIVE_METAL':'OFF'}
   if value is not None:actual['SEGARALLY_NATIVE_VULKAN']=value
   with self.assertRaisesRegex(RuntimeError,'Missing or invalid SEGARALLY_NATIVE_VULKAN'):
    m.renderer_features(actual,'linux',True)
 def test_vulkan_cannot_replace_metal_on_macos(self):
  with self.assertRaisesRegex(RuntimeError,'targets Windows/Linux'):
   m.renderer_features({'SEGARALLY_NATIVE_METAL':'ON','SEGARALLY_NATIVE_VULKAN':'ON'},'darwin',True)
if __name__=='__main__':unittest.main()
