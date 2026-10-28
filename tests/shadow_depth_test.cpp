#include "../src/d3d/gpu/shadow_depth.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace gpu;
struct Key { unsigned has_ds=1,vs=7,ps=9,samples=1; };
int main() {
    auto pull=ShadowReceiverPull(true,true,true,true,true,false,2868.0f/640);
    assert(pull.enabled && pull.steps==-2 && std::fabs(pull.slope+2.240625f)<1e-6f);
    auto original=ShadowReceiverPull(false,true,true,true,true,false,4);
    assert(!original.enabled && !original.steps && !original.slope); // native Mac untouched
    assert(!ShadowReceiverPull(true,false,true,true,true,false,4).enabled); // normal wall
    assert(!ShadowReceiverPull(true,true,false,true,true,false,4).enabled); // non-triangle effect
    assert(!ShadowReceiverPull(true,true,true,false,true,false,4).enabled);
    assert(!ShadowReceiverPull(true,true,true,true,false,false,4).enabled);
    assert(!ShadowReceiverPull(true,true,true,true,true,true,4).enabled); // original depth-writing pass
    assert(ShadowReceiverPull(true,true,true,true,true,false,.5f).slope==-.5f);
    const float game_steps=3,game_slope=.25f;
    assert(game_steps+pull.steps==1 && game_slope+pull.slope<0); // additive, not replacing the game's own bias
    Key key;key.has_ds|=kDsDynamicBias;std::vector<unsigned> seen;
    auto pipeline=ShadowPipelineOrOriginal(key,[&](const Key& k){seen.push_back(k.has_ds);return k.has_ds==1 ? 42u : 0u;});
    assert(pipeline==42 && key.has_ds==1 && key.vs==7 && key.ps==9 && key.samples==1);
    assert((seen==std::vector<unsigned>{3,1})); // surface is drawn while the new variant compiles
    seen.clear();key.has_ds=3;
    pipeline=ShadowPipelineOrOriginal(key,[&](const Key& k){seen.push_back(k.has_ds);return k.has_ds==3 ? 43u : 42u;});
    assert(pipeline==43 && key.has_ds==3 && seen.size()==1);
    key.has_ds=1;seen.clear();
    pipeline=ShadowPipelineOrOriginal(key,[&](const Key& k){seen.push_back(k.has_ds);return 0u;});
    assert(!pipeline && seen.size()==1); // do not disguise ordinary pipeline failure
    puts("PASS: upstream shadow receiver selector and high-scale pull; other passes unchanged; cached original pipeline prevents missing draws");
}
