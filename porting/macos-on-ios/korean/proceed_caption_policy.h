#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Reuse the native localized footer only for its exact English caption. */
static inline unsigned ngb_proceed_caption_kind(int korean,unsigned font,int x,int y,
                                               const uint8_t *text,size_t length)
{
    if(!korean || font>2 || x<420 || x>=720 || y<350 || y>=480 || !text)return 0;
    if(length>=8 && !memcmp(text,"Proceed",8))return 1;
    if(length>=12 && !memcmp(text,"@BX Proceed",12))return 2;
    return 0;
}
