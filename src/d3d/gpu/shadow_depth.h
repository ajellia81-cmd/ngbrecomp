#pragma once
#include <algorithm>
#include <cstdint>
namespace gpu {
// Backport of gamboozino/ngbrecomp 7caa75a (#106). The bit reuses an
// existing pipeline-key field; its serialized layout does not change.
constexpr uint32_t kDsDynamicBias=2;
struct ShadowDepthPull { float steps=0,slope=0; bool enabled=false; };
inline ShadowDepthPull ShadowReceiverPull(bool enabled,bool receiver,bool triangles,
                                         bool has_depth,bool tests_depth,bool writes_depth,float scale) {
    if (!enabled || !receiver || !triangles || !has_depth || !tests_depth || writes_depth) return {};
    return {-2.0f,-0.5f*std::max(scale,1.0f),true};
}
// A newly requested biased variant must not hide the surface while its
// pipeline compiles. Use the original cached pipeline until it is ready.
template<class Key,class Lookup> auto ShadowPipelineOrOriginal(Key& key,Lookup&& lookup) {
    auto pipe=lookup(key);
    if (!pipe && (key.has_ds&kDsDynamicBias)) {
        Key plain=key;plain.has_ds&=~kDsDynamicBias;
        if (auto fallback=lookup(plain)) {key=plain;return fallback;}
    }
    return pipe;
}
}
