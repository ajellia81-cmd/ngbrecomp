"""Compare real original endpoint HLSL with Motion off/on; no game inputs or GPU."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
base='5fc57d84d617aa7f360cf0a5ab707dc9334f4b95'
emitter=r'''
#include "gpu/shaders.h"
#include <iostream>
int main(){
 std::cout<<gpu::FixedVertexHlsl(1,false,0,gpu::FixedKey{});
 std::cout<<gpu::FixedVertexHlsl(1,true,0,gpu::FixedKey{});
 std::cout<<gpu::VertexProgramHlsl({},1,0);
 std::cout<<gpu::PixelHlsl(gpu::PixelKey{});
}
'''
with tempfile.TemporaryDirectory(prefix='ngb-endpoint-shader-') as temp:
 work=Path(temp);(work/'gpu').mkdir();(work/'emit.cpp').write_text(emitter)
 for name in ('shaders.cpp','shaders.h'):
  (work/'gpu'/name).write_bytes(subprocess.check_output(['git','show',base+':src/d3d/gpu/'+name],cwd=root))
 compiler=os.environ.get('CXX','c++');outputs={}
 for label,source,include,flags in [('original',work/'gpu/shaders.cpp',work,[]),('off',root/'src/d3d/gpu/shaders.cpp',root/'src/d3d',[]),('on',root/'src/d3d/gpu/shaders.cpp',root/'src/d3d',['-DNGB_SMOOTH_MOTION=1'])]:
  exe=work/label
  subprocess.run([compiler,'-std=c++20','-O0',*flags,'-I'+str(include),'-I'+str(root/'src/d3d'),str(work/'emit.cpp'),str(source),'-o',str(exe)],check=True,capture_output=True)
  outputs[label]=subprocess.check_output([str(exe)])
 assert outputs['original']==outputs['off'],'Motion-off endpoint shaders changed'
 assert outputs['original']==outputs['on'].replace(b'    float4 motion;\n',b''),'Motion-on changed the original endpoint program'
 print('Original endpoint HLSL preserved with Motion off and on; only an unused replay-switch uniform is added')
