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
static void draw_matrix(float* out,const Mat& m) {
    for(unsigned row=0;row<4;row++) for(unsigned col=0;col<4;col++) out[row*4+col]=m.m[col*4+row];
}
static void input_cache_tests() {
    float positions[]={0,0,0,1, 1,0,0,1, 0,1,0,1};
    float uv[]={0,0, 1,0, 0,1};uint16_t indices[]={0,1,2};
    auto a=geometry(positions);a.regs|=1<<9;a.buffer_count=2;
    a.elements[9]={16,1,0};a.buffers[1]={reinterpret_cast<const uint8_t*>(uv),sizeof uv,8};
    a.index_src=indices;a.index16=true;a.index_count=3;
    auto model=Mat::identity();model.m[14]=50;
    auto s=state(model);s.vs_handle=0;s.vs.reset();s.rs[kRsLighting]=1;
    DrawConstants dc{};draw_matrix(dc.wv,model);draw_matrix(dc.wvp,projection()*model);
    SceneMotion history;SceneOutput output;
    auto add=[&](unsigned hits) {
        check(history.Add(s,a,dc,output),"cache fixture rejected");
        check(history.Resolve().input_cached==hits,"input cache reused changed bytes or missed identical bytes");
        history.Reset();
    };
    add(0);add(1);
    float relocated[12];std::copy(positions,positions+12,relocated);
    a.buffers[0].src=reinterpret_cast<const uint8_t*>(relocated);add(1);
    relocated[4]=2;add(0);add(1); // same address, changed geometry
    uv[0]=.5f;add(0);add(1); // material coordinates are part of VF3 matching
    std::swap(indices[0],indices[1]);add(0);add(1);
    a.decode=kDecodeW1;add(0);add(1);
    dc.wv[3]=10;add(1); // transforms do not invalidate immutable local geometry
}
static void fixed_scene_tests() {
    DrawConstants dc{};
    auto fixed_state=[](const Mat& world,float camera,DrawConstants& constants) {
        auto s=state(world,camera);s.vs_handle=0;s.vs.reset();s.rs[kRsLighting]=1;
        auto view=Mat::identity();view.m[12]=camera;
        draw_matrix(constants.wv,view*world);draw_matrix(constants.wvp,projection()*view*world);
        return s;
    };
    float vertices[10][12]{};
    for(unsigned i=0;i<10;i++) {
        float mesh[]={0,0,0,7, 1,0,0,8, 0,1,0,9};
        std::copy(mesh,mesh+12,vertices[i]);if(i<8) vertices[i][4]=1+float(i)*.1f;
    }
    SceneMotion history;std::vector<SceneOutput> first(10),second(11);
    auto add=[&](unsigned i,float camera,SceneOutput& output) {
        auto world=Mat::identity();world.m[12]=i<8?float(i*4):i==8?-20:20;world.m[14]=i<8?200+float(i*10):30;
        auto s=fixed_state(world,camera,dc);auto a=geometry(vertices[i]);
        check(history.Add(s,a,dc,output),"fixed terrain not imported");
    };
    for(unsigned i=0;i<10;i++) add(i,0,first[i]);
    check(history.Resolve().fixed_rigid==10,"fixed terrain skipped rigid path");history.Finish();
    for(unsigned i=0;i<8;i++) add(i,-1,second[i]);
    add(9,-1,second[8]);add(8,-1,second[9]);add(9,-1,second[10]);
    auto stats=history.Resolve();
    check(stats.fixed_rigid==11 && stats.evaluated_vertices==0,"rigid terrain still interpreted per vertex");
    check(stats.cached>=10 && stats.vf.blended==0,"static fixed terrain rebuilt/deformed");
    check(stats.vf.matched==10,"fixed scene pairing differs");
    check(!second[8].positions.empty(),"fixed midpoint missing");
    near(second[8].positions[0][0],19.5f,"fixed lamp X differs");
    near(second[8].positions[0][3],30,"fixed clip W differs");
    near(second[9].positions[0][0],-20.5f,"fixed reordered lamp X differs");
    check(second[8].positions==second[10].positions,"fixed depth/material passes diverged");
    check(vertices[9][3]==7,"fixed point-W conversion modified source vertices");
    // All six fixed-function blend modes, including explicit sums != 1.
    // Expected projected positions are computed from endpoint vertex outputs,
    // independently of the adapter's optimized model-space bone calculation.
    for(unsigned mode=1;mode<=6;mode++) {
        SceneMotion skin;std::vector<SceneOutput> outputs(9);
        ScenePosition old_positions[3],new_positions[3];
        float skinned[]={0,0,0,1, .2f,.3f,.1f,.5f,
                         2.3f,0,0,1, .3f,.3f,.1f,.5f,
                         0,1,0,1, .4f,.3f,.1f,.5f};
        Assembled a;a.regs=3;a.vertex_count=3;a.draw_count=3;a.buffer_count=1;
        a.buffers[0]={reinterpret_cast<uint8_t*>(skinned),sizeof skinned,32};
        a.elements[0]={2,0,0};a.elements[1]={2,0,16};
        for(unsigned frame=0;frame<2;frame++) {
            for(unsigned i=0;i<8;i++) {
                auto world=Mat::identity();world.m[12]=float(i*4);world.m[14]=300+float(i*10);
                auto s=fixed_state(world,0,dc);auto g=geometry(vertices[i]);
                check(skin.Add(s,g,dc,outputs[i]),"skin background not imported");
            }
            auto world=Mat::identity();auto s=fixed_state(world,0,dc);s.rs[kRsVertexBlend]=mode;
            draw_matrix(dc.pv,projection());
            for(unsigned bone=0;bone<4;bone++) {
                auto matrix=Mat::identity();matrix.m[12]=float(bone*2)+float(frame)*(.25f+float(bone)*.1f);matrix.m[14]=200;
                draw_matrix(dc.wvb[bone],matrix);
            }
            for(unsigned i=0;i<3;i++) check(SceneVertexPosition(s,a,i,dc,frame?new_positions[i]:old_positions[i]),"bone endpoint evaluation failed");
            check(skin.Add(s,a,dc,outputs[8]),"fixed skinned mesh not imported");
            auto result=skin.Resolve();
            check(result.fixed_skin==1 && result.evaluated_vertices==0,"fixed bones still repeat general evaluator");
            if(frame==0) skin.Finish();
        }
        check(outputs[8].positions.size()==3,"fixed skin midpoint missing");
        for(unsigned i=0;i<3;i++) for(unsigned component=0;component<4;component++) {
            float old=component==3?old_positions[i][3]:old_positions[i][component]*old_positions[i][3];
            float now=component==3?new_positions[i][3]:new_positions[i][component]*new_positions[i][3];
            near(outputs[8].positions[i][component],(old+now)*.5f,"optimized fixed skin midpoint differs from endpoints");
        }
    }
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
    fixed_scene_tests();
    input_cache_tests();
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
