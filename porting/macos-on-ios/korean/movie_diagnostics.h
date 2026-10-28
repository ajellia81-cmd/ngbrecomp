#pragma once
#include <stdint.h>
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
void ngb_movie_diag_open(const char *directory,const char *version,const char *source);
void ngb_movie_diag_emit(const char *event,uint32_t a,uint32_t b,uint32_t c,uint32_t d);
void ngb_movie_diag_glyph(void);
void ngb_movie_diag_cache_wait(uint32_t original,uint32_t effective,uint32_t state);
void ngb_movie_diag_cache_queue(uint32_t available,uint32_t remaining);
void ngb_movie_diag_cache_io(unsigned kind,unsigned end,unsigned bytes,unsigned result);
void ngb_movie_diag_cache_producer(unsigned fast);
void ngb_movie_diag_punctuation(uint32_t code,uint32_t format,uint32_t scale,uint32_t shift);
void ngb_movie_diag_loading(uint32_t stage,uint32_t ticks,uint32_t frame,uint32_t gate,
                           uint32_t sx,uint32_t sy,uint32_t anchor,uint32_t count);
void ngb_ios_movie_probe(unsigned stage);
void ngb_ios_movie_draw(unsigned state,unsigned caller);
#endif
