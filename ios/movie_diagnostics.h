#pragma once
#include <stdint.h>
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
void ngb_movie_diag_open(const char *directory,const char *version,const char *source);
void ngb_movie_diag_emit(const char *event,uint32_t a,uint32_t b,uint32_t c,uint32_t d);
void ngb_movie_diag_glyph(void);
void ngb_ios_movie_probe(unsigned stage);
void ngb_ios_movie_draw(unsigned state,unsigned caller);
#endif
