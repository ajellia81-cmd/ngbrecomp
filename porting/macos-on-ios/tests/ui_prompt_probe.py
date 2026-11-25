"""Exercise the real UI collector, boot-file writer, and guest context snapshot."""
from pathlib import Path
import os,subprocess,tempfile,json
root=Path(__file__).resolve().parents[3];s=(root/'src/pause_tab.c').read_text();a=s.index('static void ui_prompt_seen(');b=s.index('\n}',a)+2;body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "ui_prompt_probe.h"
static uint8_t mem[0x1000000];
static uint32_t esp,g_ebp,eax,esi;
void ngb_aspect_tick(void){}
void sprite_submit(void){esp+=4;g_ebp=0x50000;}
#define MEM16(a) (*(uint16_t *)(void *)(mem+(a)))
#define MEM32(a) (*(uint32_t *)(void *)(mem+(a)))
#define MEMF(a) (*(float *)(void *)(mem+(a)))
#define GLYPH_MAP 0x430C48
static uint32_t sprite_def(uint32_t id){return id?0x3000:0;}
static unsigned records,counts[3];static NgbUiPromptRecord last;
void ngb_ui_prompt_count(unsigned n){assert(n<3);counts[n]++;}
void ngb_ui_prompt_record(const NgbUiPromptRecord *r){assert(r->tw==128 && r->th==128);records++;last=*r;}
'''
start=s.index('void sub_002216B0(void)\n{');end=s.index('    if (ngb_font_tex_n',start)
snapshot=s[start:end]+r'''
 sprite_submit();assert(ui_caller==0x111111 && ui_parent==0x222222);
 (void)p;(void)verts;(void)def;
}
'''
assert 'ui_loading_seen(p,verts);ui_prompt_seen(p,verts,ui_caller,ui_parent);' in s
main=r'''
int main(void){
 esp=0x5000;g_ebp=0x40000;MEM32(esp)=0x111111;MEM32(g_ebp+4)=0x222222;sub_002216B0();
 uint32_t packet=0x1000,verts=0x2000;
 MEM32(packet)=64;MEM32(0x3010)=7;MEM32(GLYPH_MAP+4*'P')=64;
 MEM32(0x8DBB28+7*8)=0x4000;MEM32(0x400C)=(7u<<20)|(7u<<24);MEM32(0xDA41C8)=2;
 MEMF(verts)=100;MEMF(verts+4)=100;MEMF(verts+28)=120;MEMF(verts+60)=120;
 ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==1 && last.sample && last.letter=='P');
 for(int i=0;i<1000;i++){ui_prompt_seen(packet,verts,0x12345,0x23456);}
 assert(records==1 && counts[0]>1000);
 for(unsigned i=1;i<40;i++){MEM32(packet)=i;ui_prompt_seen(packet,verts,0x12345,0x23456);}
 assert(records==16);
 MEM32(packet)=900;MEMF(verts)=NAN;MEM16(packet+4)=550;MEM16(packet+6)=430;
 ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==17 && !last.sample && last.input_x==550);
 MEM32(packet)=901;MEM16(packet+4)=0;MEM16(packet+6)=0;
 MEMF(verts)=500;MEMF(verts+4)=400;MEMF(verts+28)=520;MEMF(verts+60)=420;
 ui_prompt_seen(packet,verts,0x12345,0x23456);assert(records==18 && last.caller==0x12345 && last.parent==0x23456);
 for(int i=1000;i<1700;i++){MEM32(packet)=i;ui_prompt_seen(packet,verts,0x12345,0x23456);}
 assert(records==512);
 return 0;
}
'''
writer=r'''
#include "ui_prompt_probe.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <cstring>
int main(int argc,char **argv){
 assert(argc==2 && ngb_ui_prompt_start(argv[1]));
 FILE *f=std::fopen(argv[1],"r");assert(f);char first[256];assert(std::fgets(first,sizeof first,f));std::fclose(f);
 bool heartbeat=false;
 for(int attempt=0;attempt<500 && !heartbeat;attempt++){
   FILE *check=std::fopen(argv[1],"r");char line[512];
   while(check && std::fgets(line,sizeof line,check))if(std::strstr(line,"heartbeat"))heartbeat=true;
   if(check)std::fclose(check);
   if(!heartbeat)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 assert(heartbeat); // no sprite: header + liveness must still exist
 for(int i=0;i<5;i++)ngb_ui_prompt_count(0);
 NgbUiPromptRecord r{};r.sprite=123;r.caller=0x12345;r.parent=0x23456;r.rect[0]=NAN;
 for(int i=0;i<3;i++)ngb_ui_prompt_record(&r);
 r.menu=1;r.tab=5;r.visible=0;r.header_visible=0;r.state_flags=2;ngb_ui_prompt_record(&r);
 r.menu=0;r.loading=1;r.phase=3;r.guide=1;r.host_buttons=1u<<20;r.guest[1]=5;r.wall_ms=1234;ngb_ui_prompt_record(&r);
 ngb_ui_prompt_stop();return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+body+snapshot+main)
 flags=['-I'+str(root/'porting/macos-on-ios')]
 subprocess.run([os.environ.get('CC','cc'),'-DNGB_IOS_UI_PROMPT_PROBE=1','-std=c11','-Wall','-Wextra','-fsanitize=address,undefined',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 (p/'writer.cpp').write_text(writer)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-pthread',*flags,str(p/'writer.cpp'),str(root/'porting/macos-on-ios/ui_prompt_probe.cpp'),'-o',str(p/'writer')],check=True)
 log=p/'ui.log';subprocess.run([str(p/'writer'),str(log)],check=True)
 rows=[json.loads(line) for line in log.read_text().splitlines()]
 assert rows[0]['session'] and rows[0]['format']==2 and not rows[0]['performance_observers']
 beats=[r for r in rows if r.get('kind')=='heartbeat'];assert any(r['visits']==0 for r in beats)
 assert beats[-1]['visits']==5 and beats[-1]['records']==5
 records=[r for r in rows if r.get('kind')=='sprite'];assert len(records)==3 and records[0]['rect'][0]==0
 menus=[r for r in rows if r.get('kind')=='menu'];assert len(menus)==1 and menus[0]['tab']==5 and menus[0]['state_flags']==2
 loading=[r for r in rows if r.get('kind')=='loading'];assert len(loading)==1 and loading[0]['phase']==3 and loading[0]['guide_seen']==1 and loading[0]['guest'][1]==5 and loading[0]['ms']==1234
 assert records[0]['caller']==0x12345 and records[0]['parent']==0x23456
print('PASS: immediate boot log with zero sprites; heartbeat, real writer/drain/JSON, input-coordinate fallback, early samples, hard limit and guest caller snapshot')
