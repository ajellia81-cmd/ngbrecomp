"""Exercise actual localized Proceed swaps in both measurement and drawing."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'src/menu_mouse.c').read_text()
a=s.index('#ifdef NGB_IOS_KOREAN\n#include "proceed_caption_policy.h"');b=s.index('\n#endif',a)+len('\n#endif');support=s[a:b]
def function(name):
 a=s.index('void '+name+'(void)\n{');return s[a:s.index('\n}',a)+2]
a=s.index('#ifdef NGB_IOS_KOREAN\n#include "chapter_card_state.h"');b=s.index('\n#endif',a)+len('\n#endif');guide=s[a:b]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "chapter_card_state.h"
static uint8_t mem[0x1000000];
@GUEST_MEMORY_MACROS@
static uint32_t esp,eax,seen_font,seen_text;static int active,callbacks;
static uint64_t s_text_ms,s_text_since;
int ngb_korean_active(void){return active;}
unsigned ngb_chapter_loading_caption(void){return 0x60000;}
static uint64_t now_ms(void){return 1000;}
void ngb_menu_mark(void){}
void ngb_menu_font_init(void){}
void record_text(uint32_t st,uint32_t caller){(void)st;(void)caller;}
static void observe(uint32_t st){seen_font=MEM32(st+0x38);seen_text=MEM32(st+0x10);assert(MEMF(st+0x14)==0.75f);}
static void menu_text(void){uint32_t st=MEM32(esp+8),rect=MEM32(esp+4);observe(st);MEM32(rect)=seen_font==1?40:86;eax=19;esp+=4;}
static void text_draw(void){uint32_t st=MEM32(esp+4);observe(st);MEM32(st+8)=42;eax=37;esp+=4;}
static void kr_measure(void){menu_text();}
static void kr_draw(void){text_draw();}
void (*ngb_korean_lookup_raw(unsigned address))(void){return !callbacks?0:address==0x001E86A0?kr_measure:kr_draw;}
'''
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros=[]
for name in ('XBOX_PTR','MEM8','MEM16','MEM32','MEMF'):
 match=re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)
 assert match, name
 macros.append(match[0])
prefix=prefix.replace('@GUEST_MEMORY_MACROS@','static uintptr_t g_xbox_mem_offset;\n'+'\n'.join(macros))
main=r'''
static void run(const char *word,unsigned font,int x,int y,int expected){
 uint32_t st=0x30000,original=0x40000;memset(mem+st,0,0x60);memset(mem+original,0,32);
 strcpy((char *)mem+original,word);MEM32(st+0x10)=original;MEM32(st+0x38)=font;
 MEM16(st+0x44)=x;MEM16(st+0x46)=y;MEMF(st+0x14)=0.75f;MEM32(0x8DB160)=3;
 for(int measure=0;measure<2;measure++){
  esp=0x20000;MEM32(esp)=0x12345;
  MEM32(esp+4)=measure?0x31000:st;MEM32(esp+8)=st;
  if(measure)sub_001E86A0();else sub_001E6D30();
  assert(esp==0x20004 && eax==(measure?19:37));
  assert(MEM32(st+0x10)==original && MEM32(st+0x38)==font && MEMF(st+0x14)==0.75f);
  assert(seen_text==(expected==3?0x60000:expected?(expected==1?0x50000:0x50010):original));
  assert(seen_font==(expected==3?(active?1:font):expected?1:font));
  if(!measure)assert(MEM32(st+8)==42);
 }
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)mem;
 MEM32(0x431B3C)=0x60;MEM32(0x431B40)=0x50000;
 MEM32(0x431A34)=0x4A;MEM32(0x431A38)=0x50010;
 memcpy(mem+0x50000,"\x91\x41\x91\x42",5);memcpy(mem+0x50010,"@BX \x91\x41\x91\x42",9);
 for(callbacks=0;callbacks<2;callbacks++)for(active=0;active<2;active++){
#ifdef NGB_IOS_KOREAN
  int expected=active;
#else
  int expected=0;
#endif
  run("Proceed",0,565,398,expected);run("Proceed",1,565,398,expected);run("@BX Proceed",0,540,440,expected?2:0);
  run("Proceed with care",0,565,398,0);run("proceed",0,565,398,0);
  run("Proceed",0,60,120,0);run("Proceed",3,565,398,0);
  MEM32(0x431B3C)=99;run("Proceed",0,565,398,0);MEM32(0x431B3C)=0x60;
  ngb_chapter_card_start_begin();ngb_chapter_card_draw_begin(3);ngb_chapter_card_footer_begin(0x1F5AF6,3);
#ifdef NGB_IOS_KOREAN
  run("native progress",5,360,0,3); // both measure and draw must select Loading
#else
  run("native progress",5,360,0,0);
#endif
  ngb_chapter_card_footer_end();ngb_chapter_card_draw_end();ngb_chapter_card_start_end();
  MEM32(0x431B40)=0x4000000;run("Proceed",0,565,398,0);MEM32(0x431B40)=0x50000;
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+support+'\n'+function('sub_001E86A0')+'\n'+guide+'\n'+function('sub_001E6D30')+main)
 for enabled in (False,True):
  flags=['-DNGB_IOS_KOREAN=1'] if enabled else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror=int-conversion','-fsanitize=address,undefined',*flags,'-I'+str(root/'src'),'-I'+str(root/'porting/macos-on-ios/korean'),str(p/'test.c'),str(root/'porting/macos-on-ios/korean/chapter_card_state.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: real measure/draw wrappers reuse native localized captions/font, restore descriptors, preserve ABI/layout; language, text/context, table and pointer guards checked')
