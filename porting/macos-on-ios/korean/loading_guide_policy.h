#pragma once
#include <stddef.h>
#include <stdint.h>
/* Phase 2 still runs the opening transition; the original confirm check is
 * reached in phase 3. Keep the card readable without inviting an early press. */
static inline int ngb_loading_guide_pending(unsigned phase,int x,int y,
                                           const uint8_t *text,size_t length)
{
    if(phase!=2 || x<420 || x>=720 || y<350 || y>=480 || !text)return 0;
    for(size_t i=0;i<length && text[i];){
        if(text[i]&0x80){if(i+1>=length)break;i+=2;continue;}
        /* The native inline X-button command, used by the existing captions. */
        if(text[i]=='@' && i+2<length && text[i+1]=='A' && text[i+2]=='X')return 1;
        i++;
    }
    return 0;
}
