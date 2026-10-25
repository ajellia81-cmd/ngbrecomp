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
/* Fit an overlay into both the content rectangle and the device's safe area.
 * Input pixels retain their original row stride when the copy is clipped. */
static inline void ngb_safe_overlay(uint32_t dw,uint32_t dh,double aspect,
    float left,float top,float right,float bottom,uint32_t ow,uint32_t oh,
    uint32_t *x,uint32_t *y,uint32_t *w,uint32_t *h)
{
    uint32_t cw,ch; ngb_native_viewport(dw,dh,aspect,&cw,&ch);
    uint32_t pad=dh/100>8 ? dh/100 : 8;
    uint32_t sx=(uint32_t)ceilf(fmaxf(0,left)*dw),sy=(uint32_t)ceilf(fmaxf(0,top)*dh);
    uint32_t cx=(dw-cw)/2,cy=(dh-ch)/2;
    *x=(sx>cx ? sx : cx)+pad; *y=(sy>cy ? sy : cy)+pad;
    int64_t ex=(int64_t)dw-(int64_t)ceilf(fmaxf(0,right)*dw)-pad;
    int64_t ey=(int64_t)dh-(int64_t)ceilf(fmaxf(0,bottom)*dh)-pad;
    *w=ex>*x ? (ow<(uint64_t)(ex-*x) ? ow : (uint32_t)(ex-*x)) : 0;
    *h=ey>*y ? (oh<(uint64_t)(ey-*y) ? oh : (uint32_t)(ey-*y)) : 0;
}
#endif
