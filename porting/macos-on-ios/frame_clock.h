#ifndef NGB_CONVERSION_FRAME_CLOCK_H
#define NGB_CONVERSION_FRAME_CLOCK_H
#include <stdint.h>
/* A suspended app owes no simulation frames. Keep normal phase, but discard
 * overdue deadlines instead of running guest vblanks at display speed. */
static inline int64_t ngb_frame_clock_rebase(int64_t previous,int64_t now,int64_t period,int foreground)
{
    return !foreground || now-previous>period ? now : previous;
}
#endif
