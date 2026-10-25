#include "language_policy.h"
#include "nv2a_vk/host/pair_mailbox.h"
#include "nv2a_vk/host/native_viewport.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"runtime UI check failed at %d\n",__LINE__); return 1; } } while(0)
int main(void) {
    for (int language=0;language<3;language++) {
        CHECK(ngb_language_step(ngb_language_step(language,1),-1)==language);
        CHECK(ngb_language_game_index(language)<2);
    }
    CHECK(ngb_language_game_index(2)==0);
    CHECK(ngb_language_text_column(1,0)==1 && ngb_language_text_column(1,1)==1);
    CHECK(ngb_language_text_column(0,0)==0 && ngb_language_text_column(0,1)==1);
    NgbPairMailbox m={0}; NgbPresentPair active;
    CHECK(ngb_pair_offer(&m,(NgbPresentPair){.mid=1,.end=2}));
    CHECK(ngb_pair_take(&m,&active));
    CHECK(ngb_pair_busy(&m,1) && ngb_pair_busy(&m,2));
    CHECK(!ngb_pair_offer(&m,(NgbPresentPair){.mid=1,.end=3}));
    CHECK(ngb_pair_offer(&m,(NgbPresentPair){.mid=3,.end=4}));
    CHECK(ngb_pair_offer(&m,(NgbPresentPair){.mid=5,.end=6}));
    CHECK(!ngb_pair_busy(&m,3) && ngb_pair_busy(&m,5) && m.superseded==1);
    ngb_pair_cancel(&m);
    CHECK(active.epoch!=m.epoch && ngb_pair_busy(&m,1) && !ngb_pair_busy(&m,5));
    ngb_pair_complete(&m);
    CHECK(!ngb_pair_busy(&m,1));
    const double aspects[]={4.0/3,16.0/9,16.0/10,64.0/27,3000.0/1400};
    for (unsigned i=0;i<sizeof aspects/sizeof aspects[0];i++) {
        uint32_t x,y,w,h;
        ngb_safe_overlay(3000,1400,aspects[i],0.06f,0.02f,0.06f,0.03f,500,60,&x,&y,&w,&h);
        CHECK(x>=180 && y>=28 && w>0 && h>0 && x+w<=2820 && y+h<=1358);
    }
    puts("Runtime UI: language enum separation, nonblocking pair ownership/cancellation and safe FPS bounds: OK");
    return 0;
}
