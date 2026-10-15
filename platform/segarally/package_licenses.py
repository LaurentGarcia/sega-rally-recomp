#!/usr/bin/env python3
"""Retain existing source and dependency notices beside the local Mac binary."""
from __future__ import annotations
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def leading_comments(text: str) -> str:
    # Copy notices verbatim, never generated game code or implementation bodies.
    match = re.match(r'\A(?:\s+|//[^\n]*(?:\n|$)|/\*[\s\S]*?\*/)*', text)
    return match.group(0).strip() if match else ''


def collect(root: Path, sdl: Path, imgui: Path, lzma: Path | None = None) -> dict[str, bytes]:
    files = {'Project-BSD-3-Clause.txt': root / 'LICENSE',
             'THIRD_PARTY.md': root / 'THIRD_PARTY.md',
             'SDL.txt': sdl / 'LICENSE.txt', 'ImGui.txt': imgui / 'LICENSE.txt'}
    if lzma is not None:
        files['7-Zip-License.txt'] = lzma / 'DOC/License.txt'
    notices = {name: path.read_bytes() for name, path in files.items()}
    retained = []
    for path in sorted((root / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.hlsl'):
            continue
        preamble = leading_comments(path.read_text())
        if re.search(r'license:\s*BSD-3-Clause|copyright-holders:', preamble):
            retained.append(f'{path.relative_to(root).as_posix()}\n{preamble}\n')
    if not retained:
        raise ValueError('retained runtime source notices are missing')
    # The accompanying Project-BSD-3-Clause.txt retains the full BSD conditions
    # and disclaimer. These are the original per-file author/source notices,
    # including optional components; no new license wording is substituted.
    notices['Source-notices.txt'] = ('\n'.join(retained)).encode()
    if lzma is not None:
        entries = []
        for name in ('Sha256.c', 'Sha256Opt.c', 'CpuArch.c'):
            preamble = leading_comments((lzma / 'C' / name).read_text())
            if 'Public domain' not in preamble:
                raise ValueError(f'{name} no longer declares public-domain status')
            entries.append(f'C/{name}\n{preamble}\n')
        notices['7-Zip-selected-sources.txt'] = ('\n'.join(entries)).encode()
    return notices


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sdl-source', type=Path, required=True)
    parser.add_argument('--imgui-source', type=Path, required=True)
    parser.add_argument('--lzma-source', type=Path)
    args = parser.parse_args()
    try:
        notices = collect(ROOT, args.sdl_source, args.imgui_source, args.lzma_source)
        args.output.mkdir(parents=True, exist_ok=True)
        for name, content in notices.items():
            (args.output / name).write_bytes(content)
        # Recording can be disabled during reconfiguration of an existing host.
        for name in ('7-Zip-License.txt', '7-Zip-selected-sources.txt'):
            if name not in notices:
                (args.output / name).unlink(missing_ok=True)
    except (OSError, ValueError) as error:
        parser.exit(2, f'package_licenses: {error}\n')


if __name__ == '__main__':
    main()
