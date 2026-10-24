#pragma once
#include "gpu/shaders.h"
#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gpu {
#ifdef NGB_SMOOTH_MOTION
// History has no guest pointers or GPU allocations. Geometry/material keys are
// matched in occurrence order; changed topology starts a new history entry.
struct MotionPose {
    std::array<std::array<float,4>,192> vc{};
    float wvp[16]{}, wv[16]{}, wvn[16]{}, wvb[4][16]{}, wvbn[4][16]{}, pv[16]{};
    static MotionPose Capture(const State& s,const DrawConstants& c) {
        MotionPose p; p.vc=s.vsc;
#define COPY_MATRIX(x) std::memcpy(p.x,c.x,sizeof p.x)
        COPY_MATRIX(wvp); COPY_MATRIX(wv); COPY_MATRIX(wvn); COPY_MATRIX(wvb); COPY_MATRIX(wvbn); COPY_MATRIX(pv);
#undef COPY_MATRIX
        return p;
    }
    void Previous(DrawConstants& c) const {
        std::memcpy(c.motion_vc,vc.data(),sizeof c.motion_vc);
#define COPY_MATRIX(x) std::memcpy(c.motion_##x,x,sizeof x)
        COPY_MATRIX(wvp); COPY_MATRIX(wv); COPY_MATRIX(wvn); COPY_MATRIX(wvb); COPY_MATRIX(wvbn); COPY_MATRIX(pv);
#undef COPY_MATRIX
        c.motion[0]=0.5f;
    }
};
class MotionHistory {
    using Entries=std::unordered_map<uint64_t,std::vector<MotionPose>>;
    Entries before_,now_;
public:
    bool Add(uint64_t key,const MotionPose& p,DrawConstants& c) {
        auto& list=now_[key]; size_t occurrence=list.size();
        auto it=before_.find(key);
        bool matched=it!=before_.end() && occurrence<it->second.size();
        if (matched) it->second[occurrence].Previous(c);
        list.push_back(p);
        return matched && std::memcmp(&it->second[occurrence],&p,sizeof p)!=0;
    }
    void Finish() { before_=std::move(now_); now_.clear(); }
    void Reset() { before_.clear(); now_.clear(); }
};
std::string MotionVertexHlsl(std::string source);
#endif
}
