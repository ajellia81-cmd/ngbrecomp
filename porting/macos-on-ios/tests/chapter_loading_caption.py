"""Run the actual original-loading visual adapter without modifying load control."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'porting/macos-on-ios/korean/chapter_loading_caption.c').read_text();a=s.index('void ngb_chapter_loading_visual(void) {');body=s[a:s.index('\n}',a)+2]
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros='\n'.join(re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)[0] for name in ['XBOX_PTR','MEM8','MEM32','MEMF'])
push=re.search(r'^#define PUSH32\(sp, val\).*?\} while\(0\)',runtime,re.M|re.S)[0]
xmm=re.search(r'typedef union RecompXmm \{.*?\} RecompXmm;',runtime,re.S)[0]
prefix='''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "chapter_card_state.h"
static uint8_t mem[0x1000000];static uintptr_t g_xbox_mem_offset;
'''+macros+'\n'+push+'\n'+xmm+'''
static uint32_t eax,ecx,edx,ebx,esi,edi,esp,g_ebp,g_seh_ebp,g_df;
static RecompXmm xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7;
static double g_fp_stack[8];static int g_fp_top,g_fp_cmp;
static uint16_t g_fp_control_word,g_fp_cc;
static unsigned calls,expected_tick,expected_prompt;
void sub_00204540(void){
 assert(eax==2 && MEM32(esp)==0x20668D && MEM32(0x8DB160)==2 && MEMF(0x8DB054)==1);
 assert(MEM32(0xBCDFDC)==expected_tick && MEM32(0x8DB248)==expected_prompt && !MEM32(0x8DB6B8));
 assert(ngb_chapter_loading_visual_active() && (ngb_chapter_card_route()&16));
 assert(!ngb_chapter_loading_control_phase(MEM32(0x8DB160)));
 MEM32(0x8DB248)=777;MEM32(0x8DB1CC)=42; // renderer-owned state is retained
 eax=ecx=edx=ebx=esi=edi=g_ebp=g_seh_ebp=0x99999999;esp+=4;
 memset(&xmm0,0,sizeof xmm0);xmm1=xmm2=xmm3=xmm4=xmm5=xmm6=xmm7=xmm0;
 memset(g_fp_stack,0,sizeof g_fp_stack);g_fp_top=7;g_fp_cmp=9;g_fp_control_word=0;g_fp_cc=0;g_df=0;calls++;
}
'''
main='''
static void draw_preserving_state(void){
 uint32_t controls[]={MEM32(0x8DB160),MEM32(0x8DB054),MEM32(0xBCDFDC),MEM32(0x8DB248),MEM32(0x8DB6B8)};
 eax=11;ecx=12;edx=13;ebx=14;esi=15;edi=16;esp=0x20000;g_ebp=17;g_seh_ebp=18;g_df=1;MEM32(esp)=0xabcdef;
 memset(&xmm0,0x53,sizeof xmm0);xmm1=xmm2=xmm3=xmm4=xmm5=xmm6=xmm7=xmm0;
 RecompXmm before=xmm0;double fp[8];for(int i=0;i<8;i++)fp[i]=g_fp_stack[i]=i+0.25;
 g_fp_top=3;g_fp_cmp=-1;g_fp_control_word=0x37f;g_fp_cc=0x140;
 ngb_chapter_loading_visual();
 assert(eax==11&&ecx==12&&edx==13&&ebx==14&&esi==15&&edi==16&&esp==0x20000&&g_ebp==17&&g_seh_ebp==18&&g_df==1);
 assert(MEM32(esp)==0xabcdef && !memcmp(fp,g_fp_stack,sizeof fp));
 RecompXmm regs[]={xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7};
 for(int i=0;i<8;i++)assert(!memcmp(&regs[i],&before,sizeof before));
 assert(g_fp_top==3&&g_fp_cmp==-1&&g_fp_control_word==0x37f&&g_fp_cc==0x140);
 uint32_t after[]={MEM32(0x8DB160),MEM32(0x8DB054),MEM32(0xBCDFDC),MEM32(0x8DB248),MEM32(0x8DB6B8)};
 assert(!memcmp(controls,after,sizeof controls) && !ngb_chapter_loading_visual_active());
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)mem;MEM32(0x8DB028)=3;
 draw_preserving_state();assert(!calls); // no chapter preparation, no effect
 ngb_chapter_card_start_begin();
 for(unsigned phase=1;phase<=4;phase++){
  MEM32(0x8DB160)=phase;MEM32(0xD21734)=100;MEM32(0xBCDFDC)=180;MEM32(0x8DB248)=20;
  draw_preserving_state();assert(!calls); // native loading is never duplicated
 }
 MEM32(0x8DB160)=0;MEM32(0xD21734)=123;MEM32(0xBCDFDC)=0;MEM32(0x8DB248)=0;MEM32(0x8DB6B8)=17;
 expected_tick=203;expected_prompt=20;draw_preserving_state();assert(calls==1 && MEM32(0x8DB1CC)==42);
 MEM32(0xD21734)=124;expected_tick=204;expected_prompt=777;draw_preserving_state();assert(calls==2);
 ngb_chapter_card_start_ready();draw_preserving_state();assert(calls==2);ngb_chapter_card_start_end();
 ngb_chapter_card_start_begin();
 for(unsigned mode=0;mode<=7;mode++)if(mode!=2 && mode!=3){MEM32(0x8DB028)=mode;draw_preserving_state();assert(calls==2);}
 MEM32(0x8DB028)=2;expected_tick=200;expected_prompt=0;draw_preserving_state();assert(calls==3);
 ngb_chapter_card_start_end();draw_preserving_state();assert(calls==3);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'porting/macos-on-ios/korean'),str(p/'test.c'),str(root/'porting/macos-on-ios/korean/chapter_card_state.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: original busy drawer only during extra chapter preparation; clock continues, control/GP/SIMD/x87/DF/stack restored, native drawing retained, other loading and ready state untouched')
