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
    check(!history.Add(7,MotionPose::Capture(source,constants),midpoint),"first frame cannot interpolate");
    // Mutation after capture cannot change the retained previous frame.
    source.vsc[96][3]=0.3f; history.Finish();
    check(history.Add(7,MotionPose::Capture(source,constants),midpoint),"moved geometry must pair");
    check(std::abs(midpoint.motion_vc[96][3]-0.1f)<1e-6f,"history aliases mutable current constants");
    check(midpoint.motion[0]==0.5f,"midpoint is an endpoint");
    check(!history.Add(8,MotionPose::Capture(source,constants),midpoint),"new topology must not pair");
    history.Finish(); history.Reset();
    check(!history.Add(7,MotionPose::Capture(source,constants),midpoint),"lifecycle/cut reset retained history");
    // Newly rasterized midpoint geometry, not a copy of an endpoint. Four output
    // aspects must retain their distinct projected positions and game time.
    for (float aspect : {4.f/3,16.f/9,16.f/10,64.f/27}) {
        float old_x=0.1f/aspect,now_x=0.3f/aspect,mid_x=(old_x+now_x)*0.5f;
        check(mid_x!=old_x && mid_x!=now_x,"intermediate scene repeats an endpoint");
    }
    if (argc==2) {
        auto emit=[&](const char *name,std::string shader) {
            auto wrapped=MotionVertexHlsl(std::move(shader));
            check(!wrapped.empty() && wrapped.find("motion_previous")!=std::string::npos,"missing previous shader evaluation");
            check(wrapped.find("n.pos=MotionClipPosition(p.pos,n.pos,motion.x)")!=std::string::npos,"missing raster geometry interpolation");
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
