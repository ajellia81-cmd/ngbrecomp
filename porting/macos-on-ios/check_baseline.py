#!/usr/bin/env python3
"""Check the clean rendering/compilation baseline before any private build."""
from pathlib import Path
import subprocess,re
root=Path(__file__).resolve().parents[2]
base='5fc57d84d617aa7f360cf0a5ab707dc9334f4b95'
def original(path):return subprocess.check_output(['git','show',base+':'+path],cwd=root,text=True)
def function(source,name):
 start=source.index(name);brace=source.index('{',start);depth=1;i=brace+1
 # These source routines contain no braces in raw strings.
 while depth:
  depth+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[start:i]
path='src/d3d/vk/backend.cpp';before=original(path);after=(root/path).read_text()
for name in ('void VulkanBackend::CacheOpen(', 'void VulkanBackend::Prewarm(', 'void VulkanBackend::CacheRecord(', 'void VulkanBackend::WarmSamples('):
 if name not in before:continue
 assert function(before,name)==function(after,name),name+' changed'
for path in ('xboxrecomp/src/platform/vcpu.c','xboxrecomp/src/kernel/kernel_hal.c'):
 assert (root/path).read_text()==original(path),path+' changed'
for path in ('porting/macos-on-ios/bootstrap.m','porting/macos-on-ios/AppKit_NGB.m','porting/macos-on-ios/korean/game_hooks.c','porting/macos-on-ios/korean/wrap_game_codegen.py'):
 s=(root/path).read_text()
 for forbidden in ('NGB_D3D_SYNC_COMPILE','shader_prewarm_ready','cache_producer_fast','cache_copy_delay'):
  assert forbidden not in s,(path,forbidden)
assert 'ROW_SMOOTH,' in (root/'src/pc_menu.c').read_text().split('s_rows_gfx[] = {',1)[1].split('};',1)[0]
assert 'ROW_SMOOTH,' not in (root/'src/pc_menu.c').read_text().split('s_rows_disp[] = {',1)[1].split('};',1)[0]
for name in ('std::string VertexProgramHlsl(', 'std::string FixedVertexHlsl(', 'std::string PixelHlsl('):
 path='src/d3d/gpu/shaders.cpp'
 assert function(original(path),name)==function((root/path).read_text(),name),name+' changed'
for forbidden in ('MotionPrewarm(', 'motion_budget_', 'shader_prewarm_ready', 'cache_producer_fast'):
 assert forbidden not in after,forbidden
print('Original Mac prewarm/cache, endpoint shader generators and CPU timing preserved; no old loading workarounds or adaptive cooldown')
