/* Loading-caption UI adapter only; native game assets remain private. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "korean.h"
#include <string.h>
extern unsigned ngb_ios_game_text_language(unsigned saved);

/* Resolve the existing localized loading message through the same native
 * lookup as the footer. Host drawing must not disturb the guest CPU state. */
unsigned ngb_chapter_loading_caption(void) {
    uint32_t regs[]={eax,ecx,edx,ebx,esi,edi,esp,g_ebp,g_seh_ebp};
    RecompXmm simd[]={xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7};
    double fp[8];memcpy(fp,g_fp_stack,sizeof fp);
    int top=g_fp_top,cmp=g_fp_cmp;uint16_t cw=g_fp_control_word,cc=g_fp_cc;
    uint32_t df=g_df;
    unsigned language=ngb_ios_game_text_language(MEM32(0xDB9890));
    PUSH32(esp,language==0?1u:0u);
    PUSH32(esp,0x001F5E5Du); /* the native footer's existing lookup caller */
    eax=0x61u; /* verified native Now Loading caption, not a new translation */
    sub_001E6AE0();
    uint32_t caption=eax;
    eax=regs[0];ecx=regs[1];edx=regs[2];ebx=regs[3];esi=regs[4];edi=regs[5];
    esp=regs[6];g_ebp=regs[7];g_seh_ebp=regs[8];
    xmm0=simd[0];xmm1=simd[1];xmm2=simd[2];xmm3=simd[3];
    xmm4=simd[4];xmm5=simd[5];xmm6=simd[6];xmm7=simd[7];
    memcpy(g_fp_stack,fp,sizeof fp);g_fp_top=top;g_fp_cmp=cmp;
    g_fp_control_word=cw;g_fp_cc=cc;g_df=df;
    return caption>=0x10000 && caption<=0x3FFFFA0 && MEM8(caption)?caption:0;
}
