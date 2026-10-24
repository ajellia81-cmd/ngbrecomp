#ifndef NGB_NATIVE_VIEWPORT_H
#define NGB_NATIVE_VIEWPORT_H
#include <math.h>
#include <stdint.h>

/* Largest content rectangle inside the drawable at the requested aspect.
 * Zero aspect stretches to the complete drawable. Pixel edges are rounded. */
static inline void ngb_native_viewport(uint32_t dw, uint32_t dh, double aspect,
                                       uint32_t *width, uint32_t *height)
{
    if (dw && dh && aspect > 0) {
        if ((double)dw / dh > aspect) dw = (uint32_t)llround(dh * aspect);
        else dh = (uint32_t)llround(dw / aspect);
    }
    *width = dw;
    *height = dh;
}
#endif
