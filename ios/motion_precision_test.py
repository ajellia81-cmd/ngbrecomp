"""Check the actual interpolation helper's SPIR-V arithmetic precision."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def arithmetic_is_precise(path):
    data=path.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
    assert words[0]==0x07230203
    protected=set();arithmetic=[];i=5
    while i<len(words):
        size=words[i]>>16;opcode=words[i]&65535
        assert size and i+size<=len(words)
        operands=words[i+1:i+size]
        if opcode==71 and len(operands)>=2 and operands[1]==42:
            protected.add(operands[0]) # OpDecorate ... NoContraction
        if opcode in (129,131,133,142): # FAdd, FSub, FMul, VectorTimesScalar
            arithmetic.append(operands[1])
        i+=size
    return len(arithmetic)>=3 and all(value in protected for value in arithmetic)


def main():
    folder=Path(sys.argv[1]);source=(folder/'motion-fixed.vert.hlsl').read_text()
    start=source.index('float4 MotionClipPosition(');end=source.index('\n}',start)+2
    helper=source[start:end]
    main_shader='''
RWStructuredBuffer<float4> points : register(u0);
[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {
 points[2]=MotionClipPosition(points[0],points[1],points[3].x);
}
'''
    with tempfile.TemporaryDirectory() as directory:
        directory=Path(directory)
        for name,body,expected in [('actual',helper,True),('unprotected',helper.replace('precise ',''),False)]:
            hlsl=directory/(name+'.hlsl');spv=directory/(name+'.spv')
            hlsl.write_text(body+main_shader)
            subprocess.run(['glslangValidator','-D','-V','--target-env','vulkan1.1','-S','comp','-e','main',str(hlsl),'-o',str(spv)],check=True,capture_output=True)
            assert arithmetic_is_precise(spv)==expected,name
    print('Motion depth precision: production subtract/multiply/add have NoContraction; unsafe control rejected (compiler check, no GPU execution)')
if __name__=='__main__': main()
