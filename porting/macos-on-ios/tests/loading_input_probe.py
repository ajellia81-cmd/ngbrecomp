"""Exercise actual loading observer: state/guide/button edges and bounded reads."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'src/pause_tab.c').read_text()
a=s.index('static void ui_loading_seen(');b=s.index('\n}',a)+2
body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "ui_prompt_probe.h"
static uint8_t mem[0x1000000];
#define MEM8(a) mem[a]
#define MEM32(a) (*(uint32_t *)(void *)(mem+(a)))
#define MEMF(a) (*(float *)(void *)(mem+(a)))
static uint32_t host,flags,ms=1000;static unsigned count,polls;
static NgbUiPromptRecord last;
void ngb_ui_prompt_host_state(uint32_t *b,uint32_t *f,uint32_t *t){*b=host;*f=flags;*t=ms;polls++;}
void ngb_ui_prompt_record(const NgbUiPromptRecord *r){last=*r;count++;}
'''
main=r'''
int main(void){
 uint32_t p=0x1000,v=0x2000;
 ui_loading_seen(p,v);assert(!count && !polls); // idle never polls input
 MEM32(0x8DB160)=1;MEM32(0xD21734)=1;ui_loading_seen(p,v);assert(count==1 && last.phase==1);
 for(int i=0;i<1000;i++){ui_loading_seen(p,v);}assert(count==1 && polls==1);
 MEM32(0xD21734)++;ui_loading_seen(p,v);assert(count==1); // one second timer
 ms+=1000;MEM32(0xD21734)++;ui_loading_seen(p,v);assert(count==2);
 MEM32(0x8DB160)=2;MEM32(0xD21734)++;ui_loading_seen(p,v);assert(count==3 && !last.guide);
 MEM32(p)=5;MEMF(p+0x20)=1;MEMF(v)=530;MEMF(v+4)=420;
 ui_loading_seen(p,v);assert(count==4 && last.guide && last.phase==2); // first visible guide, same frame
 for(int i=0;i<1000;i++){ui_loading_seen(p,v);}assert(count==4);
 MEM32(0xD21734)++;host=1u<<20;flags=5;ui_loading_seen(p,v);
 assert(count==5 && last.host_buttons==(1u<<20) && last.host_flags==5);
 MEM32(0xD21734)++;MEM32(0xD219A8)=5;ui_loading_seen(p,v);
 assert(count==6 && last.guest[1]==5); // raw guest report differs from host
 MEM8(0xDB932E)=255;MEM32(0xD21734)++;host=0;ui_loading_seen(p,v);
 assert(last.pad_index==255 && !last.guest[1]); // invalid player does not read out of bounds
 MEM32(0x8DB160)=5;MEM32(0xD21734)++;ui_loading_seen(p,v);assert(last.phase==5 && !last.guide);
 unsigned before=count,poll_before=polls;
 for(int i=0;i<1000;i++){MEM32(0xD21734)++;ms+=1000;ui_loading_seen(p,v);}
 assert(count==before && polls==poll_before); // no gameplay sampling
 MEM32(0x8DB160)=3;
 for(int i=0;i<1000;i++){MEM32(0xD21734)++;ms+=1000;ui_loading_seen(p,v);}
 assert(count==256);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-fsanitize=address,undefined','-I'+str(root/'porting/macos-on-ios'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: actual observer records guide/state/host-vs-guest input edges; one poll/frame, 1Hz periodic, 256 cap, no gameplay sampling')
