#pragma once
// NGB -> VF3 scene bridge. The VF3 pairing/cut/interpolation implementation is
// unchanged; this adapter supplies geometry, model-to-view matrices and viewports.
#include "gpu/shaders.h"
#include "gpu/vertex.h"
#include "../../../third_party/vf3_scene/r3d_scene.h"
#include <memory>
#include <unordered_map>

namespace gpu {
#ifdef NGB_SMOOTH_MOTION
using ScenePosition = std::array<float,4>;
struct SceneOutput { std::vector<ScenePosition> positions; };
class SceneMotion {
public:
    struct Stats {
        uint32_t draws=0,models=0,shared=0,unsupported=0,fallback=0,rigid=0,cached=0,changed=0,cuts=0;
        uint32_t fixed_rigid=0,fixed_skin=0,vertices=0,evaluated_vertices=0;
        video::InterpStats vf{};
        double cpu_ms=0,extract_ms=0,interpolate_ms=0,project_ms=0;
    };
    SceneMotion();
    ~SceneMotion();
    SceneMotion(const SceneMotion&)=delete;
    SceneMotion& operator=(const SceneMotion&)=delete;
    bool Add(const State&,const Assembled&,const DrawConstants&,SceneOutput&);
    Stats Resolve();
    void Finish();
    void Reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// A CPU copy of the original vertex program's position calculation. It reads
// the actual declaration/streams/constants, including relative bone addressing.
// Exposed for comparison fixtures; it never executes guest code or touches GPU state.
bool SceneVertexPosition(const State&,const Assembled&,uint32_t,const DrawConstants&,ScenePosition&);
std::string MotionVertexHlsl(std::string);
#endif
} // namespace gpu
