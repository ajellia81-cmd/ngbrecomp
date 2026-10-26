// No game inputs, GPU, desktop window or saves. Exercise the actual retained
// pose and generated shader code; emitted fixtures are compiled by glslang in CI.
#include "gpu/motion.h"
#include "gpu/upload_ring.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void check(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
int main(int argc,char **argv) {
    using namespace gpu;
    // Reproduce the old full-allocation guard: 31 MiB + a 2 MiB draw is
    // inside 64 MiB, but Upload() wraps at the 32 MiB frame-slot boundary.
    check(kUploadRingBytes==2*kUploadSlotBytes,"upload slots disagree with allocation");
    check(UploadRingFits(0,kUploadSlotBytes),"exactly one slot must fit");
    check(UploadRingFits(kUploadSlotBytes-4096,4096),"exact tail must fit");
    check(!UploadRingFits(31u<<20,2u<<20),"draw would cross into a new uniform generation");
    check(!UploadRingFits(0,kUploadSlotBytes+1),"oversized draw must not overwrite next frame");
    check(!UploadRingFits(UINT64_MAX,1),"overflow must not make reservation fit");
    State source; DrawConstants constants{};
    MotionHistory history;
    source.vsc[96]={1,0,0,0.1f};
    DrawConstants midpoint{};
    auto vertices=source.vsc;
    history.Add(7,MotionPose::Capture(source,constants),midpoint,&vertices);
    check(history.Resolve().matched==0,"first frame cannot interpolate");
    // Mutation after capture cannot change the retained previous frame.
    source.vsc[96][3]=0.3f; history.Finish(); vertices=source.vsc;
    history.Add(7,MotionPose::Capture(source,constants),midpoint,&vertices);
    check(history.Resolve().changed==1,"moved geometry must pair");
    check(std::abs(vertices[96][3]-0.2f)<1e-6f,"history aliases mutable current constants");
    check(midpoint.motion[0]==0.0f,"midpoint must use the original vertex program");
    history.Add(8,MotionPose::Capture(source,constants),midpoint,&vertices);
    check(history.Resolve().missing==1,"new topology must not pair");
    history.Finish(); history.Reset();
    history.Add(7,MotionPose::Capture(source,constants),midpoint,&vertices);
    check(history.Resolve().matched==0,"lifecycle/cut reset retained history");
    // Repeated meshes change draw order as the camera culls room pieces.
    // The old occurrence matching pulls both pieces to the middle of the room.
    MotionHistory room;
    State left,right;left.vsc[96][3]=10;right.vsc[96][3]=20;
    DrawConstants lc{},rc{};auto lv=left.vsc,rv=right.vsc;
    room.Add(99,MotionPose::Capture(left,lc),lc,&lv);
    room.Add(99,MotionPose::Capture(right,rc),rc,&rv);room.Finish();
    right.vsc[96][3]=20.2f;left.vsc[96][3]=10.2f;
    rv=right.vsc;lv=left.vsc;
    room.Add(99,MotionPose::Capture(right,rc),rc,&rv);
    room.Add(99,MotionPose::Capture(left,lc),lc,&lv);
    auto paired=room.Resolve();
    check(paired.matched==2 && paired.reordered==2,"reordered meshes paired by occurrence");
    check(std::abs(rv[96][3]-20.1f)<1e-5f && std::abs(lv[96][3]-10.1f)<1e-5f,"room pieces moved to unrelated poses");
    room.Resolve();check(std::abs(rv[96][3]-20.1f)<1e-5f,"Resolve averaged a midpoint twice");
    room.Finish();
    left.vsc[96][3]=10.4f;lv=left.vsc;
    room.Add(99,MotionPose::Capture(left,lc),lc,&lv);
    check(room.Resolve().matched==1 && std::abs(lv[96][3]-10.3f)<1e-5f,"culling paired the remaining mesh with its neighbour");
    MotionHistory global;
    State zero,two,near_one,near_two;
    two.vsc[96][3]=.2f;near_one.vsc[96][3]=.11f;near_two.vsc[96][3]=.21f;
    DrawConstants a0{},a1{},b0{},b1{};
    auto z=zero.vsc,t=two.vsc,n1=near_one.vsc,n2=near_two.vsc;
    global.Add(88,MotionPose::Capture(zero,a0),a0,&z);
    global.Add(88,MotionPose::Capture(two,a1),a1,&t);global.Finish();
    global.Add(88,MotionPose::Capture(near_one,b0),b0,&n1);
    global.Add(88,MotionPose::Capture(near_two,b1),b1,&n2);global.Resolve();
    check(std::abs(n1[96][3]-.055f)<1e-5f && std::abs(n2[96][3]-.205f)<1e-5f,"first draw stole a closer pose from another draw");
    // A new visible lamp can replace an old one even with a singleton key.
    // Nearest-only matching fabricated a glow halfway across the room.
    MotionHistory replaced;
    State lamp;lamp.vsc[96][3]=10;DrawConstants lamp_dc{};auto lamp_vc=lamp.vsc;
    replaced.Add(77,MotionPose::Capture(lamp,lamp_dc),lamp_dc,&lamp_vc);replaced.Finish();
    lamp.vsc[96][3]=20;lamp_vc=lamp.vsc;
    replaced.Add(77,MotionPose::Capture(lamp,lamp_dc),lamp_dc,&lamp_vc);
    auto rejected=replaced.Resolve();
    check(rejected.matched==0 && rejected.discontinuous==1 && lamp_vc[96][3]==20,"disappeared lamp was blended with a different visible instance");
    // Normal small input motion still yields a distinct midpoint.
    replaced.Finish();lamp.vsc[96][3]=20.2f;lamp_vc=lamp.vsc;
    replaced.Add(77,MotionPose::Capture(lamp,lamp_dc),lamp_dc,&lamp_vc);
    check(replaced.Resolve().matched==1 && std::abs(lamp_vc[96][3]-20.1f)<1e-5f,"normal small motion lost interpolation");
    // Stale constants not read by this shader must not reject a stable draw.
    State masked;masked.vs=std::make_shared<VertexShader>();
    masked.vs->program={0,(1u<<21)|(5u<<13),(3u<<26),(15u<<12)|(1u<<11)|1u};
    auto used=VertexProgramConstants(masked.vs->program);
    check(used[5] && !used[170],"program constant use disagrees with MOV operand");
    check(VertexProgramHlsl(masked.vs->program,1).find("c[5]")!=std::string::npos,"fixture shader does not use the claimed constant");
    auto first=MotionPose::Capture(masked,constants);
    masked.vsc[170][0]=1000;
    auto second=MotionPose::Capture(masked,constants);
    check(first.RelativeStep(second)==0,"unused constant rejected a stable pose");
    masked.vsc[5][0]=1;
    check(first.RelativeStep(MotionPose::Capture(masked,constants))>.25f,"active input discontinuity was ignored");
    masked.vs->program[3]|=2;
    check(VertexProgramConstants(masked.vs->program)[170],"relative addressing omitted possible input constants");
    auto fixed=MotionPose::Capture(masked,constants,false);
    masked.vsc[5][0]=2000;
    check(fixed.RelativeStep(MotionPose::Capture(masked,constants,false))==0,"fixed function used irrelevant vertex program constants");
    // Same byte storage and relative indices can select different submeshes.
    Assembled range;State target;
    range.index_count=3;range.index16=true;
    auto key=MotionRangeKey(123,range,target);
    range.base_vertex=3;
    check(MotionRangeKey(123,range,target)!=key,"different absolute indexed vertices share a motion key");
    range.base_vertex=0;range.draw_count=6;
    check(MotionRangeKey(123,range,target)!=key,"different unindexed lengths share a motion key");
    range.draw_count=0;target.rt.data=0x8000;
    check(MotionRangeKey(123,range,target)==key,"rotating swap buffers lost adjacent history");
    target.rt.format=0x100;
    check(MotionRangeKey(123,range,target)!=key,"incompatible render formats share a motion key");
    // Coplanar passes must receive identical midpoint inputs; a different
    // material does not add independent clip-position arithmetic afterwards.
    MotionPose prior=MotionPose::Capture(source,constants);
    prior.wvp[0]=1; constants.wvp[0]=3;
    auto pass_a=source.vsc,pass_b=source.vsc;
    DrawConstants depth=constants,color=constants;
    check(prior.Midpoint(pass_a,depth) && prior.Midpoint(pass_b,color),"valid shared pose rejected");
    check(depth.wvp[0]==2 && !std::memcmp(depth.wvp,color.wvp,sizeof depth.wvp),"multipass inputs differ");
    check(constants.wvp[0]==3,"authoritative endpoint mutated");
    pass_b[0][0]=INFINITY; auto unchanged=pass_b;
    check(!prior.Midpoint(pass_b,color),"non-finite pose accepted");
    check(!std::memcmp(pass_b.data(),unchanged.data(),sizeof pass_b),"invalid pose partially blended");
        // Newly rasterized midpoint geometry, not a copy of an endpoint. Four output
    // aspects must retain their distinct projected positions and game time.
    for (float aspect : {4.f/3,16.f/9,16.f/10,64.f/27}) {
        float old_x=0.1f/aspect,now_x=0.3f/aspect,mid_x=(old_x+now_x)*0.5f;
        check(mid_x!=old_x && mid_x!=now_x,"intermediate scene repeats an endpoint");
    }
    if (argc==2) {
        auto emit=[&](const char *name,std::string shader) {
            auto wrapped=MotionVertexHlsl(shader);
            check(!wrapped.empty() && wrapped.find("motion_previous")==std::string::npos,"clip-space interpolation unexpectedly retained");
            check(wrapped==shader,"authoritative position program changed");
            std::ofstream(std::string(argv[1])+"/"+name)<<wrapped;
        };
        emit("motion-fixed.vert.hlsl",FixedVertexHlsl(1,false));
        std::vector<uint32_t> program;
        for (unsigned row=0;row<4;row++) {
            program.push_back(0);
            program.push_back((7u<<21)|((96+row)<<13)|0x1B);
            program.push_back((2u<<26)|(1u<<21)|(2u<<19)|(3u<<17)|(3u<<11));
            program.push_back(((8u>>row)<<12)|(1u<<11)|(row==3));
        }
        emit("motion-program.vert.hlsl",VertexProgramHlsl(program,1));
        check(MotionVertexHlsl("no vertex entry").empty(),"unsupported entry silently accepted");
    }
    std::cout<<"Smooth Motion: 32 MiB slot boundaries, owned history, changed geometry, topology/reset rejection and fixed/program shader generation: OK\n";
}
