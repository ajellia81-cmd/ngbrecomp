/* Defer only the native chapter drawing timeline, never its preparation loop.
 * Native mode 2 uses 200 ticks; its final 20 ticks move the indicator out. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "chapter_card_state.h"

typedef struct { uint32_t mode,counter; unsigned changed; } ChapterVisual;
static ChapterVisual visuals[4];
static unsigned visual_depth,seen_generation,held,exit_started;
static uint32_t exit_frame;

void ngb_chapter_animation_draw_begin(void)
{
    ChapterVisual saved={MEM32(0x8DB028),MEM32(0x8DAF70),0};
    unsigned busy=0,generation=ngb_chapter_card_start_generation();
    if(generation!=seen_generation) {
        seen_generation=generation;held=0;exit_started=0;
    }
    if(visual_depth<4 && ngb_chapter_card_start_active() &&
       MEM32(0x8DB160)==0 && (saved.mode==2 || saved.mode==3)) {
        unsigned waiting=ngb_chapter_card_start_waiting();
        if(waiting && ((saved.mode==2 && saved.counter<=20) || saved.mode==3))held=1;
        if(held) {
            uint32_t counter=20;
            if(!waiting) {
                uint32_t frame=MEM32(0xD21734);
                if(!exit_started){exit_started=1;exit_frame=frame;}
                uint32_t elapsed=frame-exit_frame;
                if(elapsed>=20){held=0;counter=0;}
                else counter=20-elapsed;
            }
            if(counter) {
                /* Only the existing drawer sees these values. Its caller's
                 * mode and countdown are restored before the next update. */
                MEM32(0x8DB028)=2;MEM32(0x8DAF70)=counter;
                saved.changed=1;busy=1;
            }
        }
    }
    if(visual_depth<4)visuals[visual_depth]=saved;
    visual_depth++;
    ngb_chapter_card_visual_begin(busy);
}

void ngb_chapter_animation_draw_end(void)
{
    if(!visual_depth)return;
    visual_depth--;
    if(visual_depth<4 && visuals[visual_depth].changed) {
        MEM32(0x8DB028)=visuals[visual_depth].mode;
        MEM32(0x8DAF70)=visuals[visual_depth].counter;
    }
    ngb_chapter_card_visual_end();
}
