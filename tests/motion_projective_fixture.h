#pragma once
#include <array>
#include <vector>
#include <cstdint>
// Original synthetic shader: projected UVs share a clip temporary with oPos.
// No game shader bytes, hashes or assets are included in this fixture.
namespace motion_fixture {
struct Source{unsigned mux=1,reg=0;std::array<unsigned,4> sw{0,1,2,3};};
inline void emit(std::vector<uint32_t>& words,unsigned mac,unsigned ilu,unsigned constant,unsigned reg,unsigned mask,Source a={},Source b={},Source c={},unsigned imask=0,unsigned omask=0,unsigned output=0,bool output_ilu=false){
    uint32_t w1=(mac<<21)|(ilu<<25)|(constant<<13),w2=(a.mux<<26)|(a.reg<<28)|(b.mux<<11)|(b.reg<<13);
    for(unsigned i=0;i<4;i++){w1|=a.sw[i]<<(6-i*2);w2|=b.sw[i]<<(23-i*2);w2|=c.sw[i]<<(8-i*2);}
    w2|=c.reg>>2;
    uint32_t w3=(c.mux<<28)|((c.reg&3)<<30)|(reg<<20)|(mask<<24)|(imask<<16)|(omask<<12)|(output<<3);
    if(omask)w3|=1u<<11;
    if(output_ilu)w3|=4;
    words.insert(words.end(),{0,w1,w2,w3});
}
inline std::vector<uint32_t> projective_program(){
    std::vector<uint32_t> p;
    for(unsigned r=0;r<4;r++)emit(p,7,0,64+r,4,8u>>r,{2},{3});
    // Parallel MAD derives a UV numerator while MOV publishes its clip source.
    emit(p,4,1,70,5,15,{1,4,{3,3,3,3}},{3},{1,4},0,15,0,true);
    emit(p,2,0,71,5,15,{1,5},{3});
    emit(p,0,2,0,6,0,{},{},{1,5,{3,3,3,3}},1);
    emit(p,2,0,0,0,0,{1,5},{1,6,{3,3,3,3}},{},0,15,9);
    emit(p,2,3,58,0,0,{1,12},{3},{1,12,{3,3,3,3}},8,14);
    emit(p,4,0,59,0,0,{1,12},{1,1,{0,0,0,0}},{3},0,14);
    p.back()|=1;
    return p;
}
}
