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

static _Atomic uint32_t cachePolls,cacheReady,cacheAvailable,cacheRemaining;
static _Atomic uint32_t ioCalls[2],ioBytes[2],ioMicros[2],ioMax[2],ioFailures[2];
static _Thread_local uint64_t ioBegin[2];
static _Thread_local uint32_t ioRequest[2];
static _Atomic uint32_t producerFast,producerDisplay;
void ngb_movie_diag_cache_producer(unsigned fast) {
    atomic_fetch_add(fast ? &producerFast : &producerDisplay,1);
}

void ngb_movie_diag_cache_queue(uint32_t available,uint32_t remaining) {
    atomic_fetch_add(&cachePolls,1);
    if (available >= (remaining<65536u ? remaining : 65536u))
        atomic_fetch_add(&cacheReady,1);
    atomic_store(&cacheAvailable,available);
    atomic_store(&cacheRemaining,remaining);
}

/* SDK-call elapsed time, not asynchronous completion latency. Byte counts are
 * requested bytes, not proof that all bytes were transferred. Thread-local
 * starts avoid mixing reads from different library threads or callback paths. */
void ngb_movie_diag_cache_io(unsigned kind,unsigned end,unsigned bytes,unsigned result) {
    if (kind>1) return;
    if (!end) { ioBegin[kind]=micros();ioRequest[kind]=bytes;return; }
    if (!ioBegin[kind]) return;
    uint32_t elapsed=(uint32_t)(micros()-ioBegin[kind]);
    ioBegin[kind]=0;
    atomic_fetch_add(&ioCalls[kind],1);
    atomic_fetch_add(&ioBytes[kind],ioRequest[kind]);
    atomic_fetch_add(&ioMicros[kind],elapsed);
    if (!result) atomic_fetch_add(&ioFailures[kind],1);
    uint32_t maximum=atomic_load(&ioMax[kind]);
    while (elapsed>maximum && !atomic_compare_exchange_weak(&ioMax[kind],&maximum,elapsed)) {}
}

void ngb_movie_diag_cache_wait(uint32_t original,uint32_t effective,uint32_t state) {
    static uint64_t last;
    static uint32_t calls,requested,actual,previous=UINT32_MAX;
    static unsigned samples;
    uint64_t now=micros();
    calls++;requested+=original;actual+=effective;
    if (samples>=240 || (samples && state==previous && now-last<1000000u)) return;
    last=now;previous=state;samples++;
    ngb_movie_diag_emit("ui-cache-wait",calls,requested,actual,state);
    ngb_movie_diag_emit("ui-cache-queue",atomic_load(&cachePolls),atomic_load(&cacheReady),
        atomic_load(&cacheAvailable),atomic_load(&cacheRemaining));
    for (unsigned i=0;i<2;i++)
        ngb_movie_diag_emit(i ? "ui-cache-write" : "ui-cache-read",atomic_load(&ioCalls[i]),
            atomic_load(&ioBytes[i]),atomic_load(&ioMicros[i]),atomic_load(&ioMax[i]));
    ngb_movie_diag_emit("ui-cache-fail",atomic_load(&ioFailures[0]),atomic_load(&ioFailures[1]),0,0);
    ngb_movie_diag_emit("ui-cache-producer",atomic_load(&producerFast),atomic_load(&producerDisplay),0,0);
}

void ngb_movie_diag_punctuation(uint32_t code,uint32_t format,uint32_t scale,uint32_t shift) {
    static uint32_t codes[16],formats[16];
    static unsigned count;
    for (unsigned i=0;i<count;i++) if (codes[i]==code && formats[i]==format) return;
    if (count==16) return;
    codes[count]=code;formats[count++]=format;
    ngb_movie_diag_emit("ui-punct-shift",code,format,scale,shift);
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
