"""Drive the actual footer-opacity wrapper, retaining its native ABI and layout."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'src/menu_mouse.c').read_text()
a=s.index('#ifdef NGB_IOS_KOREAN\n#include "loading_guide_policy.h"');b=s.index('\n}',s.index('void sub_001E6D30(void)\n{',a))+2
body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "loading_guide_policy.h"
static uint8_t mem[0x1000000];
#define MEM8(a) mem[a]
#define MEM16(a) (*(uint16_t *)(void *)(mem+(a)))
#define MEM32(a) (*(uint32_t *)(void *)(mem+(a)))
#define MEMF(a) (*(float *)(void *)(mem+(a)))
#define XBOX_PTR(a) (mem+(a))
static uint32_t esp,eax;static unsigned draws,translated_draws;static int korean;
static float seen_alpha,record_alpha;
void record_text(uint32_t st,uint32_t caller){assert(caller==0x12345);record_alpha=MEMF(st+0x14);}
static void text_draw(void){uint32_t st=MEM32(esp+4);seen_alpha=MEMF(st+0x14);MEM32(st+8)=42;eax=37;esp+=4;draws++;}
static void translated_draw(void){translated_draws++;text_draw();}
void (*ngb_korean_lookup_raw(unsigned address))(void){assert(address==0x001E6D30);return korean?translated_draw:0;}
'''
main=r'''
static void run(unsigned phase,int x,int y,const char *text,int expect_hide){
 uint32_t st=0x30000;esp=0x20000;MEM32(esp)=0x12345;MEM32(esp+4)=st;
 MEM32(0x8DB160)=phase;MEM32(st+0x10)=0x40000;MEM32(st+0x38)=1;
 MEM16(st+0x44)=(uint16_t)x;MEM16(st+0x46)=(uint16_t)y;MEMF(st+0x14)=0.75f;
 memset(mem+0x40000,0,96);strncpy((char *)mem+0x40000,text,95);
 unsigned old=draws;sub_001E6D30();
 assert(draws==old+1 && MEM32(st+8)==42 && eax==37 && esp==0x20004);
 assert(MEMF(st+0x14)==0.75f); // restore exactly, keep the drawer's other writes
 assert(seen_alpha==(expect_hide?0:0.75f) && record_alpha==seen_alpha);
}
int main(void){
 for(korean=0;korean<2;korean++){
#ifdef NGB_IOS_KOREAN
  run(2,540,440,"@c0@AX next",1);
#else
  run(2,540,440,"@c0@AX next",0);
#endif
  run(3,540,440,"@AX next",0);run(4,540,440,"@AX next",0);
  run(0,540,440,"@AX next",0);run(5,540,440,"@AX next",0);
  run(2,540,440,"loading",0);run(2,540,440,"@AA next",0);
  run(2,60,120,"@AX body",0);run(2,540,320,"@AX upper",0);
  run(2,540,440,"\x81\x40" "AX",0); // a multibyte trail @ is not a command
 }
 uint8_t cut[]={'@','A'};
 assert(!ngb_loading_guide_pending(2,540,440,cut,sizeof cut));
 assert(!ngb_loading_guide_pending(2,540,440,0,96));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 for enabled in (False,True):
  flags=['-DNGB_IOS_KOREAN=1'] if enabled else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-fsanitize=address,undefined',*flags,'-I'+str(root/'porting/macos-on-ios/korean'),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: only phase-2 footer X caption hidden; native/Korean drawers and ABI/layout writes run, opacity restored; other loading text, body, phases and desktop retained')
