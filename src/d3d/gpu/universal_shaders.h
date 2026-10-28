#pragma once
#include "gpu/shaders.h"
#include "gpu/vertex.h"

namespace gpu {
// A fixed ABI and fixed shader sources: draw state is data, never source text.
// uint4 storage avoids HLSL cbuffer scalar-array packing differences.
struct UniversalConstants {
    std::array<std::array<uint32_t,4>,224> words{};
    uint32_t& operator[](size_t i) {return words[i/4][i%4];}
    uint32_t operator[](size_t i) const {return words[i/4][i%4];}
};
enum UniversalWord : uint32_t {
    uPixel=0,uFlags=85,uRegisters=86,uDecodeLo=87,uDecodeHi=88,uProgramWords=89,
    uFixed=96,uVertex=112,uBlend=176,uBlendConstant=180,uProgram=192
};
enum UniversalFlag : uint32_t {
    uPretransformed=1,uProgrammableVertex=2,uPoints=4,uSprite=8,uQuad=16,uSpark=32,uBlending=64
};
UniversalConstants MakeUniversalConstants(const State&,const Assembled&,const PixelKey&,uint32_t flags);
void UniversalVertexOffsets(UniversalConstants&,const Assembled&,const uint64_t *offsets);
std::string UniversalVertexHlsl();
std::string UniversalPixelHlsl(bool multisampled,bool depth_only=false,bool blend_in_shader=true);
}
