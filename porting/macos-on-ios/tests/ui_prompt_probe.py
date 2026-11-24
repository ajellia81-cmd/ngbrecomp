"""Drive the actual bounded UI observer with synthetic guest memory."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'src/pause_tab.c').read_text();a=s.index('static void ui_prompt_seen(');b=s.index('\n}',a)+2;body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <math.h>
static uint8_t mem[0x1000000];
static uint32_t esp,g_ebp,eax,esi;
void ngb_aspect_tick(void){}
void sprite_submit(void){esp+=4;g_ebp=0x50000;}
#define MEM8(a) mem[(a)]
#define MEM32(a) (*(uint32_t *)(void *)(mem+(a)))
#define MEMF(a) (*(float *)(void *)(mem+(a)))
#define GLYPH_MAP 0x430C48
static uint32_t sprite_def(uint32_t id){return id?0x3000:0;}
static int records,last_letter;
void ngb_ui_prompt_record(uint32_t frame,uint32_t sprite,uint32_t texture,int letter,
 int tw,int th,float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1,uint32_t caller,uint32_t parent){
 (void)caller;(void)parent;(void)frame;(void)sprite;(void)texture;(void)x0;(void)y0;(void)x1;(void)y1;
 (void)u0;(void)v0;(void)u1;(void)v1;assert(tw==128 && th==128);records++;last_letter=letter;
}
'''
start=s.index('void sub_002216B0(void)\n{');end=s.index('    if (ngb_font_tex_n',start)
snapshot=s[start:end]+r'''
 sprite_submit();assert(ui_caller==0x111111 && ui_parent==0x222222);
 (void)p;(void)verts;(void)def;
}
'''
assert 'if(eax) ui_prompt_seen(p,verts,ui_caller,ui_parent);' in s
main=r''' 
int main(void){
 esp=0x5000;g_ebp=0x40000;MEM32(esp)=0x111111;MEM32(g_ebp+4)=0x222222;sub_002216B0();
 uint32_t packet=0x1000,verts=0x2000;
 MEM32(packet)=64;MEM32(0x3010)=7;MEM32(GLYPH_MAP+4*'P')=64;
 MEM32(0x8DBB28+7*8)=0x4000;MEM32(0x8DBB28+8*8)=0x4000;MEM32(0x400C)=(7u<<20)|(7u<<24);
 MEMF(verts)=100;MEMF(verts+4)=430;MEMF(verts+28)=120;MEMF(verts+60)=450;
 ui_prompt_seen(packet,verts,0x12345,0x23456);assert(!records);
 MEMF(verts)=500;MEMF(verts+28)=520;
 ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==1 && last_letter=='P');
 for(int i=0;i<1000;i++){ui_prompt_seen(packet,verts,0x12345,0x23456);}
 assert(records==1);
 MEM32(0x3010)=8;ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==2);
 MEMF(verts)=NAN;ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==2);
 MEMF(verts)=500;MEMF(verts+60)=520;ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==2);
 MEMF(verts+60)=450;
 for(int i=1;i<700;i++){MEM32(packet)=i;ui_prompt_seen(packet,verts,0x12345,0x23456);}
 assert(records==512);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+body+snapshot+main)
 subprocess.run([os.environ.get('CC','cc'),'-DNGB_IOS_UI_PROMPT_PROBE=1','-std=c11','-Wall','-Wextra','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: footer-only UI metadata, deduplication, ASCII identity, texture changes, invalid bounds and hard 512-record limit')
