#pragma once
#include <stdint.h>
/* One atomic command replaces the previous one; a slow engine must never queue
 * a backlog of old motor pulses. Xbox motor state lasts until the next command;
 * lifecycle/disconnect explicitly clear it. */
static inline uint64_t ngb_haptic_command(uint16_t low,uint16_t high,uint32_t ms) {
    return ((uint64_t)ms<<32)|((uint32_t)low<<16)|high;
}
static inline void ngb_haptic_values(uint64_t command,uint32_t ms,int active,uint16_t *low,uint16_t *high) {
    (void)ms;
    if (!active) { *low=*high=0;return; }
    *low=(uint16_t)(command>>16);*high=(uint16_t)command;
}

static inline int ngb_haptic_retry_ready(uint32_t deadline,uint32_t now) {
    return (int32_t)(now-deadline)>=0;
}
