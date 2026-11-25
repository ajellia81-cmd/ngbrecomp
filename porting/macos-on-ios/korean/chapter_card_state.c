#include "chapter_card_state.h"
/* The native UI controller and drawer run on the guest UI thread. A readiness
 * observation belongs to one subsequent draw; never retain it across scenes. */
static unsigned update_seen,update_ready,depth;
typedef struct { unsigned active,ready; } CardDraw;
static CardDraw draws[4];
void ngb_chapter_card_update_begin(void){update_seen=1;update_ready=0;}
void ngb_chapter_card_input_ready(void){if(update_seen)update_ready=1;}
void ngb_chapter_card_draw_begin(unsigned mode)
{
    unsigned ready=depth && depth<=4?draws[depth-1].ready:update_ready;
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
