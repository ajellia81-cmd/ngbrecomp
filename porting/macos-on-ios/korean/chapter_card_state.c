#include "chapter_card_state.h"
/* Native chapter setup pumps the UI before reaching its input loop. Keep that
 * lifetime separate from the common card updater, which runs on every pump. */
static unsigned update_seen,update_ready,depth,start_depth,footer_depth;
typedef struct { unsigned active,ready; } CardDraw;
static CardDraw draws[4];
static unsigned starts[4],footers[4],visual_depth,visual_phases[4];
void ngb_chapter_card_start_begin(void){if(start_depth<4)starts[start_depth]=0;start_depth++;}
void ngb_chapter_card_start_ready(void){if(start_depth && start_depth<=4)starts[start_depth-1]=1;}
void ngb_chapter_card_start_end(void){if(start_depth)start_depth--;}
void ngb_chapter_card_update_begin(void){update_seen=1;update_ready=0;}
void ngb_chapter_card_input_ready(void){if(update_seen)update_ready=1;}
void ngb_chapter_card_draw_begin(unsigned mode)
{
    unsigned ready=depth && depth<=4?draws[depth-1].ready:update_ready;
    if(start_depth)ready=start_depth<=4?starts[start_depth-1]:0;
    if(!depth){update_ready=0;update_seen=0;}
    if(depth<4){draws[depth].active=mode>=1 && mode<=7;draws[depth].ready=ready;}
    depth++;
}
void ngb_chapter_card_draw_end(void){if(depth)depth--;}
int ngb_chapter_card_pending(void)
{
    return depth && depth<=4 && draws[depth-1].active && !draws[depth-1].ready;
}
int ngb_chapter_card_can_confirm(void)
{
    return depth && depth<=4 && draws[depth-1].active && draws[depth-1].ready;
}
void ngb_chapter_card_footer_begin(unsigned caller,unsigned mode)
{
    /* Only this chapter-start controller has the extra preparation stage.
     * Other loading screens keep their native captions and input handling. */
    unsigned waiting=start_depth && (start_depth>4 || !starts[start_depth-1]);
    if(footer_depth<4)footers[footer_depth]=caller==0x001F5AF6u && mode>=1 && mode<=7 && waiting;
    footer_depth++;
}
void ngb_chapter_card_footer_end(void){if(footer_depth)footer_depth--;}
int ngb_chapter_card_hide_footer(void)
{
    return footer_depth && footer_depth<=4 && footers[footer_depth-1];
}
int ngb_chapter_card_start_waiting(void)
{
    return start_depth && (start_depth>4 || !starts[start_depth-1]);
}
void ngb_chapter_loading_visual_begin(unsigned phase)
{
    if(visual_depth<4)visual_phases[visual_depth]=phase;
    visual_depth++;
}
void ngb_chapter_loading_visual_end(void){if(visual_depth)visual_depth--;}
int ngb_chapter_loading_visual_active(void){return visual_depth!=0;}
unsigned ngb_chapter_loading_control_phase(unsigned phase)
{
    return visual_depth && visual_depth<=4?visual_phases[visual_depth-1]:phase;
}
unsigned ngb_chapter_card_route(void)
{
    return (start_depth?1u:0u) | (start_depth && start_depth<=4 && starts[start_depth-1]?2u:0u) |
        (depth?4u:0u) | (ngb_chapter_card_hide_footer()?8u:0u) | (visual_depth?16u:0u);
}
