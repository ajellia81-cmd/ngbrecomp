#!/usr/bin/env python3
"""Reject legacy renderer/interpolation/loading changes in the new upstream port."""
from pathlib import Path
import subprocess,re
root=Path(__file__).resolve().parents[2]
base='2606383704e2675b687fec89fb3dfa965b068d46'
def original(path):return subprocess.check_output(['git','show',base+':'+path],cwd=root)
files=subprocess.check_output(['git','ls-tree','-r','--name-only',base,'src/d3d'],cwd=root,text=True).splitlines()
# Only screen-space text/overlay layout extends the original graphics directory.
for path in files:
 if path=='src/d3d/vk/quick_menu.inc':continue
 if path in ('src/d3d/vk/spark_aspect.h','src/d3d/vk/glow_options.h'):
  clean=re.sub(r'#ifdef NGB_CONVERTED_DISPLAY\n.*?#endif\n','',(root/path).read_text(),flags=re.S)
  assert clean.encode()==original(path),path+' changed beyond native iPhone aspect'
  continue
 assert (root/path).read_bytes()==original(path),path+' changed from upstream'
for path in ('xboxrecomp/src/platform/vcpu.c','xboxrecomp/src/kernel/kernel_hal.c'):
 assert (root/path).read_bytes()==original(path),path+' changed from upstream'
for name in ('src/d3d/gpu/scene_motion.cpp','src/d3d/vk/motion_replay.inc','src/d3d/vk/universal_backend.inc','third_party/vf3_scene'):
 assert not (root/name).exists(),name+' must not be carried over'
for path in ('porting/macos-on-ios/bootstrap.m','porting/macos-on-ios/target.cmake'):
 s=(root/path).read_text()
 for forbidden in ('NGB_SMOOTH_MOTION','NGB_D3D_UNIVERSAL','NGB_D3D_SYNC_COMPILE','shader_prewarm_ready','cache_producer_fast','scene_motion.cpp'):
  assert forbidden not in s,(path,forbidden)
print('PASS: upstream 0.9.72 graphics/shaders/interpolation/loading/CPU clock retained; no legacy VF3 or universal fallback')
