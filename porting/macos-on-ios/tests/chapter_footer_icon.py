"""Run the actual sprite-submit callsite with a native separate X icon."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3];source=(root/'src/pause_tab.c').read_text()
a=source.index('#ifdef NGB_IOS_KOREAN\n    /* The chapter footer draws');b=source.index('\n#endif',source.index('if(footer_icon)MEM32(p+0x20)=footer_icon_alpha;',a))+7
body=source[a:b]
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros='\n'.join(re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)[0] for name in ['XBOX_PTR','MEM32','MEMF'])
prefix='''#include <stdint.h>
#include <assert.h>
#include "chapter_card_state.h"
static uint8_t mem[4096];static uintptr_t g_xbox_mem_offset;
'''+macros+'''
static uint32_t p=128,verts=256,eax;static float submitted;
static void sprite_submit(void){submitted=MEMF(p+0x20);MEM32(p+8)=42;eax=1;}
static void pt_header_seen(unsigned id,float a,float x,float y,float z,float w){(void)id;(void)a;(void)x;(void)y;(void)z;(void)w;}
#ifdef NGB_IOS_UI_PROMPT_PROBE
static unsigned ui_caller,ui_parent;static float observed;
static void ui_loading_seen(unsigned packet,unsigned v){(void)v;observed=MEMF(packet+0x20);}
static void ui_prompt_seen(unsigned packet,unsigned v,unsigned c,unsigned parent){(void)packet;(void)v;(void)c;(void)parent;}
#endif
static void submit(void){
'''
main='''
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)mem;
 for(unsigned ready=0;ready<2;ready++)for(unsigned id=5;id<=6;id++){
  ngb_chapter_card_start_begin();if(ready)ngb_chapter_card_start_ready();
  ngb_chapter_card_footer_begin(0x1F5AF6,3);MEM32(p)=id;MEMF(p+0x20)=0.8f;
  submit();float expected=(!ready && id==5)?0:0.8f;
#ifndef NGB_IOS_KOREAN
  expected=0.8f;
#endif
  assert(submitted==expected && MEMF(p+0x20)==0.8f && MEM32(p+8)==42 && eax==1);
#ifdef NGB_IOS_UI_PROMPT_PROBE
  assert(observed==expected);
#endif
  ngb_chapter_card_footer_end();ngb_chapter_card_start_end();
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 for flags in [[],['-DNGB_IOS_KOREAN=1'],['-DNGB_IOS_KOREAN=1','-DNGB_IOS_UI_PROMPT_PROBE=1']]:
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror=int-conversion',*flags,'-I'+str(root/'porting/macos-on-ios/korean'),str(p/'test.c'),str(root/'porting/macos-on-ios/korean/chapter_card_state.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('PASS: separate native X sprite hidden only in pending chapter footer; native submission and packet writes retained, opacity restored, probe observes actual hidden state')
