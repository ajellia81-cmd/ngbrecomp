/* Host-only iOS hooks; original generated bodies stay private in the build. */
#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include "korean.h"
#include "launcher.h"
#include "language_policy.h"
extern void ngb_base_001E6AE0(void);
extern void ngb_base_0017E8A0(void);
extern int ngb_pc_setting(int);
extern int ngb_ios_text_override(void);
extern void ngb_pc_set_setting(int,int);

/* The game's saved text enum is Japanese=0, English=1. The public text
 * lookup's pointer-column enum is the reverse. Never change speech settings. */
unsigned ngb_ios_game_text_language(unsigned saved) {
    int selected=ngb_ios_text_override();
    return selected>=0 ? (unsigned)selected : saved;
}
void sub_001E6AE0(void) {
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
