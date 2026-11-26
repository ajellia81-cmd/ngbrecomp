/* Reuse the original busy-screen graphics during chapter preparation. The
 * loader controller, resource preparation and confirmation input stay native. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "chapter_card_state.h"
#include <string.h>

void ngb_chapter_loading_visual(void) {
    static uint32_t last_frame,last_tick,visual_prompt;
    static int seen_clock;
    if(ngb_chapter_loading_visual_active())return;
    if(!ngb_chapter_card_start_waiting()){seen_clock=0;return;}
    uint32_t phase=MEM32(0x8DB160),frame=MEM32(0xD21734);
    if(phase) {
        if(phase==2){last_frame=frame;last_tick=MEM32(0xBCDFDC);
            visual_prompt=MEM32(0x8DB248);seen_clock=1;}
        return; /* Existing loading screens draw themselves exactly once. */
    }
    uint32_t mode=MEM32(0x8DB028);
    if(mode!=2 && mode!=3)return;
    if(!seen_clock){last_frame=frame;last_tick=200;visual_prompt=0;seen_clock=1;}
    uint32_t regs[]={eax,ecx,edx,ebx,esi,edi,esp,g_ebp,g_seh_ebp};
    RecompXmm simd[]={xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7};
    double fp[8];memcpy(fp,g_fp_stack,sizeof fp);
    int top=g_fp_top,cmp=g_fp_cmp;uint16_t cw=g_fp_control_word,cc=g_fp_cc;
    uint32_t df=g_df;
    uint32_t saved[]={MEM32(0x8DB160),MEM32(0x8DB054),MEM32(0xBCDFDC),
                      MEM32(0x8DB248),MEM32(0x8DB6B8)};
    ngb_chapter_loading_visual_begin(phase);
    /* Busy presentation only. No loading update runs with these temporary
     * values, and the prose sub-drawer is excluded from this extra overlay. */
    MEM32(0x8DB160)=2;MEMF(0x8DB054)=1.0f;
    MEM32(0xBCDFDC)=last_tick+(frame-last_frame);
    MEM32(0x8DB248)=visual_prompt;MEM32(0x8DB6B8)=0;
    PUSH32(esp,0x0020668Du);
    sub_00204540();
    visual_prompt=MEM32(0x8DB248);
    MEM32(0x8DB160)=saved[0];MEM32(0x8DB054)=saved[1];MEM32(0xBCDFDC)=saved[2];
    MEM32(0x8DB248)=saved[3];MEM32(0x8DB6B8)=saved[4];
    ngb_chapter_loading_visual_end();
    eax=regs[0];ecx=regs[1];edx=regs[2];ebx=regs[3];esi=regs[4];edi=regs[5];
    esp=regs[6];g_ebp=regs[7];g_seh_ebp=regs[8];
    xmm0=simd[0];xmm1=simd[1];xmm2=simd[2];xmm3=simd[3];
    xmm4=simd[4];xmm5=simd[5];xmm6=simd[6];xmm7=simd[7];
    memcpy(g_fp_stack,fp,sizeof fp);g_fp_top=top;g_fp_cmp=cmp;
    g_fp_control_word=cw;g_fp_cc=cc;g_df=df;
}
