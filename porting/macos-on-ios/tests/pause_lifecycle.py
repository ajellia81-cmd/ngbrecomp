"""Check the real custom-page gate across root menu transition states."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/pause_tab.c').read_text()
a=s.index('static int s_parent_header_visible;');b=s.index('\n}',s.index('static int pt_page_live(void)'))+2
body=s[a:b]
for tab,drawer in [('PT_TAB','pt_draw'),('6','pt_exit_draw')]:
 assert re.search(r'if \(eax == '+tab+r'.*?if \(pt_page_live\(\)\) '+drawer+r'\(\);',s)
program=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
static uint8_t mem[64];
#define PAGE_Y 118
#define MEM8(a) mem[(a)-0xDA41C0]
#define MEM32(a) (*(uint32_t *)(void *)(mem+(a)-0xDA41C0))
'''+body+r'''
int main(void){
 memset(mem,0,sizeof mem);
 assert(!pt_page_live());
 MEM32(0xDA41C8)=1;assert(!pt_page_live());
 MEM32(0xDA41C8)=2;assert(!pt_page_live());
 pt_header_seen(0x17B,1,10,10,100,80);assert(pt_page_live());
 s_parent_header_visible=0;assert(!pt_page_live());
 pt_header_seen(0x17B,0,10,10,100,80);assert(!pt_page_live());
 pt_header_seen(0x17B,1,-100,10,-10,80);assert(!pt_page_live());
 pt_header_seen(0x17B,1,10,200,100,280);assert(!pt_page_live());
 pt_header_seen(0x177,1,10,10,100,80);assert(!pt_page_live());
 pt_header_seen(0x17B,1,10,10,100,80);assert(pt_page_live());
 MEM32(0xDA41C8)=3;assert(!pt_page_live()); // close can clear page-local flags while the root still fades
 MEM32(0xDA41C8)=2;assert(pt_page_live());
 MEM32(0xDA41C0)=1;assert(pt_page_live());
 MEM8(0xDA41DD)=1;assert(!pt_page_live());
 MEM8(0xDA41DD)=0;MEM8(0xDA41E3)=1;assert(!pt_page_live());
 MEM8(0xDA41E3)=0;MEM32(0xDA41C0)=UINT32_MAX;assert(!pt_page_live());
 MEM32(0xDA41C0)=0;assert(pt_page_live());
 return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(program)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: Settings/Exit pages suppressed during root closing, transition and inactive states; reopened pages retained')
