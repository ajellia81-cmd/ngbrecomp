/* Keep the chapter's own settled loading indicator visible during preparation.
 * Never call a second screen drawer or advance the native loading controller. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "chapter_card_state.h"

typedef struct { uint32_t slot,bits; } IndicatorDraw;
static IndicatorDraw indicator_draws[4];
static unsigned indicator_depth;

void ngb_chapter_indicator_begin(unsigned stack,unsigned caller)
{
    IndicatorDraw saved={0,0};
    /* Mode 2 owns the flight into view. Mode 3 has reached its resting place,
     * but its native opacity can fade before chapter preparation returns.
     * The first cdecl argument is copied to the native sprite's alpha field. */
    if(indicator_depth<4 && caller==0x001F5B11u &&
       ngb_chapter_card_start_waiting() && MEM32(0x8DB028)==3 &&
       MEM32(0x8DB160)==0) {
        float alpha=MEMF(stack+4);
        if(alpha>=0.0f && alpha<1.0f) {
            saved.slot=stack+4;saved.bits=MEM32(saved.slot);
            MEMF(saved.slot)=1.0f;
        }
    }
    if(indicator_depth<4)indicator_draws[indicator_depth]=saved;
    indicator_depth++;
}

void ngb_chapter_indicator_end(void)
{
    if(!indicator_depth)return;
    indicator_depth--;
    if(indicator_depth<4) {
        IndicatorDraw saved=indicator_draws[indicator_depth];
        if(saved.slot)MEM32(saved.slot)=saved.bits;
    }
}
