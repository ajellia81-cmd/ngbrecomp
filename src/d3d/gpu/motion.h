#pragma once
#include "gpu/shaders.h"
#include "gpu/vertex.h"
#include <array>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <limits>
#include <tuple>

namespace gpu {
#ifdef NGB_SMOOTH_MOTION
// History has no guest pointers or GPU allocations. Geometry/material keys are
// matched by owned pose within geometry/material groups; topology changes reset them.
struct MotionPose {
    std::array<std::array<float,4>,192> vc{};
    std::array<uint8_t,192> used{};
    uint32_t programmable=1;
    float wvp[16]{}, wv[16]{}, wvn[16]{}, wvb[4][16]{}, wvbn[4][16]{}, pv[16]{};
    static MotionPose Capture(const State& s,const DrawConstants& c,bool programmable=true) {
        MotionPose p; p.vc=s.vsc; p.programmable=programmable;
        if(programmable) {
            if(s.vs && !s.vs->program.empty()) p.used=VertexProgramConstants(s.vs->program);
            else p.used.fill(1); // unknown program: do not assume constants are unused
        }
#define COPY_MATRIX(x) std::memcpy(p.x,c.x,sizeof p.x)
        COPY_MATRIX(wvp); COPY_MATRIX(wv); COPY_MATRIX(wvn); COPY_MATRIX(wvb); COPY_MATRIX(wvbn); COPY_MATRIX(pv);
#undef COPY_MATRIX
        return p;
    }
    // A nearest candidate is not necessarily the same object. In particular,
    // one visible instance can replace another while both groups have size 1.
    // Reject a large relative input step for the whole draw, never per vertex.
    // This is an input-continuity guard, not a world-space camera-cut metric.
    float RelativeStep(const MotionPose& current) const {
        if(programmable!=current.programmable) return std::numeric_limits<float>::infinity();
        double largest=0;
        auto add=[&](float a,float b) {
            if(!std::isfinite(a) || !std::isfinite(b)) { largest=std::numeric_limits<double>::infinity(); return; }
            double scale=std::max({1.0,std::abs(double(a)),std::abs(double(b))});
            largest=std::max(largest,std::abs(double(a)-b)/scale);
        };
        for(size_t r=0;r<192;r++) if(used[r] || current.used[r])
            for(size_t k=0;k<4;k++) add(vc[r][k],current.vc[r][k]);
#define STEP_MATRIX(x) for(size_t k=0;k<sizeof x/sizeof(float);k++) add(((const float*)x)[k],((const float*)current.x)[k])
        if(!programmable) {
            STEP_MATRIX(wvp); STEP_MATRIX(wv); STEP_MATRIX(wvn);
            STEP_MATRIX(wvb); STEP_MATRIX(wvbn); STEP_MATRIX(pv);
        }
#undef STEP_MATRIX
        return float(largest);
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
// Identical buffer bytes need not mean identical geometry: indexed base
// vertices and unindexed draw lengths select different parts of those bytes.
inline uint64_t MotionRangeKey(uint64_t key,const Assembled& a,const State& s) {
    key=Hash(&a.base_vertex,sizeof a.base_vertex,key);
    key=Hash(&a.draw_count,sizeof a.draw_count,key);
    key=Hash(&a.index_count,sizeof a.index_count,key);
    key=Hash(&a.index16,sizeof a.index16,key);
    key=Hash(&a.topology,sizeof a.topology,key);
    key=Hash(&a.regs,sizeof a.regs,key);
    key=Hash(&a.decode,sizeof a.decode,key);
    // Swap-chain buffer addresses can rotate each game frame. Their logical
    // format/extent is stable; raw addresses would reject all adjacent pairs.
    key=Hash(&s.rt.format,sizeof s.rt.format,key);
    key=Hash(&s.rt.size,sizeof s.rt.size,key);
    key=Hash(&s.zs.format,sizeof s.zs.format,key);
    return Hash(&s.zs.size,sizeof s.zs.size,key);
}
class MotionHistory {
    using Constants=std::array<std::array<float,4>,192>;
    struct Pending { MotionPose pose; DrawConstants* output; Constants* vertices; };
    using Entries=std::unordered_map<uint64_t,std::vector<MotionPose>>;
    Entries before_;
    std::unordered_map<uint64_t,std::vector<Pending>> now_;
public:
    struct Stats { unsigned matched=0,changed=0,missing=0,reordered=0,bounded=0,discontinuous=0; float largest_step=0; };
    // Outputs belong to heap-allocated replay packets, pinned by the recipe
    // list through Resolve. No guest memory or pointers survive Finish/Reset.
    void Add(uint64_t key,const MotionPose& p,DrawConstants& c,Constants* vertices) {
        now_[key].push_back({p,&c,vertices});
    }
    Stats Resolve() {
        Stats stats;
        for (auto& [key,list]:now_) {
            // Resolve is repeatable; never average an already blended output.
            for (auto& entry:list) if (entry.vertices) {
                *entry.vertices=entry.pose.vc;
#define RESTORE_MATRIX(x) std::memcpy(entry.output->x,entry.pose.x,sizeof entry.pose.x)
                RESTORE_MATRIX(wvp); RESTORE_MATRIX(wv); RESTORE_MATRIX(wvn);
                RESTORE_MATRIX(wvb); RESTORE_MATRIX(wvbn); RESTORE_MATRIX(pv);
#undef RESTORE_MATRIX
                entry.output->motion[0]=0;
            }
            auto it=before_.find(key);
            if (it==before_.end()) { stats.missing+=list.size(); continue; }
            auto& old=it->second;
            // VF3 bounds large matching groups too. Do not let repeated
            // particles turn pose matching into an unbounded quadratic sort.
            if (old.size()>32 || list.size()>32) {
                stats.missing+=list.size(); stats.bounded++; continue;
            }
            std::vector<std::tuple<double,size_t,size_t>> candidates;
            for (size_t j=0;j<list.size();j++) for (size_t i=0;i<old.size();i++) {
                if(old.size()==1 && list.size()==1) { candidates.emplace_back(0,j,i); continue; }
                double cost=0;
                auto add=[&](float a,float b) {
                    if (!std::isfinite(a) || !std::isfinite(b)) { cost=std::numeric_limits<double>::infinity(); return; }
                    double d=(double(a)-b)/(1+std::abs(double(a))+std::abs(double(b)));
                    cost+=d*d;
                };
                for (size_t r=0;r<192;r++) if(old[i].used[r] || list[j].pose.used[r])
                    for(size_t k=0;k<4;k++) add(old[i].vc[r][k],list[j].pose.vc[r][k]);
#define POSE_COST(x) for(size_t k=0;k<sizeof old[i].x/sizeof(float);k++) \
    add(((const float*)old[i].x)[k],((const float*)list[j].pose.x)[k])
                if(!old[i].programmable || !list[j].pose.programmable) {
                    POSE_COST(wvp); POSE_COST(wv); POSE_COST(wvn);
                    POSE_COST(wvb); POSE_COST(wvbn); POSE_COST(pv);
                }
#undef POSE_COST
                if (std::isfinite(cost)) candidates.emplace_back(cost,j,i);
            }
            // Plan the entire group before writing any outputs: a greedy draw
            // can otherwise steal another object's much closer previous pose.
            std::sort(candidates.begin(),candidates.end());
            std::vector<bool> used(old.size()),paired(list.size());
            for (auto [cost,j,i]:candidates) {
                (void)cost;
                if (used[i] || paired[j] || !list[j].vertices) continue;
                float step=old[i].RelativeStep(list[j].pose);
                if(std::isfinite(step)) stats.largest_step=std::max(stats.largest_step,step);
                if (!(step<=0.25f)) { stats.discontinuous++; continue; }
                if (!old[i].Midpoint(*list[j].vertices,*list[j].output)) continue;
                used[i]=paired[j]=true;stats.matched++;stats.reordered+=(i!=j);
                stats.changed+=std::memcmp(&old[i],&list[j].pose,sizeof(MotionPose))!=0;
            }
            stats.missing+=std::count(paired.begin(),paired.end(),false);
        }
        return stats;
    }
    void Finish() {
        before_.clear();
        for (auto& [key,list]:now_) {
            auto& saved=before_[key];saved.reserve(list.size());
            for(auto& p:list) saved.push_back(std::move(p.pose));
        }
        now_.clear();
    }
    void Reset() { before_.clear(); now_.clear(); }
};
std::string MotionVertexHlsl(std::string source);
#endif
}
