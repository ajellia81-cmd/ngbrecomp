// No game inputs, GPU, window or saves. Test the actual NGB scene bridge and
// unchanged VF3 interpolation; emit the production shader wrappers for CI.
#include "gpu/scene_motion.h"
#include "gpu/upload_ring.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace gpu;
using Mat=video::Mat4;
static void check(bool v,const char* why) {if(!v) throw std::runtime_error(why);}
static void near(float a,float b,const char* why) {if(std::fabs(a-b)>.003f) {std::cerr<<a<<" != "<<b<<'\n';throw std::runtime_error(why);}}
struct Source {unsigned mux=1,reg=0;std::array<unsigned,4> sw{0,1,2,3};};
static void instruction(std::vector<uint32_t>& code,unsigned mac,unsigned ilu,unsigned constant,unsigned dest,unsigned mask,
                        Source a={},Source b={},Source c={},unsigned imask=0,unsigned outmask=0,bool output_ilu=false,bool relative=false) {
    uint32_t w1=(mac<<21)|(ilu<<25)|(constant<<13);
    for(unsigned i=0;i<4;i++) w1|=a.sw[i]<<(6-i*2);
    uint32_t w2=(a.mux<<26)|(a.reg<<28)|(b.mux<<11)|(b.reg<<13);
    for(unsigned i=0;i<4;i++) {w2|=b.sw[i]<<(23-i*2);w2|=c.sw[i]<<(8-i*2);}
    w2|=c.reg>>2;
    uint32_t w3=(c.mux<<28)|((c.reg&3)<<30)|(dest<<20)|(mask<<24)|(imask<<16)|(outmask<<12);
    if(outmask) w3|=1u<<11;
    if(output_ilu) w3|=4;
    if(relative) w3|=2;
    code.insert(code.end(),{0,w1,w2,w3});
}
static std::vector<uint32_t> position_program() {
    std::vector<uint32_t> code;
    for(unsigned i=0;i<4;i++) instruction(code,7,0,96+i,0,8u>>i,{2},{3});
    instruction(code,0,2,0,1,0,{},{},{1,0,{3,3,3,3}},15);
    instruction(code,2,0,0,0,14,{1,0},{1,1});
    instruction(code,2,0,58,0,14,{1,0},{3});
    instruction(code,3,0,59,0,14,{1,0},{},{3});
    instruction(code,1,0,0,0,0,{1,0},{},{},0,15);
    code.back()|=1;
    return code;
}
static void constants(State& s,unsigned base,const Mat& m) {
    for(unsigned r=0;r<4;r++) for(unsigned c=0;c<4;c++) s.vsc[base+r][c]=m.m[c*4+r];
}
static Mat projection() {Mat m{};m.m[0]=1;m.m[5]=1;m.m[10]=1000.f/999;m.m[11]=1;m.m[14]=-1000.f/999;return m;}
static State state(const Mat& model,float camera=0) {
    State s;s.vs_handle=1;s.vs=std::make_shared<VertexShader>();s.vs->program=position_program();
    auto p=projection();auto view=Mat::identity();view.m[12]=camera;
    std::copy(p.m,p.m+16,s.scene_projection.begin());std::copy(view.m,view.m+16,s.scene_view.begin());s.scene_camera_valid=true;
    s.vsc[58]={320,-240,16777215,0};s.vsc[59]={320,240,0,0};
    constants(s,96,p*view*model);constants(s,100,view*model);
    return s;
}
static Assembled geometry(float* data,unsigned count=3) {
    Assembled a;a.regs=1;a.vertex_count=count;a.draw_count=count;a.buffer_count=1;
    a.buffers[0]={reinterpret_cast<uint8_t*>(data),count*16,16};a.elements[0]={2,0,0};return a;
}
int main(int argc,char** argv) {
    check(UploadRingFits(0,kUploadSlotBytes),"full upload slot rejected");
    check(!UploadRingFits(31u<<20,2u<<20),"draw crosses retained frame allocation");
    Mat model=Mat::identity();model.m[12]=10;model.m[14]=200;
    State s=state(model);DrawConstants dc{};float raw[]={0,0,0,1, 1,0,0,1, 0,1,0,1};
    auto a=geometry(raw);ScenePosition p{};
    check(SceneVertexPosition(s,a,0,dc,p),"actual vertex program failed");near(p[0],336,"viewport X differs");near(p[1],240,"viewport Y differs");near(p[3],200,"clip W differs");
    // Relative addressing and paired MAC/ILU both read pre-instruction state.
    State relative=s;relative.vs=std::make_shared<VertexShader>();auto& code=relative.vs->program;
    instruction(code,13,0,10,0,0,{3}); // a0 = c10.x = 2
    instruction(code,1,0,20,0,0,{3},{},{},0,15,false,true);code.back()|=1;
    relative.vsc[10]={2,0,0,0};relative.vsc[22]={7,8,9,10};
    check(SceneVertexPosition(relative,a,0,dc,p),"relative position program rejected");near(p[0],7,"bone address was not retained");near(p[3],10,"relative constant W differs");
    SceneMotion history;std::vector<SceneOutput> before(10),after(11);
    float meshes[10][12]{};
    for(unsigned i=0;i<10;i++) {std::copy(raw,raw+12,meshes[i]);if(i<8) meshes[i][4]=1+float(i)*.1f;}
    auto add=[&](unsigned i,float camera,SceneOutput& output) {
        auto m=Mat::identity();m.m[12]=i<8?float(i*4):i==8?-20:20;m.m[14]=i<8?200+float(i*10):30;
        auto st=state(m,camera);auto draw=geometry(meshes[i]);
        check(history.Add(st,draw,dc,output),"valid model was not imported");
    };
    for(unsigned i=0;i<10;i++) add(i,0,before[i]);
    check(history.Resolve().changed==0,"first scene invented previous geometry");history.Finish();
    for(unsigned i=0;i<8;i++) add(i,-1,after[i]);
    add(9,-1,after[8]);add(8,-1,after[9]);add(9,-1,after[10]);
    auto stats=history.Resolve();
    check(stats.rigid==11,"rigid position program was not proved algebraically");
    check(stats.cached>=10,"immutable static meshes were rebuilt between frames");
    check(stats.vf.blended==0,"static meshes incorrectly became deformed meshes");
    check(stats.shared>=1,"multipass model not shared");check(stats.vf.matched>=10,"VF3 did not pair the scene");
    check(!after[8].positions.empty() && !after[9].positions.empty(),"repeated instances lost midpoint");
    near(after[8].positions[0][0],320+320*19.5f/30,"right lamp moved to a different lamp");
    near(after[9].positions[0][0],320+320*-20.5f/30,"left lamp moved to a different lamp");
    check(after[8].positions==after[10].positions,"EQUAL-depth material passes receive different geometry");
    near(s.vsc[96][3],10,"endpoint shader constants were mutated");
    history.Finish();history.Reset();SceneOutput reset;
    add(9,-2,reset);check(history.Resolve().changed==0,"reset reused an old scene");
    // RCC's magnitude clamp has the same algebraic path only while the
    // perspective W is inside its finite domain.
    auto rcc=state(model);rcc.vs->program[4*4+1]&=~(7u<<25);rcc.vs->program[4*4+1]|=3u<<25;
    SceneMotion rcc_scene;SceneOutput rcc_output;
    check(rcc_scene.Add(rcc,a,dc,rcc_output),"RCC program was not imported");
    check(rcc_scene.Resolve().rigid==1,"finite-domain RCC was not proved affine");
    // A nonlinear position program must use the original evaluator. Failed
    // symbolic expressions must never turn into a fabricated affine matrix.
    auto nonlinear=state(model);nonlinear.vs=std::make_shared<VertexShader>();
    instruction(nonlinear.vs->program,2,0,0,2,15,{2},{2});
    auto nonlinear_body=position_program();
    for(unsigned i=0;i<4;i++) {nonlinear_body[i*4+2]&=~(3u<<26);nonlinear_body[i*4+2]|=(1u<<26)|(2u<<28);}
    nonlinear.vs->program.insert(nonlinear.vs->program.end(),nonlinear_body.begin(),nonlinear_body.end());
    SceneMotion nonlinear_scene;SceneOutput nonlinear_output;
    check(nonlinear_scene.Add(nonlinear,a,dc,nonlinear_output),"nonlinear program fallback failed");
    check(nonlinear_scene.Resolve().rigid==0,"nonlinear program was mistaken for a rigid matrix");
    // Raw SHORT3 decode and base-vertex triangle strip indexing are imported.
    int16_t short_data[12]={0,0,0,0, 1,0,0,0, 0,1,0,0};
    auto packed=a;packed.buffers[0]={reinterpret_cast<const uint8_t*>(short_data),sizeof short_data,8};
    packed.elements[0].format=14;packed.decode=kDecodeIntW1;
    check(SceneVertexPosition(s,packed,1,dc,p),"SHORT3 input decode failed");near(p[0],337.6f,"SHORT3 position changed");
    std::string fixed=MotionVertexHlsl(FixedVertexHlsl(1,false));
    std::string program=MotionVertexHlsl(VertexProgramHlsl(position_program(),1));
    std::string decoded=MotionVertexHlsl(VertexProgramHlsl(position_program(),1,kDecodeIntW1));
    check(fixed.find("scene_positions[scene_vertex]")!=std::string::npos,"fixed shader not connected to scene output");
    check(program.find("scene_positions[scene_vertex]")!=std::string::npos,"program shader not connected to scene output");
    check(decoded.find("VOut main(VRaw ri, uint scene_vertex")!=std::string::npos,"decoded vertex entry not wrapped");
    auto shader_file=[](const std::string& path,std::string source) {
        for(size_t at=source.find("1.#INF");at!=std::string::npos;at=source.find("1.#INF")) source.replace(at,6,"asfloat(0x7F800000)");
        std::ofstream(path)<<source;
    };
    if(argc>1) {
        shader_file(std::string(argv[1])+"/motion-fixed.vert.hlsl",fixed);
        shader_file(std::string(argv[1])+"/motion-program.vert.hlsl",program);
        shader_file(std::string(argv[1])+"/motion-decoded.vert.hlsl",decoded);
    }
    std::cout<<"Unchanged VF3 core with NGB scene extraction: camera, repeated instances, shared depth passes, bone addressing, raw formats, reset and shader wrappers passed.\n";
    return 0;
}
