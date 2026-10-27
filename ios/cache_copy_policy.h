#pragma once
/* The original cache worker sleeps 50 ms before every 64 KiB packet. Keep
 * yielding to other game threads, without throttling active work to 1.25 MiB/s.
 * Pauses, stop requests and all other callers retain their original waits. */
static inline unsigned ngb_cache_copy_wait(unsigned milliseconds,unsigned state) {
    return milliseconds==50 && state==0 ? 1 : milliseconds;
}
