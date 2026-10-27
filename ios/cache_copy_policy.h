#pragma once
/* The original cache worker sleeps 50 ms before every 64 KiB packet. Keep
 * yielding to other game threads, without throttling active work to 1.25 MiB/s.
 * Background gameplay, pauses, stop requests and all other callers retain
 * their original waits. Only a busy foreground loading screen qualifies. */
static inline unsigned ngb_cache_copy_wait(unsigned milliseconds,unsigned state,unsigned foreground) {
    return milliseconds==50 && state==0 && foreground ? 1 : milliseconds;
}

/* Only the library's service thread may replace its display-paced wait while
 * the foreground loader is waiting for an active cache copy. */
static inline unsigned ngb_cache_producer_fast(unsigned state,unsigned foreground,
                                              unsigned copying,unsigned stopping) {
    return state==0 && foreground && copying==1 && !stopping;
}
