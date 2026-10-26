"""Replay overlapping cue arrivals through the actual host subtitle wrapper."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'ios/game_hooks.c').read_text()
wrapper=s[s.index('void sub_00232D40(void) {'):s.index('void sub_001E6AE0(void) {')]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "movie_subtitle_policy.h"
static unsigned char ram[16*1024*1024];
#define MEM32(a) (*(uint32_t *)(ram+(a)))
#define MEM16(a) (*(uint16_t *)(ram+(a)))
#define PUSH32(s,v) ((s)-=4,MEM32(s)=(v))
typedef struct { uint32_t words[4]; } RecompXmm;
static uint32_t eax,ecx=2,edx=3,ebx=4,esi=5,edi=6,esp,g_ebp=8,g_seh_ebp=9;
static RecompXmm xmm0={{1}},xmm1={{2}},xmm2={{3}},xmm3={{4}},xmm4={{5}},xmm5={{6}},xmm6={{7}},xmm7={{8}};
static double g_fp_stack[8]={1,2,3,4,5,6,7,8};
static int g_fp_top=3,g_fp_cmp=7;
static uint16_t g_fp_control_word=0x37f,g_fp_cc=0x100;
static int korean=1,closed,accepted;
static unsigned displayed;
int ngb_korean_active(void) { return korean; }
void sub_00231EB0(void) {
 assert(eax==0);closed++;MEM16(0x8FA960+0x5C)=0;
 eax=ecx=edx=ebx=esi=edi=esp=g_ebp=g_seh_ebp=0;
 memset(&xmm0,0,sizeof xmm0);memset(&xmm1,0,sizeof xmm1);
 memset(g_fp_stack,0,sizeof g_fp_stack);g_fp_top=g_fp_cmp=g_fp_control_word=g_fp_cc=0;
}
void ngb_base_00232D40(void) {
 assert(ecx==2 && edx==3 && ebx==4 && esi==5 && edi==6 && esp==4096 && g_ebp==8 && g_seh_ebp==9);
 assert(xmm0.words[0]==1 && xmm1.words[0]==2 && xmm7.words[0]==8);
 assert(g_fp_stack[7]==8 && g_fp_top==3 && g_fp_cmp==7 && g_fp_control_word==0x37f && g_fp_cc==0x100);
 if (!MEM16(0x8FA960+0x5C)) { displayed=eax;accepted++;MEM16(0x8FA960+0x5C)=1; }
}
'''
main=r'''
static void cue(unsigned id,unsigned frame) { eax=id;esp=4096;MEM32(esp)=0x1234;MEM32(0x9C25FC)=frame;sub_00232D40(); }
int main(void) {
 MEM32(0x9C2738)=1;MEM32(0x9C2588)=1;MEM16(0x8FA960+0x62)=6;
 cue(100,0);cue(100,1);assert(accepted==1 && closed==0);
 cue(101,2);assert(displayed==101 && closed==1);
 cue(101,3);assert(accepted==2 && closed==1);
 cue(102,4);assert(displayed==102 && closed==2);
 MEM16(0x8FA960+0x62)=7;cue(103,5);assert(closed==2);
 MEM16(0x8FA960+0x62)=6;korean=0;cue(104,6);assert(closed==2);
 korean=1;MEM16(0x8FA960+0x5C)=0;cue(105,7);assert(displayed==105 && closed==2);
 cue(680,8);assert(closed==2);
 MEM32(0x9C2738)=0;cue(106,9);assert(closed==2);
 MEM32(0x9C2738)=1;MEM32(0x9C2588)=2;MEM16(0x8FA960+0x5C)=0;
 cue(100,0);assert(displayed==100 && closed==2);
 cue(101,1);assert(displayed==101 && closed==3);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory=Path(directory)
    for name,body,success in [('fixed',wrapper,True),('old','void sub_00232D40(void) { ngb_base_00232D40(); }',False)]:
        path=directory/(name+'.c');exe=directory/name;path.write_text(prefix+body+main)
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-I'+str(ROOT/'ios'),str(path),'-o',str(exe)],check=True)
        result=subprocess.run([str(exe)],capture_output=True)
        assert (result.returncode==0)==success
print('Movie subtitles: overlapping cues replaced once; repeats, other UI, other languages and guest registers preserved; old behavior rejected')
