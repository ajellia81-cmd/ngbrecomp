// Validate the shipped recipe ABI and reconstruct shaders through the actual
// production generators. No GPU, game execution, settings or saves are used.
// Optional shader output is scratch-only and must never be published.
#include "gpu/shaders.h"
#ifdef NGB_SEED_COMPILE
#include "vk/spirv.h"
#endif
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
using namespace gpu;
static void check(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
static std::vector<uint8_t> read(const char *path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"cannot read test input");
    return {std::istreambuf_iterator<char>(input),{}};
}
template<class T> static T at(const std::vector<uint8_t>& data,size_t offset) {
    check(offset<=data.size() && sizeof(T)<=data.size()-offset,"truncated recipe");
    T value;std::memcpy(&value,data.data()+offset,sizeof value);return value;
}
int main(int argc,char **argv) {try {
    check(argc>=2 && argc<=4,"usage: d3d_seed_validate seed [executable [scratch_shader_dir]]");
    static_assert(sizeof(FixedKey)==28 && sizeof(PixelKey)==340,"recipe ABI changed");
    auto data=read(argv[1]);std::vector<uint8_t> xbe;
    if(argc>=3) xbe=read(argv[2]);
    check(data.size()>=8 && !std::memcmp(data.data(),"NGBD3DC1",8),"bad seed magic");
    std::set<uint64_t> vertices,pixels;std::vector<std::pair<uint64_t,uint64_t>> pipelines;
    std::map<char,unsigned> counts;size_t offset=8;unsigned emitted=0,refs=0;
    while(offset<data.size()) {
        check(data.size()-offset>=13,"short record header");
        char kind=char(data[offset]);uint64_t key=at<uint64_t>(data,offset+1);
        uint32_t size=at<uint32_t>(data,offset+9);offset+=13;
        check(size<=data.size()-offset,"short record data");
        size_t start=offset;offset+=size;std::string source;bool vertex=true;
        if(kind=='K') {
            check(size==256,"pipeline ABI changed");
            pipelines.emplace_back(at<uint64_t>(data,start),at<uint64_t>(data,start+8));
        } else if(kind=='W') {
            check(size==19,"reference ABI changed");
            auto regs=at<uint16_t>(data,start);auto decode=at<uint64_t>(data,start+2);
            auto pos=at<uint32_t>(data,start+11),words=at<uint32_t>(data,start+15);
            check(words>0 && words<=544,"invalid vertex program length");
            if(!xbe.empty()) {
                check(pos<=xbe.size() && size_t(words)*4<=xbe.size()-pos,"reference outside executable");
                std::vector<uint32_t> program(words);std::memcpy(program.data(),xbe.data()+pos,words*4);
                uint64_t actual=Hash(&decode,8,(Hash(program.data(),words*4)|1)^regs);
                if(data[start+10]) actual^=0xC3C30000;
                check(actual==key,"vertex reference hash mismatch: wrong executable or recipe");
                source=VertexProgramHlsl(program,regs,decode);
                if(data[start+10]) source=QuadHlsl(std::move(source));
                refs++;
            }
            check(vertices.insert(key).second,"duplicate vertex recipe");
        } else if(kind=='F') {
            check(size==39,"fixed-function ABI changed");
            FixedKey fixed=at<FixedKey>(data,start+11);bool pretransformed=data[start+2]!=0;
            source=FixedVertexHlsl(at<uint16_t>(data,start),pretransformed,at<uint64_t>(data,start+3),fixed);
            if(pretransformed) source=QuadHlsl(std::move(source));
            check(vertices.insert(key).second,"duplicate vertex recipe");
        } else if(kind=='P') {
            check(size==sizeof(PixelKey),"pixel ABI changed");
            source=PixelHlsl(at<PixelKey>(data,start));vertex=false;
            check(pixels.insert(key).second,"duplicate pixel recipe");
        } else throw std::runtime_error("seed contains raw program or unsupported record");
        counts[kind]++;
        if(kind!='K' && (kind!='W' || !xbe.empty())) {
            check(!source.empty() && source.find("main(")!=std::string::npos,"empty or invalid generated shader");
#ifdef NGB_SEED_COMPILE
            std::vector<uint32_t> spirv;
            check(vkr::Compile(source,vertex,spirv),"production compiler rejected a seed shader");
            check(spirv.size()>5 && spirv[0]==0x07230203,"invalid compiled SPIR-V");
#endif
            if(argc==4) {
                std::filesystem::create_directories(argv[3]);
                auto name=std::filesystem::path(argv[3])/(std::to_string(emitted)+(vertex?".vert.hlsl":".frag.hlsl"));
                std::ofstream file(name);file<<source;check(bool(file),"cannot write scratch shader");
            }
            emitted++;
        }
    }
    for(auto [vs,ps]:pipelines) check(vertices.count(vs) && pixels.count(ps),"pipeline lacks recreatable shaders");
    std::cout<<"Seed validated: "<<vertices.size()<<" vertex, "<<pixels.size()<<" pixel, "<<pipelines.size()
             <<" pipelines; "<<refs<<" executable references verified; "<<emitted<<" production shaders reconstructed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
