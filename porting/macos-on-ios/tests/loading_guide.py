"""Drive the actual footer-opacity wrapper, retaining its native ABI and layout."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'src/menu_mouse.c').read_text()
a=s.index('#ifdef NGB_IOS_KOREAN\n#include "chapter_card_state.h"');b=s.index('\n}',s.index('void sub_001E6D30(void)\n{',a))+2
body=s[a:b]
c=s.index('#ifdef NGB_IOS_KOREAN\n#include "proceed_caption_policy.h"');d=s.index('\n#endif',c)+len('\n#endif');support=s[c:d]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "loading_guide_policy.h"
#include "chapter_card_state.h"
static uint8_t mem[0x1000000];
@GUEST_MEMORY_MACROS@
static uint32_t esp,eax;static unsigned draws,translated_draws;static int korean;
static float seen_alpha,record_alpha;static uint32_t seen_text,seen_font;
int ngb_korean_active(void){return korean;}
unsigned ngb_chapter_loading_caption(void){return 0x50000;}
void record_text(uint32_t st,uint32_t caller){assert(caller==0x12345);record_alpha=MEMF(st+0x14);}
static void text_draw(void){uint32_t st=MEM32(esp+4);seen_alpha=MEMF(st+0x14);seen_text=MEM32(st+0x10);seen_font=MEM32(st+0x38);MEM32(st+8)=42;eax=37;esp+=4;draws++;}
static void translated_draw(void){translated_draws++;text_draw();}
void (*ngb_korean_lookup_raw(unsigned address))(void){assert(address==0x001E6D30);return korean?translated_draw:0;}
'''
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros=[]
for name in ('XBOX_PTR','MEM8','MEM16','MEM32','MEMF'):
 match=re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)
 assert match, name
 macros.append(match[0])
prefix=prefix.replace('@GUEST_MEMORY_MACROS@','static uintptr_t g_xbox_mem_offset;\n'+'\n'.join(macros))
main=r'''
static void run(int locked,unsigned caller,unsigned mode,unsigned font,int x,int y,const char *text){
 uint32_t st=0x30000;esp=0x20000;MEM32(esp)=0x12345;MEM32(esp+4)=st;
 MEM32(st+0x10)=0x40000;MEM32(st+0x38)=font;
 MEM16(st+0x44)=(uint16_t)x;MEM16(st+0x46)=(uint16_t)y;MEMF(st+0x14)=0.75f;
 memset(mem+0x40000,0,96);strncpy((char *)mem+0x40000,text,95);
 ngb_chapter_card_start_begin();if(!locked)ngb_chapter_card_start_ready();
 ngb_chapter_card_update_begin(); // common updater stays locked during chapter setup
 ngb_chapter_card_draw_begin(mode);ngb_chapter_card_footer_begin(caller,mode);
 int replace=locked && mode>=1 && mode<=7 && caller==0x1F5AF6;
#ifndef NGB_IOS_KOREAN
 replace=0;
#endif
 unsigned old=draws;sub_001E6D30();
 assert(draws==old+1 && MEM32(st+8)==42 && eax==37 && esp==0x20004);
 assert(MEMF(st+0x14)==0.75f && seen_alpha==0.75f && record_alpha==seen_alpha);
 assert(seen_text==(replace?0x50000u:0x40000u) && seen_font==(replace && korean?1u:font));
 assert(MEM32(st+0x10)==0x40000 && MEM32(st+0x38)==font);
 ngb_chapter_card_footer_end();ngb_chapter_card_draw_end();ngb_chapter_card_start_end();
 assert(!ngb_chapter_card_hide_footer());
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)mem;
 for(korean=0;korean<2;korean++)for(int locked=0;locked<2;locked++){
  run(locked,0x1F5AF6,3,1,540,440,"@BX next");
  run(locked,0x1F5AF6,3,5,360,0,"localized caption"); // no guessed font/anchor filter
  run(locked,0x204321,3,1,540,440,"@BX other screen");
  run(locked,0x1F5AF6,0,1,540,440,"@BX gameplay");
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+support+'\n'+body+main)
 for enabled in (False,True):
  flags=['-DNGB_IOS_KOREAN=1'] if enabled else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror=int-conversion','-fsanitize=address,undefined',*flags,'-I'+str(root/'porting/macos-on-ios/korean'),'-I'+str(root/'src'),str(p/'test.c'),str(root/'porting/macos-on-ios/korean/chapter_card_state.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: pending chapter footer reuses visible native Loading text independent of font/anchor; descriptor/font/opacity restored; native/Korean ABI and other scenes unchanged')

# Keep the established CI entry point while covering the separate icon and the
# native localized lookup used by the same pending chapter footer.
import runpy
for name in ('chapter_footer_icon.py','chapter_loading_caption.py'):
 runpy.run_path(str(Path(__file__).with_name(name)),run_name='__main__')
