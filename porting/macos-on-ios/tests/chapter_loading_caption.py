"""Native localized lookup must preserve all guest registers and stack state."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'porting/macos-on-ios/korean/chapter_loading_caption.c').read_text();a=s.index('unsigned ngb_chapter_loading_caption(void) {');body=s[a:s.index('\n}',a)+2]
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros='\n'.join(re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)[0] for name in ['XBOX_PTR','MEM8','MEM32'])
push=re.search(r'^#define PUSH32\(sp, val\).*?\} while\(0\)',runtime,re.M|re.S)[0]
xmm=re.search(r'typedef union RecompXmm \{.*?\} RecompXmm;',runtime,re.S)[0]
prefix='''#include <stdint.h>
#include <string.h>
#include <assert.h>
static uint8_t mem[0x1000000];static uintptr_t g_xbox_mem_offset;
'''+macros+'\n'+push+'\n'+xmm+'''
static uint32_t eax,ecx,edx,ebx,esi,edi,esp,g_ebp,g_seh_ebp,g_df;
static RecompXmm xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
static double g_fp_stack[8];static int g_fp_top,g_fp_cmp;
static uint16_t g_fp_control_word,g_fp_cc;
static unsigned selected,caption,calls;
unsigned ngb_ios_game_text_language(unsigned saved){assert(saved==1);return selected;}
void sub_001E6AE0(void){
 assert(eax==0x61 && MEM32(esp)==0x1F5E5D && MEM32(esp+4)==(selected==0?1u:0u));
 eax=caption;ecx=edx=ebx=esi=edi=g_ebp=g_seh_ebp=0x99999999;esp+=8;
 memset(&xmm0,0,sizeof xmm0);xmm1=xmm2=xmm3=xmm4=xmm5=xmm6=xmm7=xmm0;
 memset(g_fp_stack,0,sizeof g_fp_stack);g_fp_top=7;g_fp_cmp=9;g_fp_control_word=0;g_fp_cc=0;g_df=0;calls++;
}
'''
main='''
int main(void){
 g_xbox_mem_offset=(uintptr_t)mem;MEM32(0xDB9890)=1;
 for(selected=0;selected<2;selected++)for(unsigned invalid=0;invalid<4;invalid++){
  caption=invalid==0?0x50000:invalid==1?0x8000:invalid==2?0x4000000:0x50100;
  MEM8(0x50000)=0x81;MEM8(0x50100)=0;
  eax=11;ecx=12;edx=13;ebx=14;esi=15;edi=16;esp=0x20000;g_ebp=17;g_seh_ebp=18;g_df=1;
  MEM32(esp)=0xabcdef;
  memset(&xmm0,0x53,sizeof xmm0);xmm1=xmm2=xmm3=xmm4=xmm5=xmm6=xmm7=xmm0;
  RecompXmm before=xmm0;double fp[8];for(int i=0;i<8;i++)fp[i]=g_fp_stack[i]=i+0.25;
  g_fp_top=3;g_fp_cmp=-1;g_fp_control_word=0x37f;g_fp_cc=0x140;
  unsigned result=ngb_chapter_loading_caption();assert(result==(invalid?0:caption));
  assert(eax==11&&ecx==12&&edx==13&&ebx==14&&esi==15&&edi==16&&esp==0x20000&&g_ebp==17&&g_seh_ebp==18&&g_df==1);
  assert(MEM32(esp)==0xabcdef && !memcmp(fp,g_fp_stack,sizeof fp));
  RecompXmm regs[]={xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7};
  for(int i=0;i<8;i++)assert(!memcmp(&regs[i],&before,sizeof before));
  assert(g_fp_top==3&&g_fp_cmp==-1&&g_fp_control_word==0x37f&&g_fp_cc==0x140);
 }
 assert(calls==8);return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: native 0x61 localized lookup with actual guest push/memory/XMM definitions preserves GP/SIMD/x87/DF and live stack, validates returned pointer')
