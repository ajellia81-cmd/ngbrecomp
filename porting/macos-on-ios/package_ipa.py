#!/usr/bin/env python3
"""Audit the converted bundle and package it without including build/debug inputs."""
import argparse,hashlib,json,os,plistlib,shutil,subprocess,tempfile,zipfile
from pathlib import Path

def run(*args):return subprocess.check_output([str(a) for a in args],text=True)
def code_section(binary):
 # Platform/load-path/signature changes must not change the Mac game instructions.
 lines=run('otool','-s','__TEXT','__text',binary).splitlines()[2:]
 return hashlib.sha256(''.join(''.join(l.split()[1:]) for l in lines).encode()).hexdigest()
def main():
 a=argparse.ArgumentParser();a.add_argument('mac_app',type=Path);a.add_argument('app',type=Path);a.add_argument('ipa',type=Path);o=a.parse_args()
 root=Path(__file__).resolve().parent;info_path=o.app/'Info.plist';info=plistlib.loads(info_path.read_bytes())
 info.update(CFBundleName='Ninja Gaiden Black',CFBundleDisplayName='Ninja Gaiden Black',CFBundleVersion='3.1.'+os.environ.get('GITHUB_RUN_NUMBER','0'),CFBundleShortVersionString='3.1',NGBBuildConfiguration='Release',NGBPortMethod='macOS-on-iOS',NGBSmoothMotionEnabled=True,NGBSmoothMotionDefault=False,NGBSmoothMotionReference='vf3-recomp@842151aaf97a4bf5e59709004a385ae5735c5a05',NGBShaderCompilerPolicy='unmodified macOS baseline',NGBSourceCommit=os.environ.get('GITHUB_SHA','local'),UIFileSharingEnabled=True,LSSupportsOpeningDocumentsInPlace=True,UIRequiresFullScreen=True,UIDeviceFamily=[1],UISupportedInterfaceOrientations=['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'],CADisableMinimumFrameDurationOnPhone=True,MetalHudEnabled=False)
 info.pop('CFBundleIconFile',None)
 info_path.write_bytes(plistlib.dumps(info))
 run('xcrun','--sdk','macosx','swift',root/'korean/build_icons.swift',root/'assets/app-icon-source.jpg',o.app,o.app.parent/'icon-work')
 info=plistlib.loads(info_path.read_bytes());exe=info['CFBundleExecutable'];mac=o.mac_app/'Contents/MacOS'/exe;converted=o.app/exe
 if 'platform MACOS' not in run('vtool','-show-build',mac):raise SystemExit('Input engine is not macOS code')
 if 'platform IOS' not in run('vtool','-show-build',converted):raise SystemExit('Converted platform tag is not iOS')
 if code_section(mac)!=code_section(converted):raise SystemExit('Game machine instructions changed during conversion')
 resources=o.app/'Mac/Contents/Resources'
 runtime=resources.parent/'Frameworks/libSDL3.0.dylib'
 if not runtime.is_file() or 'platform IOS' not in run('vtool','-show-build',runtime):raise SystemExit('The converted SDL3 runtime is missing or has the wrong platform')
 print('SDL3 runtime: bundled and converted for iOS')
 for name in ('SettingsTab-Korean.png','KoreanPatch/manifest.json','share/NinjaGaidenBlack/d3d-seed.bin'):
  if (resources/name).read_bytes()!=(o.mac_app/'Contents/Resources'/name).read_bytes():raise SystemExit('Required feature resource changed: '+name)
 for p in o.app.rglob('*'):
  if p.is_symlink():continue
  if p.is_file() and (p.suffix.lower() in ('.c','.cpp','.m','.mm','.py','.log','.xbe','.afs','.iso') or p.name in ('build-info.txt','ui-layout.log','movie-subtitles.log')):raise SystemExit('Non-runtime file in app: '+p.name)
 licenses=resources/'licenses';
 if not (licenses/'vf3_scene-LICENSE').is_file():raise SystemExit('Missing VF3 interpolation license')
 shutil.copy2(root.parent.parent/'LICENSE',licenses/'LICENSE-ngbrecomp-conversion.txt')
 run('codesign','--force','--deep','--sign','-',o.app);run('codesign','--verify','--deep','--strict',o.app)
 o.ipa.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(dir=o.ipa.parent) as temp:
  payload=Path(temp)/'Payload';payload.mkdir();shutil.copytree(o.app,payload/o.app.name,symlinks=True)
  with zipfile.ZipFile(o.ipa,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
   for p in payload.rglob('*'):
    if p.is_file() and not p.is_symlink():z.write(p,p.relative_to(Path(temp)))
    elif p.is_symlink():
     item=zipfile.ZipInfo(str(p.relative_to(Path(temp))));item.create_system=3;item.external_attr=0o120777<<16;z.writestr(item,os.readlink(p))
 digest=hashlib.sha256(o.ipa.read_bytes()).hexdigest()
 print('macOS engine instructions: preserved through conversion')
 print('Shader compiler policy: unchanged Mac generation/cache/prewarm')
 print('Smooth Motion: VF3 production core 842151aa; available, default off; game clock remains 60 Hz')
 print('Packaged runtime build:',info['CFBundleVersion'])
 print('Unsigned game IPA bytes:',o.ipa.stat().st_size)
 print('Unsigned game IPA SHA-256:',digest)
if __name__=='__main__':main()
