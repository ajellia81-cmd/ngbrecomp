#!/usr/bin/env python3
"""Bundle the macOS engine directly, without the subprocess launcher or Qt installer."""
import argparse,json,plistlib,shutil,subprocess
from pathlib import Path

def run(*a):return subprocess.check_output([str(x) for x in a],text=True)
def dependencies(p):return [l.strip().split(' (')[0] for l in run('otool','-L',p).splitlines()[1:]]
def main():
 a=argparse.ArgumentParser();a.add_argument('binary',type=Path);a.add_argument('app',type=Path);a.add_argument('korean',type=Path);o=a.parse_args()
 if o.app.exists():raise SystemExit('Output exists; use a new scratch path')
 root=Path(__file__).resolve().parents[2];prefix=Path(run('brew','--prefix').strip());res=o.app/'Contents/Resources';libs=o.app/'Contents/Frameworks';mac=o.app/'Contents/MacOS'
 for p in (res,libs,mac):p.mkdir(parents=True)
 binary=mac/'NinjaGaidenBlack';shutil.copy2(o.binary,binary)
 queue=[(binary,o.binary)];seen={};requested=['libvulkan.1.dylib','libMoltenVK.dylib','libSDL3.0.dylib']
 for name in requested:
  src=prefix/'lib'/name;dst=libs/name;shutil.copy2(src.resolve(),dst);seen[name]=src;queue.append((dst,src))
 while queue:
  target,source=queue.pop(0)
  for dep in dependencies(target):
   if dep.startswith(('/usr/lib/','/System/Library/')):continue
   if dep.startswith('@loader_path/'):src=source.parent/dep.removeprefix('@loader_path/')
   elif dep.startswith('@rpath/'):src=prefix/'lib'/dep.removeprefix('@rpath/')
   else:src=Path(dep)
   name=src.name
   if name==target.name:continue
   if name not in seen:
    shutil.copy2(src.resolve(),libs/name);seen[name]=src;queue.append((libs/name,src))
   run('install_name_tool','-change',dep,'@rpath/'+name,target)
 for lib in libs.iterdir():
  run('install_name_tool','-id','@rpath/'+lib.name,lib)
  subprocess.run(['install_name_tool','-add_rpath','@loader_path',str(lib)],check=True)
 run('install_name_tool','-add_rpath','@executable_path/../Frameworks',binary)
 for name in ('libvulkan.dylib','libSDL3.dylib'):
  actual={'libvulkan.dylib':'libvulkan.1.dylib','libSDL3.dylib':'libSDL3.0.dylib'}[name]
  if (libs/actual).exists() and not (libs/name).exists():(libs/name).symlink_to(actual)
 share=res/'share/NinjaGaidenBlack';(share/'shader-seed').mkdir(parents=True)
 shutil.copy2(root/'packaging/d3d-seed.bin',share/'d3d-seed.bin')
 for name in ('states.bin','pipelines.bin'):shutil.copy2(root/'packaging/shader-seed'/name,share/'shader-seed'/name)
 icd=res/'vulkan/icd.d';icd.mkdir(parents=True)
 (icd/'MoltenVK_icd.json').write_text(json.dumps({'file_format_version':'1.0.0','ICD':{'library_path':'../../../Frameworks/libMoltenVK.dylib','api_version':'1.3.0','is_portability_driver':True}}))
 shutil.copytree(o.korean,res/'KoreanPatch')
 shutil.copy2(root/'porting/macos-on-ios/assets/settings-ko.png',res/'SettingsTab-Korean.png')
 licenses=res/'licenses';licenses.mkdir()
 for f in [root/'LICENSE',root/'THIRD_PARTY.md',root/'xboxrecomp/LICENSE',root/'xboxrecomp/NOTICE',root/'src/d3d/NOTICE',root/'third_party/vf3_scene/LICENSE',*root.glob('assets/fonts/OFL-*.txt')]:
  if f.exists():shutil.copy2(f,licenses/(f.parent.name+'-'+f.name))
 info={'CFBundleName':'Ninja Gaiden Black','CFBundleDisplayName':'Ninja Gaiden Black','CFBundleExecutable':'NinjaGaidenBlack','CFBundleIdentifier':'com.ngbrecomp.game','CFBundleVersion':'3.1','CFBundleShortVersionString':'3.1','CFBundlePackageType':'APPL','NSHighResolutionCapable':True,'LSMinimumSystemVersion':'26.0'}
 (o.app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
 for p in [*libs.iterdir(),binary]:
  if not p.is_symlink():run('codesign','--force','--sign','-',p)
 run('codesign','--force','--sign','-',o.app)
 print('macOS ARM64 bundle prepared; direct game entry; Korean resources included')
if __name__=='__main__':main()
