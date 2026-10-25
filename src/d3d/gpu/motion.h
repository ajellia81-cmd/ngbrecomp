#pragma once
#include "gpu/shaders.h"
#include <array>
#include <cstring>
#include <cmath>
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
    // Blend inputs once, before the original vertex program. In particular,
    // EQUAL-depth material passes then use the original position arithmetic.
    // Double intermediates avoid overflow and keep shared inputs bit identical.
    bool Midpoint(std::array<std::array<float,4>,192>& current,DrawConstants& c) const {
        auto mix=[](float a,float b) { return float((double(a)+double(b))*0.5); };
        for (size_t i=0;i<vc.size();i++) for (size_t j=0;j<4;j++)
            if (!std::isfinite(vc[i][j]) || !std::isfinite(current[i][j])) return false;
#define VALID_MATRIX(x) for (size_t i=0;i<sizeof x/sizeof(float);i++) \
    if (!std::isfinite(((const float*)x)[i]) || !std::isfinite(((const float*)c.x)[i])) return false
        VALID_MATRIX(wvp); VALID_MATRIX(wv); VALID_MATRIX(wvn);
        VALID_MATRIX(wvb); VALID_MATRIX(wvbn); VALID_MATRIX(pv);
#undef VALID_MATRIX
        for (size_t i=0;i<vc.size();i++) for (size_t j=0;j<4;j++) current[i][j]=mix(vc[i][j],current[i][j]);
#define MIX_MATRIX(x) for (size_t i=0;i<sizeof x/sizeof(float);i++) \
    ((float*)c.x)[i]=mix(((const float*)x)[i],((float*)c.x)[i])
        MIX_MATRIX(wvp); MIX_MATRIX(wv); MIX_MATRIX(wvn);
        MIX_MATRIX(wvb); MIX_MATRIX(wvbn); MIX_MATRIX(pv);
#undef MIX_MATRIX
        c.motion[0]=0;
        return true;
    }
};
class MotionHistory {
    using Entries=std::unordered_map<uint64_t,std::vector<MotionPose>>;
    Entries before_,now_;
public:
    bool Add(uint64_t key,const MotionPose& p,DrawConstants& c,std::array<std::array<float,4>,192>* vertices=nullptr) {
        auto& list=now_[key]; size_t occurrence=list.size();
        auto it=before_.find(key);
        bool matched=it!=before_.end() && occurrence<it->second.size();
        if (matched && vertices) matched=it->second[occurrence].Midpoint(*vertices,c);
        list.push_back(p);
        return matched && std::memcmp(&it->second[occurrence],&p,sizeof p)!=0;
    }
    void Finish() { before_=std::move(now_); now_.clear(); }
    void Reset() { before_.clear(); now_.clear(); }
};
std::string MotionVertexHlsl(std::string source);
#endif
}
