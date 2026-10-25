#pragma once
#include <stdint.h>
/* One atomic command replaces the previous one; a slow engine must never queue
 * a backlog of old motor pulses. Match SDL's previous one-second expiry. */
static inline uint64_t ngb_haptic_command(uint16_t low,uint16_t high,uint32_t ms) {
    return ((uint64_t)ms<<32)|((uint32_t)low<<16)|high;
}
static inline void ngb_haptic_values(uint64_t command,uint32_t ms,int active,uint16_t *low,uint16_t *high) {
    if (!active || (uint32_t)(ms-(uint32_t)(command>>32))>=1000) { *low=*high=0;return; }
    *low=(uint16_t)(command>>16);*high=(uint16_t)command;
}

static inline int ngb_haptic_retry_ready(uint32_t deadline,uint32_t now) {
    return (int32_t)(now-deadline)>=0;
}
