#!/usr/bin/env python3
"""Derive Apple app icons from the shared opaque PNG; build-time only."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets/branding/app-icon.png'
MAX_SOURCE = 16 * 1024 * 1024


def sha(data): return hashlib.sha256(data).hexdigest()


def validate_source(source: Path):
    # Apple tools are the image decoder; the master must already be opaque.
    result = subprocess.run(['/usr/bin/sips','-g','format','-g','pixelWidth',
        '-g','pixelHeight','-g','hasAlpha',str(source)],check=True,
        capture_output=True,text=True,timeout=30)
    properties = {}
    for line in result.stdout.splitlines():
        key, separator, value = line.strip().partition(':')
        if separator and key in ('format','pixelWidth','pixelHeight','hasAlpha'):
            properties[key] = value.strip()
    if properties != {'format':'png','pixelWidth':'1024','pixelHeight':'1024','hasAlpha':'no'}:
        raise ValueError('Use a 1024x1024 PNG with no alpha channel (sips hasAlpha=no)')


def ios_images():
    rows=[]
    for idiom, sizes in [('iphone', [('20x20',2),('20x20',3),('29x29',2),('29x29',3),('40x40',2),('40x40',3),('60x60',2),('60x60',3)]),
                         ('ipad', [('20x20',1),('20x20',2),('29x29',1),('29x29',2),('40x40',1),('40x40',2),('76x76',1),('76x76',2),('83.5x83.5',2)]),
                         ('ios-marketing',[('1024x1024',1)])]:
        for size,scale in sizes:
            pixels=int(float(size.split('x')[0])*scale)
            rows.append({'idiom':idiom,'size':size,'scale':f'{scale}x','filename':f'icon-{pixels}.png'})
    return rows


def build_icons(source: Path, output: Path, platform: str) -> Path:
    source, output = Path(source), Path(output).resolve()
    if sys.platform != 'darwin': raise ValueError('Apple icon derivation requires macOS sips/iconutil')
    if source.is_symlink() or not source.is_file(): raise ValueError('Shared app-icon PNG is missing or linked')
    if output == ROOT/'build' or not output.is_relative_to(ROOT/'build'):
        raise ValueError('Icon output must be below this checkout ignored build/')
    if platform not in ('mac','ios'): raise ValueError('Choose mac or ios icons')
    if source.stat().st_size > MAX_SOURCE: raise ValueError('App-icon source exceeds 16 MiB')
    validate_source(source)
    data = source.read_bytes()
    identity = {'source_sha256':sha(data),'builder_sha256':sha(Path(__file__).read_bytes()),'platform':platform}
    key = sha(json.dumps(identity,sort_keys=True).encode())[:24]
    output.mkdir(parents=True,exist_ok=True)
    destination = output/(platform+'-'+key)
    product = Path('AppIcon.icns') if platform == 'mac' else Path('AppIcon.xcassets')
    if destination.exists():
        report_path = destination/'icon-build.json'
        if destination.is_symlink() or report_path.is_symlink(): raise ValueError('Linked icon output is not accepted')
        report=json.loads(report_path.read_text())
        if report['identity'] != identity: raise ValueError('Existing icon cache identity differs')
        actual={p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file() and p!=report_path}
        if actual != set(report['files']): raise ValueError('Existing icon cache file inventory differs')
        for name,expected in report['files'].items():
            path=destination/name
            if path.is_symlink() or not path.resolve().is_relative_to(destination) or sha(path.read_bytes())!=expected:
                raise ValueError('Existing icon cache bytes differ; choose a new build directory')
        return destination/product
    with tempfile.TemporaryDirectory(prefix='.icons-',dir=output) as folder:
        stage=Path(folder)
        def resize(size,path):
            if size==1024: shutil.copyfile(source,path)
            else: subprocess.run(['/usr/bin/sips','-z',str(size),str(size),str(source),'--out',str(path)],check=True,stdout=subprocess.DEVNULL,timeout=30)
        if platform=='mac':
            icons=stage/'AppIcon.iconset';icons.mkdir()
            for size in (16,32,128,256,512):
                resize(size,icons/f'icon_{size}x{size}.png')
                resize(size*2,icons/f'icon_{size}x{size}@2x.png')
            subprocess.run(['/usr/bin/iconutil','-c','icns','-o',str(stage/product),str(icons)],check=True,timeout=30)
        else:
            catalog=stage/product;appicons=catalog/'AppIcon.appiconset';appicons.mkdir(parents=True)
            info={'author':'sega-rally-recomp','version':1}
            (catalog/'Contents.json').write_text(json.dumps({'info':info},indent=2)+'\n')
            rows=ios_images()
            for size in sorted({int(float(row['size'].split('x')[0])*int(row['scale'][0])) for row in rows}): resize(size,appicons/f'icon-{size}.png')
            (appicons/'Contents.json').write_text(json.dumps({'images':rows,'info':info},indent=2)+'\n')
        if sha(source.read_bytes()) != identity['source_sha256']:
            raise ValueError('Shared app icon changed during generation')
        files={p.relative_to(stage).as_posix():sha(p.read_bytes()) for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage/'icon-build.json').write_text(json.dumps({'schema':1,'identity':identity,'files':files},indent=2)+'\n')
        if destination.exists(): raise ValueError('Concurrent icon generation; retry after the other build completes')
        stage.rename(destination)
    return destination/product


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--platform',choices=['mac','ios'],required=True)
    args=parser.parse_args()
    try: print(build_icons(args.source,args.output,args.platform));return 0
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'App icon: {error}',file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
