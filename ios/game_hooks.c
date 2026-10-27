#include <stdio.h>
/* Host-only iOS hooks; original generated bodies stay private in the build. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "korean.h"
#include "launcher.h"
#include "language_policy.h"
#include "movie_diagnostics.h"
#include "movie_subtitle_policy.h"
#include "cache_copy_policy.h"
extern void ngb_base_001E6AE0(void);
extern void ngb_base_0017E8A0(void);
extern void ngb_base_00232D40(void);
extern int ngb_pc_setting(int);
extern int ngb_ios_text_override(void);
extern void ngb_pc_set_setting(int,int);

unsigned ngb_ios_cache_copy_delay(unsigned milliseconds,unsigned state) {
    unsigned stage=MEM32(0x8DB160);
    unsigned foreground=MEM8(0xD52F2D) && stage>=1 && stage<=3;
    unsigned wait=ngb_cache_copy_wait(milliseconds,state,foreground);
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
    ngb_movie_diag_cache_wait(milliseconds,wait,state);
#endif
    return wait;
}

/* The game's saved text enum is Japanese=0, English=1. The public text
 * lookup's pointer-column enum is the reverse. Never change speech settings. */
unsigned ngb_ios_game_text_language(unsigned saved) {
    int selected=ngb_ios_text_override();
    return selected>=0 ? (unsigned)selected : saved;
}
void sub_00232D40(void) {
    static NgbMovieSubtitlePolicy policy;
    uint32_t cue=eax,slot=0x8FA960;
    int changed=ngb_movie_subtitle_switch(&policy,
        ngb_korean_active() && MEM32(0x9C2738),MEM32(0x9C2588),MEM32(0x9C25FC),cue);
    // The movie owns caption slot zero in mode six. Its old frame-counted
    // fade can outlast the next timed packet under load. Use the game's own
    // close routine so a new cue does not wait behind or lose to that fade.
    if (changed && MEM16(slot+0x5C)==1 && MEM16(slot+0x62)==6) {
        uint32_t regs[]={eax,ecx,edx,ebx,esi,edi,esp,g_ebp,g_seh_ebp};
        RecompXmm simd[]={xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7};
        double fp[8];memcpy(fp,g_fp_stack,sizeof fp);
        int top=g_fp_top,cmp=g_fp_cmp;uint16_t cw=g_fp_control_word,cc=g_fp_cc;
        uint32_t caller=MEM32(esp);
        eax=0;PUSH32(esp,caller);sub_00231EB0();
        eax=regs[0];ecx=regs[1];edx=regs[2];ebx=regs[3];esi=regs[4];edi=regs[5];
        esp=regs[6];g_ebp=regs[7];g_seh_ebp=regs[8];
        xmm0=simd[0];xmm1=simd[1];xmm2=simd[2];xmm3=simd[3];
        xmm4=simd[4];xmm5=simd[5];xmm6=simd[6];xmm7=simd[7];
        memcpy(g_fp_stack,fp,sizeof fp);g_fp_top=top;g_fp_cmp=cmp;g_fp_control_word=cw;g_fp_cc=cc;
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
        ngb_movie_diag_emit("replace",cue,MEM16(slot+0x5C),MEM32(0x9C25FC),0);
#endif
    }
    ngb_base_00232D40();
}
void sub_001E6AE0(void) {
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
    uint32_t message=eax;
#endif
    uint32_t start=esp,caller=MEM32(esp),saved=MEM32(esp+4);
    // Only the text-language VALUE call in the original General drawer.
    if (caller==0x0021CBD0u && ngb_pc_setting(25)==2) {
        uint32_t table=MEM32(0x436248),count=MEM16(0x436270);
        for (uint32_t i=0;i<count;i++) if (MEM32(table+i*12)==0x20212u) {
            eax=MEM32(table+i*12+4); esp+=4; return;
        }
    }
    int selected=ngb_ios_text_override();
    if (selected>=0) MEM32(esp+4)=selected==0 ? 1 : 0; // patched Japanese text/font column
    ngb_korean_fn translated=ngb_korean_lookup_raw(0x001E6AE0u);
    if (translated) translated(); else ngb_base_001E6AE0();
#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
    if (MEM32(0x9C2738) && (message>>16)==6)
        ngb_movie_diag_emit("lookup",message,eax,saved,eax ? MEM8(eax) : 0);
#endif
    MEM32(start+4)=saved;
}
void sub_0017E8A0(void) {
    uint32_t state=MEM32(esp+4);
    int step=(int8_t)MEM8(state+0x10);
    if (MEM8(state)==1 && MEM8(state+1)==1 && step &&
        !(MEM32(state+0x18)&0x7FFFFFFFu) && !MEM8(state+0xD) && MEM8(state+0xC)==0xFF) {
        int old=ngb_pc_setting(25)==2 ? 2 : (MEM32(0xDA4978) ? 1 : 0);
        int selected=ngb_language_step(old,step);
        ngb_pc_set_setting(25,selected);
        // Let the original handler apply a valid Japanese/English value. Korean
        // is a host overlay boot, never an out-of-range index in the Xbox game.
        int target=(int)ngb_language_game_index(selected);
        MEM8(state+0x10)=(uint8_t)(target-(int)MEM32(0xDA4978));
    }
    ngb_korean_fn translated=ngb_korean_lookup_raw(0x0017E8A0u);
    if (translated) translated(); else ngb_base_0017E8A0();
}


void ngb_korean_font_metrics(unsigned font,unsigned width,unsigned height,float sx,float sy) {
    static unsigned seen;
    if (font>2 || (__atomic_fetch_or(&seen,1u<<font,__ATOMIC_RELAXED)&(1u<<font))) return;
    fprintf(stderr,"[Korean metrics] font %u; cell %ux%u; scale %.3f,%.3f; Korean font-bank ASCII/full-width spaces use quarter-cell advance\n",font,width,height,(double)sx,(double)sy);
}

#ifdef NGB_IOS_MOVIE_DIAGNOSTICS
extern void ngb_movie_base_text_draw(void);
extern void ngb_movie_base_glyph(float,float,float,float,float,uint32_t);
extern void ngb_movie_base_sprite_submit(void);
extern uint32_t ngb_sprite_def(uint32_t);
void sub_002216B0(void) {
    uint32_t packet=eax,verts=esi;
    ngb_movie_base_sprite_submit();
    if (!eax) return;
    uint32_t id=MEM32(packet)&0xFFFF;
    if (id>=2048 || MEMF(verts+28)-MEMF(verts)<48 || MEMF(verts+60)-MEMF(verts+4)<8) return;
    static uint32_t last[2048];static unsigned char seen[2048];
    uint32_t frame=MEM32(0xD21734);
    if (seen[id] && frame-last[id]<60) return;
    uint32_t def=ngb_sprite_def(id);if (!def) return;
    seen[id]=1;last[id]=frame;
    ngb_movie_diag_emit("ui-sprite",id,MEM32(def+0x10),MEM32(def+0x14),MEM32(packet));
    ngb_movie_diag_emit("ui-rect",MEM32(verts),MEM32(verts+4),MEM32(verts+28),MEM32(verts+60));
    ngb_movie_diag_emit("ui-uv",MEM32(verts+20),MEM32(verts+24),MEM32(verts+48),MEM32(verts+80));
    ngb_movie_diag_emit("ui-window",MEM32(def),MEM32(def+4),MEM32(def+8),MEM32(def+12));
}
void sub_001E6D30(void) {
    ngb_ios_movie_draw(MEM32(esp+4),MEM32(esp));
    ngb_movie_base_text_draw();
}
void ngb_menu_glyph(float x0,float y0,float x1,float y1,float bright,uint32_t z) {
    ngb_movie_diag_glyph();
    ngb_movie_base_glyph(x0,y0,x1,y1,bright,z);
}
void ngb_ios_movie_probe(unsigned stage) {
    static int playing;
    int active=MEM32(0x9C2738)!=0;
    if (stage==1) {
        if (playing!=active) ngb_movie_diag_emit(active ? "begin" : "end",
            MEM32(0x9C2588),MEM32(0xDB9890),ngb_ios_game_text_language(MEM32(0xDB9890)),ngb_korean_active());
        playing=active;
    }
    if (!active) return;
    if (stage==1) {
        ngb_movie_diag_emit("tick",MEM32(0x9C25FC),MEM32(0x9C2730),MEM32(0x9C271C),MEM32(0x9C2600));
    } else if (stage==2) {
        ngb_movie_diag_emit("cue",eax,esi,ebx,MEM32(0x9C25FC));
    } else if (stage==3) {
        ngb_movie_diag_emit("enqueue",eax,ngb_ios_game_text_language(MEM32(0xDB9890)),MEM32(0x9776A8),MEM32(0x9C25FC));
    } else if (stage==4) {
        ngb_movie_diag_emit("userdata",eax,MEM32(0x9C271C),MEM32(0x9C2720),MEM32(0x9C2724));
        if (MEM8(0x9C2600)) {
            // Bounded raw ASCII control packets, never localized text bytes.
            for (unsigned i=0;i<64 && MEM8(0x9C2600+i);i+=16)
                ngb_movie_diag_emit("packet",MEM32(0x9C2600+i),MEM32(0x9C2604+i),MEM32(0x9C2608+i),MEM32(0x9C260C+i));
        }
    }
}
void ngb_ios_movie_draw(unsigned state,unsigned caller) {
    if (!MEM32(0x9C2738)) return;
    ngb_movie_diag_emit("draw",MEM32(state+0x10),MEM32(state+0x38),caller,MEM32(state+0x20));
    ngb_movie_diag_emit("position",MEM32(state+0x3C),MEM32(state+0x40),MEM32(state+0x44),MEM32(state+0x48));
}
#endif
