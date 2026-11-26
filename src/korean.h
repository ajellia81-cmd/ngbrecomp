#ifndef NGB_KOREAN_H
#define NGB_KOREAN_H
typedef void (*ngb_korean_fn)(void);
int ngb_korean_active(void);
ngb_korean_fn ngb_korean_lookup(unsigned address);
ngb_korean_fn ngb_korean_lookup_raw(unsigned address);
/* Korean ASCII and full-width spaces share a quarter-cell advance. The
 * baked Options labels leave about one fifth of a glyph cell as extra word
 * spacing, beyond glyph side bearings. Keep Latin modes and ink intact. */
static inline unsigned ngb_korean_space_advance(unsigned cell,unsigned font) {
    return font==1 ? (cell+3)/4 : cell;
}
void ngb_korean_font_metrics(unsigned font,unsigned width,unsigned height,float sx,float sy);
int ngb_korean_punctuation_at(unsigned text);
int ngb_korean_punctuation_left(unsigned def,unsigned text,unsigned font,float scale);
unsigned ngb_korean_punctuation_advance(unsigned original,unsigned def,unsigned text,unsigned font,float scale);
void ngb_korean_center_paragraph(unsigned state,unsigned widths,unsigned origins,unsigned count,unsigned caller);
#endif
