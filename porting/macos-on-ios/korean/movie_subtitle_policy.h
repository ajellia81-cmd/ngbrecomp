#pragma once
#include <stdint.h>
typedef struct {
    uint32_t player,frame,cue;
    int seen;
} NgbMovieSubtitlePolicy;

/* A movie cue is repeated for several frames. Replace the old caption once
 * when its ID changes; never restart the current line on repeated packets. */
static inline int ngb_movie_subtitle_switch(NgbMovieSubtitlePolicy *s,
        int enabled,uint32_t player,uint32_t frame,uint32_t cue) {
    if (!enabled || !player) { s->seen=0;return 0; }
    if (cue>=680) return 0;
    if (!s->seen || player!=s->player || frame<s->frame) {
        *s=(NgbMovieSubtitlePolicy){player,frame,cue,1};return 0;
    }
    int changed=cue!=s->cue;
    s->frame=frame;s->cue=cue;
    return changed;
}
