#!/usr/bin/env python3
"""Prepare public ARM64 Homebrew bottles for an Intel Mac build host.

Only the isolated output prefix is written. ARM64 tools are never executed;
Homebrew, Python, pkg-config, CMake and the Apple compiler run on the host.
"""
import argparse,hashlib,json,os,shutil,subprocess,tarfile
from pathlib import Path

ROOTS=['sdl2-compat','sdl3','libepoxy','glib','glslang','molten-vk','vulkan-loader']
def is_macho(head):
 # Java class files share CAFEBABE with fat Mach-O. Validate the architecture
 # table, rather than treating every matching four-byte magic as executable.
 magic=head[:4]
 if magic in (b'\xcf\xfa\xed\xfe',b'\xfe\xed\xfa\xcf'):return len(head)>=32
 if magic not in (b'\xca\xfe\xba\xbe',b'\xbe\xba\xfe\xca',b'\xca\xfe\xba\xbf'):return False
 endian='little' if magic==b'\xbe\xba\xfe\xca' else 'big'
 count=int.from_bytes(head[4:8],endian);width=32 if magic==b'\xca\xfe\xba\xbf' else 20
 if not 1<=count<=64 or len(head)<8+width*count:return False
 cpus={7,12,18,0x01000007,0x0100000c,0x01000012,0x0200000c}
 return all(int.from_bytes(head[8+i*width:12+i*width],endian) in cpus for i in range(count))
def run(*args,env=None):return subprocess.check_output([str(x) for x in args],text=True,env=env).strip()
def mapping(prefix):
 return {'@@HOMEBREW_CELLAR@@':str(prefix/'Cellar'),'@@HOMEBREW_PREFIX@@':str(prefix),'@@HOMEBREW_REPOSITORY@@':str(prefix),'/opt/homebrew':str(prefix)}
def relocated(text,prefix):
 for old,new in mapping(prefix).items():text=text.replace(old,new)
 return text
def mirror(source,dest):
 if source.is_dir() and not source.is_symlink():
  dest.mkdir(parents=True,exist_ok=True)
  for child in source.iterdir():mirror(child,dest/child.name)
 elif not dest.exists() and not dest.is_symlink():
  dest.parent.mkdir(parents=True,exist_ok=True);dest.symlink_to(source)
def prepare(prefix,cache):
 if run('uname','-m')!='x86_64':raise SystemExit('This fallback is for an Intel Mac build host')
 if prefix.exists():raise SystemExit('Use a new isolated dependency prefix')
 prefix.mkdir(parents=True);(prefix/'Cellar').mkdir();(prefix/'opt').mkdir();cache.mkdir(parents=True,exist_ok=True)
 env=os.environ.copy();env.update(HOMEBREW_CACHE=str(cache),HOMEBREW_NO_AUTO_UPDATE='1',HOMEBREW_NO_ENV_HINTS='1',HOMEBREW_NO_INSTALL_FROM_API='1')
 # Intel's compatibility API intentionally omits current ARM bottles. The
 # public formula checkout retains all architectures and their verified hashes.
 core=Path(run('brew','--repo','homebrew/core',env=env))
 if not (core/'Formula').is_dir():subprocess.run(['brew','tap','--force','homebrew/core'],env=env,check=True)
 # Required runtime dependencies only; receipt metadata below also covers ARM-specific dependencies.
 pending=list(ROOTS);seen=set();manifest=[]
 while pending:
  name=pending.pop(0).strip()
  if not name or name in seen:continue
  formula=json.loads(run('brew','info','--json=v2','--variations','--formula',name,env=env))['formulae'][0]
  pending.extend(formula.get('dependencies',[]))
  pending.extend(formula.get('recommended_dependencies',[]))
  for platform,variation in (formula.get('variations') or {}).items():
   if platform.startswith('arm64_') and 'linux' not in platform:
    pending.extend(variation.get('dependencies',[]));pending.extend(variation.get('recommended_dependencies',[]))
  # Platform-specific Homebrew API responses expose only that platform's bottles.
  # Simulate ARM64 explicitly, rather than reading Intel formula metadata.
  archive=None;tag=None
  for system in ('sequoia','sonoma','tahoe'):
   flags=['--arch=arm','--os='+system,'--force-bottle']
   result=subprocess.run(['brew','fetch','--formula','--retry',*flags,name],env=env)
   if result.returncode:continue
   candidate=run('brew','--cache',*flags,name,env=env)
   if candidate and '.bottle' in candidate and Path(candidate).is_file():archive=Path(candidate);tag='arm64_'+system;break
  if archive is None:raise SystemExit('No supported ARM64 Mac bottle: '+name)
  # brew fetch verifies against Homebrew's published digest before this point.
  digest=hashlib.sha256(archive.read_bytes()).hexdigest()
  before=set(p for p in (prefix/'Cellar').glob('*/*') if p.is_dir())
  with tarfile.open(archive) as tar:tar.extractall(prefix/'Cellar',filter='data')
  kegs=set(p for p in (prefix/'Cellar').glob('*/*') if p.is_dir())-before
  if len(kegs)!=1:
   raise SystemExit('Unexpected bottle layout: '+name+'; top entries: '+str(list((prefix/'Cellar').glob('*/*'))[:12]))
  keg=kegs.pop();canonical=keg.parent.name
  alias=prefix/'opt'/canonical
  if not alias.exists():alias.symlink_to(keg)
  # Modern bottles can omit the installed receipt. Formula dependencies are
  # already included above; use receipt metadata as an additional check when present.
  receipt=keg/'INSTALL_RECEIPT.json'
  if receipt.is_file():pending.extend(x['full_name'] for x in json.loads(receipt.read_text()).get('runtime_dependencies',[]))
  seen.add(name);seen.add(canonical)
  manifest.append({'formula':name,'tag':tag,'sha256':digest,'keg':str(keg.relative_to(prefix))})
  # The cache belongs solely to this task, not to the host Homebrew installation.
  if archive.resolve().is_relative_to(cache.resolve()):archive.unlink()
 # Relocate bottle text placeholders and dynamic linkage, never machine instructions.
 for path in (prefix/'Cellar').rglob('*'):
  if not path.is_file() or path.is_symlink():continue
  with path.open('rb') as f:head=f.read(4096)
  if is_macho(head) or head.startswith(b'!<arch>\n'):
   architectures=run('lipo','-archs',path).split()
   if 'arm64' not in architectures:raise SystemExit('Non-ARM64 object in ARM bottle: '+str(path))
   path.chmod(path.stat().st_mode|0o200)
   if len(architectures)>1:
    thin=path.with_name(path.name+'.arm64');subprocess.run(['lipo',str(path),'-thin','arm64','-output',str(thin)],check=True);thin.replace(path)
    with path.open('rb') as f:head=f.read(4096)
   if head.startswith(b'!<arch>\n'):continue
   endian='little' if head[:4]==b'\xcf\xfa\xed\xfe' else 'big'
   if int.from_bytes(head[12:16],endian) not in (6,8):continue
   rows=run('otool','-L',path).splitlines()[1:]
   for row in rows:
    dep=row.strip().split(' (')[0];new=relocated(dep,prefix)
    if new!=dep:subprocess.run(['install_name_tool','-change',dep,new,str(path)],check=True)
   ids=run('otool','-D',path).splitlines()[1:]
   if ids:
    old=ids[0].strip();new=relocated(old,prefix)
    if old!=new:subprocess.run(['install_name_tool','-id',new,str(path)],check=True)
  elif b'\0' not in head and path.stat().st_size<8*1024*1024:
   try:text=path.read_text()
   except UnicodeError:continue
   new=relocated(text,prefix)
   if text!=new:path.chmod(path.stat().st_mode|0o200);path.write_text(new)
 for opt in (prefix/'opt').iterdir():
  # CMake imported targets validate packaged tools even when cross compiling.
  # Mirror ARM tools for path completeness; never execute them on the Intel host.
  for directory in ('bin','include','lib','share'):
   source=opt/directory
   if source.exists():mirror(source,prefix/directory)
 for name in ('libvulkan.1.dylib','libMoltenVK.dylib','libSDL3.0.dylib'):
  p=prefix/'lib'/name
  if not p.is_file():raise SystemExit('Missing required ARM runtime: '+name)
  if run('lipo','-archs',p)!='arm64':raise SystemExit('Incorrect runtime architecture: '+name)
 (prefix/'arm64-bottles.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Prepared public ARM64 Mac dependency prefix:',prefix)
 print('Runtime architecture verified: arm64; no ARM programs executed on host')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('prefix',type=Path);p.add_argument('cache',type=Path);a=p.parse_args();prepare(a.prefix.resolve(),a.cache.resolve())
