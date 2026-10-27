"""Replay the production prewarmer against a fake GPU and synthetic recipes."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
backend=(root/'src/d3d/vk/backend.cpp').read_text()
body=backend.split('void VulkanBackend::Prewarm() {',1)[1].split('\n// Every pipeline made so far',1)[0]
recipe_check=backend.split('    bool vnew;',1)[1].split('    if (vnew &&',1)[0]
source=r'''
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
using VkShaderModule=uint64_t;
using VkPipeline=uint64_t;
constexpr uint64_t VK_NULL_HANDLE=0,tag=0x5646335343454E45ull;
static const char kCacheMagic[8]={'T','E','S','T','C','A','C','H'};
struct FixedKey { uint32_t value; };
struct PixelKey { uint32_t value; };
struct PipeKey { uint64_t vs,ps;uint32_t samples;bool operator==(const PipeKey& b)const{return vs==b.vs&&ps==b.ps&&samples==b.samples;} };
struct PipeHash { size_t operator()(const PipeKey& k)const{return k.vs^k.ps^k.samples;} };
static std::map<std::string,std::vector<uint8_t>> files;
static std::map<std::string,unsigned> compiled;
static unsigned built,recorded;
static std::vector<uint8_t> ReadFile(const std::string& p){return files[p];}
static uint64_t Hash(const void*,size_t,uint64_t=0){return 42;}
static std::string VertexProgramHlsl(const std::vector<uint32_t>&,uint16_t,uint64_t){return "program";}
static std::string FixedVertexHlsl(uint16_t,bool pre,uint64_t,const FixedKey& k){return "fixed"+std::to_string(k.value)+(pre?"-screen":"-model");}
static std::string PixelHlsl(const PixelKey&){return "pixel";}
static std::string QuadHlsl(std::string s){return s+"-quad";}
static std::string MotionVertexHlsl(std::string s){return s+"-motion";}
static void nv2a_quality_get(int*,int*,int*,int* ms){*ms=1;}
static int ngb_exe_path(char*,size_t){return 0;}
struct FakeDispatch { void vkDestroyShaderModule(int,VkShaderModule,void*){};void vkDestroyPipeline(int,VkPipeline,void*){}; };
struct VulkanBackend {
 std::recursive_mutex cache_mu_;
 std::unordered_map<uint64_t,VkShaderModule> modules_;
 std::unordered_map<PipeKey,VkPipeline,PipeHash> pipelines_;
 std::unordered_map<PipeKey,bool,PipeHash> known_pipes_;
 std::string cache_dir_="cache";
 bool stop_=false;int dev_=0;FILE* recipes_=nullptr;
 uint32_t ms_counts_=1,warm_want_=0,prewarm_done_=0;
 FakeDispatch d_;
 bool Compile(const std::string& s,bool,std::vector<uint32_t>& out){compiled[s]++;out={uint32_t(compiled.size())};return true;}
 VkShaderModule MakeModule(const std::vector<uint32_t>& s){return s[0]+1;}
 VkPipeline MakePipeline(const PipeKey&,VkShaderModule v,VkShaderModule p){assert(v&&p);return ++built;}
 void CacheRecord(char,uint64_t,const void*,size_t,FILE*){recorded++;}
 void WarmSamples(uint32_t){}
 void Prewarm();
 bool RecipeMissing(uint64_t vkey,bool motion_shader);
};
'''
source+='bool VulkanBackend::RecipeMissing(uint64_t vkey,bool motion_shader) { bool vnew;'+recipe_check+'return vnew; }\n'
source+='void VulkanBackend::Prewarm() {'+body+r'''
static void append(char kind,uint64_t key,const std::vector<uint8_t>& data){
 auto& b=files["seed"];uint32_t n=data.size();b.push_back(kind);
 auto p=reinterpret_cast<uint8_t*>(&key);b.insert(b.end(),p,p+8);
 p=reinterpret_cast<uint8_t*>(&n);b.insert(b.end(),p,p+4);b.insert(b.end(),data.begin(),data.end());
}
template<class T>static std::vector<uint8_t> bytes(T value){auto p=reinterpret_cast<uint8_t*>(&value);return {p,p+sizeof value};}
static void fixed(uint64_t key,bool screen){
 std::vector<uint8_t> b(11+sizeof(FixedKey));b[2]=screen;FixedKey k{uint32_t(key)};memcpy(b.data()+11,&k,sizeof k);append('F',key,b);
}
int main(){
 setenv("NGB_D3D_SEED","seed",1);files["seed"]={kCacheMagic,kCacheMagic+8};
 fixed(100,false);fixed(101,true);append('P',200,bytes(PixelKey{1}));
 std::vector<uint8_t> program(19);uint32_t n=1;memcpy(program.data()+11,&n,4);append('V',42,program);
 append('K',0,bytes(PipeKey{100,200,1}));append('K',0,bytes(PipeKey{100^tag,200,1}));
 append('K',0,bytes(PipeKey{101,200,1}));append('K',0,bytes(PipeKey{42,200,1}));
 VulkanBackend b;b.modules_[100]=77;b.Prewarm();
#ifdef NGB_SMOOTH_MOTION
 assert(b.modules_.count(100^tag)==1); // warm motion even if the base is already cached
 assert(b.modules_.count(101^tag)==0); // never transform screen-space shaders
 assert(b.modules_.count(200^tag)==0); // never transform pixel shaders
 assert(b.modules_.count(42^tag)==1);
 assert(compiled["fixed100-model"]==0 && compiled["fixed100-model-motion"]==1);
 assert(compiled["program"]==1 && compiled["program-motion"]==1);
 assert(b.pipelines_.size()==5 && built==5); // duplicated base/motion recipes build once
 assert(b.prewarm_done_==1);
 unsigned before=compiled.size();b.Prewarm();assert(compiled.size()==before && built==5);
 for(auto& [key,count]:compiled)if(key!="fixed100-model")assert(count==1);
 b.modules_[300^tag]=99;
 assert(!b.modules_.count(300));
 assert(!b.RecipeMissing(300,true)); // cached motion draw must not rewrite the recipe every frame
 assert(b.RecipeMissing(300,false));
#else
 assert(!b.modules_.count(100^tag) && !b.modules_.count(42^tag));
 assert(b.pipelines_.size()==3 && built==3 && b.prewarm_done_==1);
 b.Prewarm();assert(built==3);
#endif
}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/'warm.cpp').write_text(source)
    for smooth in [False,True]:
        cmd=[os.environ.get('CXX','c++'),'-std=c++17',str(p/'warm.cpp'),'-o',str(p/'warm')]
        if smooth:cmd.insert(1,'-DNGB_SMOOTH_MOTION=1')
        subprocess.run(cmd,check=True)
        subprocess.run([str(p/'warm')],check=True)
print('Production prewarm: base/motion modules and pipelines, existing base cache, screen exclusion and duplicate recipes PASS')
