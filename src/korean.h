#ifndef NGB_KOREAN_H
#define NGB_KOREAN_H
typedef void (*ngb_korean_fn)(void);
int ngb_korean_active(void);
ngb_korean_fn ngb_korean_lookup(unsigned address);
ngb_korean_fn ngb_korean_lookup_raw(unsigned address);
/* Only the Korean font bank uses a full-cell ASCII space in the original
 * Japanese layout. Keep Latin-mode metrics and all non-space glyphs intact. */
static inline unsigned ngb_korean_space_advance(unsigned cell,unsigned font) {
    return font==1 ? (cell+1)/2 : cell;
}
void ngb_korean_font_metrics(unsigned font,unsigned width,unsigned height,float sx,float sy);
#endif
