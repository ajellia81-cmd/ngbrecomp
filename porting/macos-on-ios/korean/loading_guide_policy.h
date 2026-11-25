#pragma once
#include <stddef.h>
#include <stdint.h>
/* Hide only while the chapter card's own controller has not reached its
 * native confirm check. Generic loading screens retain their original UI. */
static inline int ngb_loading_guide_pending(int card_pending,int x,int y,
                                           const uint8_t *text,size_t length)
{
    if(!card_pending || x<420 || x>=720 || y<350 || y>=480 || !text)return 0;
    for(size_t i=0;i<length && text[i];){
        if(text[i]&0x80){if(i+1>=length)break;i+=2;continue;}
        /* The native inline X-button command, used by the existing captions. */
        if(text[i]=='@' && i+2<length && text[i+1]=='B' && text[i+2]=='X')return 1;
        i++;
    }
    return 0;
}
