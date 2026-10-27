/* Korean text layout uses the existing glyph pictures and measured line widths.
 * No texture edits, font replacement, string rewriting, or runtime logging. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "korean.h"
#include "movie_diagnostics.h"
#include <math.h>

int ngb_korean_punctuation_at(unsigned text) {
    if (!text) return 0;
    unsigned c=MEM8(text);
    if (c==0x81) {
        c=MEM8(text+1);
        return c==0x41 || c==0x42 || c==0x43 || c==0x44 ||
               c==0x46 || c==0x47 || c==0x48 || c==0x49;
    }
    return c=='.' || c==',' || c==':' || c==';' || c=='!' || c=='?';
}

static unsigned swizzle(unsigned x,unsigned y,unsigned w,unsigned h) {
    unsigned out=0,shift=0;
    for (unsigned bit=1;bit<w || bit<h;bit<<=1) {
        if (bit<w) out|=!!(x&bit)<<shift++;
        if (bit<h) out|=!!(y&bit)<<shift++;
    }
    return out;
}

/* Bounds include every nontransparent bright pixel, including faint AA.
 * The glyph callback has already selected this character's window in its page. */
typedef struct { uint32_t key[10]; float left,advance,ink_right; int valid; } PunctBounds;
static PunctBounds bounds_cache[32];
static unsigned bounds_next;
static PunctBounds *punct_bounds(unsigned def) {
    if (!def) return NULL;
    unsigned texture=MEM32(def+16);
    if (texture>=4096) return NULL;
    unsigned hdr=MEM32(0x8DBB28+texture*8),pal_hdr=MEM32(0x8DBB2C+texture*8);
    if (!hdr) return NULL;
    unsigned fmt=MEM32(hdr+12),kind=fmt>>8&255,lw=fmt>>20&15,lh=fmt>>24&15;
    if (lw>10 || lh>10 || (kind!=0x19 && kind!=0x0B && kind!=6 && kind!=4)) return NULL;
    unsigned w=1u<<lw,h=1u<<lh,data=MEM32(hdr+4)|0x80000000u;
    unsigned palette=kind==0x0B && pal_hdr ? MEM32(pal_hdr+4)|0x80000000u : 0;
    if (kind==0x0B && !palette) return NULL;
    uint32_t key[]={def,MEM32(def),MEM32(def+4),MEM32(def+8),MEM32(def+12),
                    MEM32(def+20),hdr,fmt,data,palette};
    for (unsigned i=0;i<32;i++) if (bounds_cache[i].valid && !memcmp(bounds_cache[i].key,key,sizeof key))
        return &bounds_cache[i];
    float u0=MEMF(def),v0=MEMF(def+4),u1=MEMF(def+8),v1=MEMF(def+12);
    if (!isfinite(u0+v0+u1+v1) || u0<0 || v0<0 || u1>1 || v1>1) return NULL;
    int x0=(int)floorf(u0*w),x1=(int)ceilf(u1*w),y0=(int)floorf(v0*h),y1=(int)ceilf(v1*h);
    if (x0>=x1 || y0>=y1 || x1-x0>64 || y1-y0>64) return NULL;
    int lo=x1,hi=x0-1;
    for (int y=y0;y<y1;y++) for (int x=x0;x<x1;x++) {
        unsigned off=swizzle(x,y,w,h),color;
        if (kind==0x19) color=((uint32_t)MEM8(data+off)<<24)|0xFFFFFF;
        else if (kind==0x0B) color=MEM32(palette+MEM8(data+off)*4);
        else if (kind==6) color=MEM32(data+off*4);
        else {
            unsigned p=MEM16(data+off*2);
            color=((p>>12)*17u<<24)|(((p>>8)&15)*17u<<16)|(((p>>4)&15)*17u<<8)|((p&15)*17u);
        }
        if (!(color>>24) || ((color>>16&255)<=120 && (color>>8&255)<=120 && (color&255)<=120)) continue;
        if (x<lo) lo=x;
        if (x>hi) hi=x;
    }
    if (hi<lo) return NULL;
    float unit=MEMF(0x43B494); // The same texel-to-console factor as the original advance calculation.
    if (!(unit>0 && unit<=2)) return NULL;
    PunctBounds *result=&bounds_cache[bounds_next++%32];
    memcpy(result->key,key,sizeof key);
    result->left=fmaxf(0,(lo-u0*w)*unit-1);
    result->advance=fminf((u1-u0)*w*unit,(hi-lo+1)*unit+2);
    result->ink_right=(hi+1-u0*w)*unit;
    result->valid=1;
    return result;
}

int ngb_korean_punctuation_left(unsigned def,unsigned text,unsigned font,float scale) {
    if (font!=1 || !ngb_korean_punctuation_at(text) || !(scale>0 && scale<=8)) return 0;
    PunctBounds *b=punct_bounds(def);
    return b ? (int)floorf(b->left*scale) : 0;
}
unsigned ngb_korean_punctuation_advance(unsigned original,unsigned def,unsigned text,unsigned font,float scale) {
    if (font!=1 || !ngb_korean_punctuation_at(text) || !(scale>0 && scale<=8)) return original;
    PunctBounds *b=punct_bounds(def);
    if (!b) return original;
    float ink_end=b->ink_right*scale-floorf(b->left*scale);
    unsigned compact=(unsigned)fmaxf(1,fmaxf(nearbyintf(b->advance*scale),ceilf(ink_end+.5f)));
    return compact<original ? compact : original;
}

/* The drawer has finished its line measurements. Move all line origins by
 * one common delta: left alignment, indentation, blank lines and Y stay intact.
 * Loading prose is an explicit full-page caller. Other wide, multi-line prose
 * at the page's left margin is centered as a block; columns, footer hints,
 * subtitles with per-line centering and small HUD prompts do not qualify. */
void ngb_korean_center_paragraph(unsigned state,unsigned widths,unsigned origins,unsigned count,unsigned caller) {
    int known_loading=caller==0x00205574u;
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
    if (known_loading) ngb_movie_diag_loading(MEM32(0x8DB160),MEM32(0xBCDFDC),
        MEM32(0xD21734),MEM8(0xD52F2D),MEM32(state+0x1C),MEM32(state+0x20),MEM32(state+0x44),count);
#endif
    if (MEM32(state+0x38)!=1 || (MEM8(state+0x4A)&15)!=0 || count<2 || count>255) return;
    int x=(int16_t)MEM16(state+0x44),y=(int16_t)MEM16(state+0x46);
    if (x<40 || x>140 || (!known_loading && (y<0 || y>320))) return;
    int left=720,right=0,nonempty=0;
    for (unsigned i=0;i<count;i++) {
        unsigned width=MEM16(widths+i*2);
        int origin=(int16_t)MEM16(origins+i*2);
        if (!width) continue;
        if (width>680 || origin<0 || origin+(int)width>720) return;
        if (origin<left) left=origin;
        if (origin+(int)width>right) right=origin+width;
        nonempty++;
    }
    if (nonempty<2 || (!known_loading && right-left<360)) return;
    int delta=(720-(right-left))/2-left;
    if (!delta) return;
    for (unsigned i=0;i<count;i++) MEM16(origins+i*2)=(int16_t)MEM16(origins+i*2)+delta;
}
