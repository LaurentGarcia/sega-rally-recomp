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
        self.font_notices = []
        for font, author in (('ProggyClean.ttf', '2001 Fixture Bitmap'),
                             ('ProggyForever-Regular-minimal.ttf', '2002 Fixture Vector')):
            self.font_notices.append(('// --------------------\n// [SECTION] Default font data (' + font + ')\n'
                '// --------------------\n// MIT ' + ('License' if 'Clean' in font else 'license') + ' / Copyright (c) ' + author + '\n'
                '// --------------------\n').encode())
        (self.root/'imgui/imgui_draw.cpp').write_bytes(
            b'// synthetic upstream font container\n// [SECTION] Default font data (ProggyClean.ttf)\n\n' +
            b'\n#if FONT_ENABLED\nstatic unsigned char forbidden_font_payload[] = {1,2};\n#endif\n'.join(self.font_notices) +
            b'\n#if FONT_ENABLED\nvoid forbidden_font_implementation() {}\n#endif\n')
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
        fonts = got['ImGui-embedded-fonts.txt']
        for notice in self.font_notices:
            self.assertIn(notice, fonts)
        self.assertTrue(fonts.endswith((self.root/'imgui/LICENSE.txt').read_bytes()))
        self.assertNotIn(b'forbidden_font', fonts)
        self.assertNotIn(b'#if', fonts)
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

    def test_missing_or_duplicate_embedded_font_notice_fails(self):
        draw = self.root/'imgui/imgui_draw.cpp'
        original = draw.read_bytes()
        for source in (original.replace(self.font_notices[0], b''),
                       original + self.font_notices[1]):
            with self.subTest(source_length=len(source)):
                draw.write_bytes(source)
                with self.assertRaisesRegex(ValueError, 'font notice'):
                    self.collect()
        draw.unlink()
        with self.assertRaises(OSError):
            self.collect()

    def test_changed_font_license_fails_without_copying_payload(self):
        draw = self.root/'imgui/imgui_draw.cpp'
        draw.write_bytes(draw.read_bytes().replace(b'MIT License', b'Other terms', 1))
        with self.assertRaisesRegex(ValueError, 'MIT copyright'):
            self.collect()

    def test_missing_font_fails_before_output_mutation(self):
        output = self.root/'output'
        output.mkdir()
        (output/'existing.txt').write_bytes(b'preserve prior notice package')
        (self.root/'imgui/imgui_draw.cpp').unlink()
        args = ['package_licenses.py', '--output', str(output), '--sdl-source', str(self.root/'sdl'),
                '--imgui-source', str(self.root/'imgui')]
        with patch.object(module, 'ROOT', self.root), patch('sys.argv', args):
            with self.assertRaises(SystemExit) as error:
                module.main()
        self.assertEqual(error.exception.code, 2)
        self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()},
                         {'existing.txt': b'preserve prior notice package'})

    def test_original_mame_header_is_collected_without_implementation(self):
        header = b'// license:BSD-3-Clause\n// copyright-holders:Olivier Galibert\n'
        path = self.root/'src/runtime/sound_fixture.cpp'
        path.write_bytes(header + b'#include "not_a_notice.h"\nvoid forbidden_sound() {}\n')
        notices = self.collect()['Source-notices.txt']
        self.assertIn(b'src/runtime/sound_fixture.cpp\n' + header, notices)
        self.assertNotIn(b'forbidden_sound', notices)
        self.assertNotIn(b'not_a_notice.h', notices)

    def test_changed_public_domain_declaration_fails(self):
        (self.root/'lzma/C/Sha256.c').write_text('/* different terms */\n')
        with self.assertRaisesRegex(ValueError, 'public-domain'):
            self.collect()


if __name__ == '__main__':
    unittest.main()
