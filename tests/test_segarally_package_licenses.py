#!/usr/bin/env python3
"""Synthetic notice packaging; no vendored code, ROMs or OS commands required."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_licenses', ROOT / 'platform/segarally/package_licenses.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackageLicenses(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name, data in {'LICENSE': b'exact BSD terms\r\n', 'THIRD_PARTY.md': b'exact dependency record\n',
                'sdl/LICENSE.txt': b'exact SDL notice', 'imgui/LICENSE.txt': b'exact MIT notice',
                'lzma/DOC/License.txt': b'exact upstream license',
                'src/runtime/synthetic.cpp': b'// license:BSD-3-Clause\n// copyright-holders:Example\n\n#include "synthetic.h"\nvoid code() {}\n'}.items():
            p = self.root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        for name in ('Sha256.c', 'Sha256Opt.c', 'CpuArch.c'):
            p = self.root / 'lzma/C' / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('/* Example author: Public domain */\n#include "a.h"\nvoid code() {}')

    def collect(self, recording=True):
        return module.collect(self.root, self.root/'sdl', self.root/'imgui',
                              self.root/'lzma' if recording else None)

    def test_exact_notice_bytes_and_source_attribution(self):
        got = self.collect()
        for name, source in {'Project-BSD-3-Clause.txt':'LICENSE', 'THIRD_PARTY.md':'THIRD_PARTY.md',
                'SDL.txt':'sdl/LICENSE.txt', 'ImGui.txt':'imgui/LICENSE.txt',
                '7-Zip-License.txt':'lzma/DOC/License.txt'}.items():
            self.assertEqual(got[name], (self.root/source).read_bytes())
        self.assertIn(b'copyright-holders:Example', got['Source-notices.txt'])
        self.assertNotIn(b'#include', got['Source-notices.txt'])
        self.assertNotIn(b'void code()', got['7-Zip-selected-sources.txt'])
        self.assertEqual(got['7-Zip-selected-sources.txt'].count(b'Public domain'), 3)

    def test_no_recording_requires_no_lzma_dependency(self):
        (self.root/'lzma/DOC/License.txt').unlink()
        got = self.collect(False)
        self.assertNotIn('7-Zip-License.txt', got)
        self.assertNotIn('7-Zip-selected-sources.txt', got)

    def test_reconfigure_without_recording_removes_old_optional_notices(self):
        output = self.root/'output'
        args = ['package_licenses.py', '--output', str(output), '--sdl-source', str(self.root/'sdl'),
                '--imgui-source', str(self.root/'imgui')]
        with patch.object(module, 'ROOT', self.root), patch('sys.argv', args + ['--lzma-source', str(self.root/'lzma')]):
            module.main()
        self.assertTrue((output/'7-Zip-License.txt').exists())
        with patch.object(module, 'ROOT', self.root), patch('sys.argv', args):
            module.main()
        self.assertFalse((output/'7-Zip-License.txt').exists())
        self.assertFalse((output/'7-Zip-selected-sources.txt').exists())
        self.assertEqual((output/'SDL.txt').read_bytes(), (self.root/'sdl/LICENSE.txt').read_bytes())

    def test_missing_notice_fails(self):
        (self.root/'sdl/LICENSE.txt').unlink()
        with self.assertRaises(OSError):
            self.collect()

    def test_missing_source_attribution_fails(self):
        (self.root/'src/runtime/synthetic.cpp').write_text('// no license notice\nint value;')
        with self.assertRaisesRegex(ValueError, 'source notices'):
            self.collect()

    def test_changed_public_domain_declaration_fails(self):
        (self.root/'lzma/C/Sha256.c').write_text('/* different terms */\n')
        with self.assertRaisesRegex(ValueError, 'public-domain'):
            self.collect()


if __name__ == '__main__':
    unittest.main()
