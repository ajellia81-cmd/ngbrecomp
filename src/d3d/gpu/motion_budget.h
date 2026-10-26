#pragma once
#include <cstdint>
namespace gpu {
// A brief stall must not disable replay for seconds. Sustained overload gets
// a short recovery window; the original guest picture always remains available.
struct MotionBudget {
    uint32_t cooldown=0,late=0;
    bool Observe(int64_t interval,bool replayed) {
        if(replayed && interval>25000000LL && interval<100000000LL) late++;
        else late=0;
        if(late<4) return false;
        cooldown=12;late=0;return true;
    }
    bool Tick() {
        if(!cooldown) return false;
        cooldown--;return true;
    }
};
}
