/* Optional movie-only recorder. No frame captures, text contents or save data. */
#include "movie_diagnostics.h"
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <string.h>

static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static FILE *output,*layout;
static unsigned lines,layoutLines;
static uint64_t started,flushed;
static _Atomic unsigned glyphs;
static uint64_t micros(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000u+t.tv_nsec/1000u;
}
void ngb_movie_diag_open(const char *directory,const char *version,const char *source) {
    char path[4096];
    if (!directory || snprintf(path,sizeof path,"%s/movie-subtitles.log",directory)>=(int)sizeof path) return;
    pthread_mutex_lock(&lock);
    if (!output) {
        output=fopen(path,"w");
        if (output) {
            started=flushed=micros();
            fprintf(output,"Movie subtitle diagnostics v1; build=%s source=%s\n",version,source);
            fprintf(output,"us,event,a,b,c,d,glyphs (numeric fields hexadecimal; glyphs cumulative)\n");
            fflush(output);
            if (snprintf(path,sizeof path,"%s/ui-layout.log",directory)<(int)sizeof path) {
                layout=fopen(path,"w");
                if (layout) {
                    fprintf(layout,"UI layout diagnostics v1; build=%s source=%s\n",version,source);
                    fprintf(layout,"us,event,a,b,c,d,glyphs (numeric fields hexadecimal)\n");
                    fflush(layout);
                }
            }
        }
    }
    pthread_mutex_unlock(&lock);
}
void ngb_movie_diag_emit(const char *event,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
    pthread_mutex_lock(&lock);
    int ui=!strncmp(event,"ui-",3);
    FILE *file=ui ? layout : output;
    unsigned *count=ui ? &layoutLines : &lines,limit=ui ? 12000 : 40000;
    if (file && *count<limit) {
        uint64_t now=micros();
        fprintf(file,"%llu,%s,%08x,%08x,%08x,%08x,%x\n",
            (unsigned long long)(now-started),event,a,b,c,d,atomic_load_explicit(&glyphs,memory_order_relaxed));
        ++*count;
        if (now-flushed>=500000 || *count==limit) {
            if (output) fflush(output);
            if (layout) fflush(layout);
            flushed=now;
        }
    }
    pthread_mutex_unlock(&lock);
}
void ngb_movie_diag_glyph(void) {
    atomic_fetch_add_explicit(&glyphs,1,memory_order_relaxed);
}

/* One sample per second, plus stage changes, for at most three minutes.
 * This observes the original loading drawer; it never advances its state. */
void ngb_movie_diag_loading(uint32_t stage,uint32_t ticks,uint32_t frame,uint32_t gate,
                           uint32_t sx,uint32_t sy,uint32_t anchor,uint32_t count) {
    static uint64_t first,last;
    static uint32_t previous=UINT32_MAX;
    static unsigned samples;
    uint64_t now=micros();
    if (!first) first=now;
    if (samples>=240 || now-first>180000000u) return;
    if (samples && stage==previous && now-last<1000000u) return;
    last=now;previous=stage;samples++;
    ngb_movie_diag_emit("ui-load-state",stage,ticks,frame,gate);
    ngb_movie_diag_emit("ui-load-font",sx,sy,anchor,count);
}
