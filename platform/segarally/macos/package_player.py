#!/usr/bin/env python3
"""Package a prebuilt game as a ROM-import app or a private personal app."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'platform/segarally'))
from package_licenses import collect

def sha(path):
    with path.open('rb') as f:
        h=hashlib.sha256()
        for chunk in iter(lambda:f.read(65536),b''):h.update(chunk)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--edition',choices=('import','personal'),required=True)
    parser.add_argument('--executable-sha256',required=True)
    args=parser.parse_args()
    build=args.build_dir.resolve();out=args.output.resolve()
    if out.exists() or not out.is_relative_to(ROOT/'build') or out==ROOT/'build':
        parser.error('Choose a new output directory inside ignored build/')
    exe=build/'segarally_native'
    if exe.is_symlink() or not exe.is_file() or sha(exe)!=args.executable_sha256:
        parser.error('The prebuilt game executable does not match the reviewed input')
    arch=subprocess.check_output(['lipo','-archs',str(exe)],text=True).strip()
    if arch!='arm64':parser.error('This package currently validates Apple Silicon only')
    notices=collect(ROOT,ROOT/'extern/sdl3',ROOT/'extern/imgui',ROOT/'extern/lzma')
    for name,data in notices.items():
        if (build/'licenses'/name).read_bytes()!=data:parser.error('Rebuild current distribution notices before packaging')
    assets=build/'assets'
    if assets.is_symlink() or (assets/'assets.json').is_symlink():parser.error('Game input manifest must not be a symbolic link')
    manifest=json.loads((assets/'assets.json').read_text())
    expected=json.loads((ROOT/'config/segarally_assets.json').read_text())
    if manifest['romset']!=expected['romset'] or manifest['images']!=expected['images']:
        parser.error('Game inputs must match all eight fixed Revision B image identities')
    for name,row in manifest['images'].items():
        image=assets/row['file']
        if image.is_symlink() or not image.resolve().is_relative_to(assets.resolve()) or image.stat().st_size!=row['size'] or sha(image)!=row['sha256']:
            parser.error('Game input identity mismatch: '+name)
    out.mkdir(parents=True)
    stage=out/'stage';stage.mkdir()
    app=stage/'Sega Rally Native.app';contents=app/'Contents';resources=contents/'Resources'
    (contents/'MacOS').mkdir(parents=True);resources.mkdir()
    shutil.copy2(exe,contents/'MacOS/segarally_native')
    if sha(contents/'MacOS/segarally_native')!=args.executable_sha256:
        raise RuntimeError('Copied game executable differs from the reviewed input')
    (resources/'licenses').mkdir()
    for name,data in notices.items():(resources/'licenses'/name).write_bytes(data)
    if args.edition=='personal':
        (resources/'assets').mkdir()
        for name in ['assets.json']+[row['file'] for row in manifest['images'].values()]:
            shutil.copy2(assets/name,resources/'assets'/name)
    text=('Sega Rally Native — prebuilt Apple Silicon game\n\n'
          'Drag Sega Rally Native.app to Applications and open it.\n'
          'CRT and sound start enabled; F1 opens Settings, F6 toggles CRT.\n'
          'The arcade startup is initially silent before attract audio begins.\n'
          'Keys: C coin, Return start, arrows steer, Z gas, X brake.\n\n')
    text+=('Personal ready-to-play edition. Includes your verified game data. Keep this package private.\n'
           if args.edition=='personal' else
           'Choose your own supported Sega Rally Revision B ZIP on first launch.\n'
           'No Python, CMake, Xcode, compiler or network connection is needed to play.\n'
           'No ROM archive or extracted game data is included in this edition.\n')
    text+=('\nThe executable contains statically recompiled game instructions. The project BSD license\n'
           'covers the original runtime and tools, not Sega game content.\n'
           'Experimental preview; ad-hoc signed and not notarized.\n')
    (resources/'README.txt').write_text(text)
    with (contents/'Info.plist').open('wb') as f:
        plistlib.dump({'CFBundleIdentifier':'com.lgarciacarro.segarally.macnative',
            'CFBundleName':'Sega Rally Native','CFBundleDisplayName':'Sega Rally Native',
            'CFBundlePackageType':'APPL','CFBundleExecutable':'segarally_native',
            'CFBundleShortVersionString':'0.1.0','CFBundleVersion':'3',
            'NSHighResolutionCapable':True,'NSPrincipalClass':'NSApplication'},f)
    def run(argv,name):
        result=subprocess.run(list(map(str,argv)),capture_output=True,text=True,timeout=180)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
    run(['codesign','--force','--sign','-',app],'sign')
    run(['codesign','--verify','--deep','--strict',app],'signature')
    files={}
    for p in sorted(app.rglob('*')):
        if p.is_symlink():raise RuntimeError('Unexpected symlink in player app')
        if p.is_file():files[str(p.relative_to(app))]={'size':p.stat().st_size,'sha256':sha(p)}
    if args.edition=='import' and any('/assets/' in p or p.endswith(('.bin','.zip','.inputs')) for p in files):
        raise RuntimeError('Unexpected game data in public app')
    (stage/'Applications').symlink_to('/Applications',target_is_directory=True)
    dmg=out/f'Sega-Rally-Native-macOS-arm64-{args.edition}.dmg'
    run(['hdiutil','create','-format','UDZO','-volname','Sega Rally Native','-srcfolder',stage,dmg],'create-dmg')
    run(['hdiutil','verify',dmg],'verify-dmg')
    if sha(exe)!=args.executable_sha256:raise RuntimeError('Prebuilt input changed during packaging')
    report={'schema':1,'status':'packaged','edition':args.edition,'architecture':arch,
        'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'input_executable_sha256':args.executable_sha256,'contains_recompiled_game_program':True,
        'game_assets_included':args.edition=='personal','requires_development_tools':False,
        'notarized':False,'app':str(app),'files':files,
        'dmg':{'path':str(dmg),'size':dmg.stat().st_size,'sha256':sha(dmg)},
        'scope':'Package/signature/byte validation; gameplay evidence is recorded separately.'}
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(dmg)

if __name__=='__main__':main()
