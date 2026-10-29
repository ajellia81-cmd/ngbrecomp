#!/usr/bin/env python3
"""Check release stripping on a synthetic dylib, never game code or real data."""
import ctypes,hashlib,importlib.util,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ngb_package_mac',root/'package_mac.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
def text_hash(binary):
 lines=subprocess.check_output(['otool','-s','__TEXT','__text',str(binary)],text=True).splitlines()[2:]
 return hashlib.sha256(''.join(''.join(row.split()[1:]) for row in lines).encode()).hexdigest()
with tempfile.TemporaryDirectory(prefix='ngb-release-strip-') as temp:
 work=Path(temp);source=work/'fixture.c';binary=work/'fixture.dylib'
 source.write_text('static __attribute__((noinline)) int local_helper(int n){return n+7;}\n__attribute__((visibility("default"))) int public_bridge(int n){return local_helper(n);}\n')
 subprocess.run(['xcrun','clang','-dynamiclib','-g','-O0','-fvisibility=hidden',str(source),'-o',str(binary)],check=True)
 original=text_hash(binary);before,after=package.compact_macho(binary)
 assert original==text_hash(binary),'Stripping changed executable instructions'
 assert after<=before,'Stripping enlarged fixture'
 subprocess.run(['codesign','--force','--sign','-',str(binary)],check=True)
 dylib=ctypes.CDLL(str(binary));assert dylib.public_bridge(35)==42
bootstrap=(root/'bootstrap.m').read_text()
for marker in ('freopen(', 'macos-on-ios-startup.log', 'setenv("NV2A_FPS_LOG"', 'setenv("NGB_D3D_HITCHLOG"', 'setenv("NGB_D3D_STATS"'):
 assert marker not in bootstrap,marker
assert 'setenv("NGB_D3D_UNIVERSAL","1",0)' in bootstrap
assert 'setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS","0",0)' in bootstrap
print('PASS: stripped instructions and dlsym exports preserved; automatic iOS logging absent; graphics policy retained')
