"""Check the actual native-indicator alpha scope, without a second screen draw."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3];base=root/'porting/macos-on-ios/korean'
s=(base/'chapter_loading_caption.c').read_text()
assert 'sub_00204540' not in s and 'PUSH32' not in s
body='\n'.join(l for l in s.splitlines() if not l.startswith(('#include','#define')))
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros='\n'.join(re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)[0] for name in ['XBOX_PTR','MEM32','MEMF'])
prefix='''#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "chapter_card_state.h"
static uint8_t memory[0x1000000];static uintptr_t g_xbox_mem_offset;
'''+macros+'\n'
main='''
static unsigned stack=0x20000;
static void run(unsigned caller,float alpha,float expected){
 MEM32(stack)=caller;MEMF(stack+4)=alpha;
 MEMF(stack+8)=565.5f;MEMF(stack+12)=398.25f;MEM32(stack+16)=247;
 uint32_t saved=MEM32(stack+4);
 ngb_chapter_indicator_begin(stack,caller);
 assert((isnan(expected)&&isnan(MEMF(stack+4))) || MEMF(stack+4)==expected);
 assert(MEMF(stack+8)==565.5f && MEMF(stack+12)==398.25f && MEM32(stack+16)==247);
 // The native callee consumes the alpha argument into its sprite packet;
 // the caller's original arguments are restored after the one native draw.
 MEM32(0x50020)=MEM32(stack+4);
 ngb_chapter_indicator_end();
 assert(MEM32(stack)==caller && MEM32(stack+4)==saved);
 assert((isnan(expected)&&isnan(MEMF(0x50020))) || MEMF(0x50020)==expected);
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)memory;MEM32(0x8DB028)=3;
 run(0x1F5B11,0,0); // unrelated screens never get an indicator
 ngb_chapter_card_start_begin();
 for(unsigned mode=0;mode<=7;mode++){
  MEM32(0x8DB028)=mode;run(0x1F5B11,0.25f,mode==3?1.0f:0.25f);
 }
 MEM32(0x8DB028)=2;run(0x1F5B11,0,0); // flight into view stays native
 MEM32(0x8DB028)=3;
 for(unsigned phase=1;phase<=4;phase++){
  MEM32(0x8DB160)=phase;run(0x1F5B11,0.25f,0.25f);
 }
 MEM32(0x8DB160)=0;
 run(0x1F5B12,0,0);run(0x1F5B11,0,1);run(0x1F5B11,0.7f,1);
 run(0x1F5B11,1,1);run(0x1F5B11,-1,-1);run(0x1F5B11,2,2);run(0x1F5B11,NAN,NAN);
 // Nesting and overflow cannot overwrite an outer call's saved argument.
 MEMF(stack+4)=0.4f;
 for(int i=0;i<6;i++)ngb_chapter_indicator_begin(stack,0x1F5B11);
 for(int i=0;i<5;i++){ngb_chapter_indicator_end();assert(MEMF(stack+4)==1);}
 ngb_chapter_indicator_end();assert(MEMF(stack+4)==0.4f);
 ngb_chapter_card_start_ready();run(0x1F5B11,0.25f,0.25f);
 ngb_chapter_card_start_end();run(0x1F5B11,0,0);
 ngb_chapter_indicator_end();return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(base),str(p/'test.c'),str(base/'chapter_card_state.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: actual native indicator alpha held only at its chapter resting stage before readiness; flight, native coordinates/clock, other callers/loading, input and bit-exact argument restoration retained; no extra screen draw')
