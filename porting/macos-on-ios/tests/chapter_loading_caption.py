"""Exercise the real draw-only chapter timeline, including native controller progress."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[3];base=root/'porting/macos-on-ios/korean'
s=(base/'chapter_loading_caption.c').read_text()
assert 'sub_00204540' not in s and 'PUSH32' not in s
body='\n'.join(l for l in s.splitlines() if not l.startswith(('#include','#define')))
runtime=(root/'xboxrecomp/templates/runtime/recomp_types.h').read_text()
macros='\n'.join(re.search(r'^#define '+name+r'\([^\n]*',runtime,re.M)[0] for name in ['XBOX_PTR','MEM32'])
prefix='''#include <stdint.h>
#include <assert.h>
#include "chapter_card_state.h"
static uint8_t memory[0x1000000];static uintptr_t g_xbox_mem_offset;
'''+macros+'\n'
main='''
static void render(unsigned mode,unsigned counter,unsigned frame,unsigned visual_mode,unsigned visual_counter,unsigned busy){
 MEM32(0x8DB028)=mode;MEM32(0x8DAF70)=counter;MEM32(0xD21734)=frame;
 MEM32(0x8DAF18)=0x3f800000;MEM32(0x8DAF20)=123;
 ngb_chapter_card_draw_begin(mode);ngb_chapter_animation_draw_begin();
 assert(MEM32(0x8DB028)==visual_mode && MEM32(0x8DAF70)==visual_counter);
 assert(ngb_chapter_card_visual_busy()==(int)busy);
 assert(MEM32(0x8DAF18)==0x3f800000 && MEM32(0x8DAF20)==123);
 ngb_chapter_card_footer_begin(0x1F5AF6,MEM32(0x8DB028));
 assert(ngb_chapter_card_hide_footer()==(ngb_chapter_card_start_waiting() || busy));
 ngb_chapter_card_footer_end();
 assert(ngb_chapter_card_can_confirm()==(!ngb_chapter_card_start_waiting() && !busy));
 MEM32(0x50020)=456; // native drawing output must survive the state restore
 ngb_chapter_animation_draw_end();ngb_chapter_card_draw_end();
 assert(MEM32(0x8DB028)==mode && MEM32(0x8DAF70)==counter);
 assert(MEM32(0x50020)==456 && !ngb_chapter_card_visual_busy());
}
int main(void){
 g_xbox_mem_offset=(uintptr_t)memory;
 // An unrelated draw has no preparation scope and is untouched.
 MEM32(0x8DB028)=3;MEM32(0x8DAF70)=0;
 ngb_chapter_animation_draw_begin();assert(MEM32(0x8DB028)==3);
 ngb_chapter_animation_draw_end();
 ngb_chapter_card_start_begin();
 render(2,200,100,2,200,0);render(2,80,220,2,80,0);render(2,21,279,2,21,0);
 // Native updates keep counting down and enter mode3; only drawing waits.
 for(unsigned n=20;n>0;n--)render(2,n,300-n,2,20,1);
 render(3,0,300,2,20,1);render(3,0,1000,2,20,1);
 ngb_chapter_card_start_ready();
 // Replay the original remaining20 ticks exactly once after actual readiness.
 for(unsigned n=0;n<20;n++){
  render(3,0,1000+n,2,20-n,1);
  render(3,0,1000+n,2,20-n,1); // repeated same-frame draws never advance time
 }
 render(3,0,1020,3,0,0);render(3,0,1080,3,0,0);
 ngb_chapter_card_start_end();
 // Fast preparation never extends or restarts a native exit that was not held.
 ngb_chapter_card_start_begin();render(2,100,1100,2,100,0);
 ngb_chapter_card_start_ready();render(2,19,1200,2,19,0);render(3,0,1219,3,0,0);
 ngb_chapter_card_start_end();
 // Generic loading is not replaced. Per-chapter state resets independently.
 ngb_chapter_card_start_begin();MEM32(0x8DB160)=2;
 render(2,10,1300,2,10,0);MEM32(0x8DB160)=0;
 render(2,200,1400,2,200,0);render(2,20,1580,2,20,1);
 // Nested drawing restores each outer visual state, then the real controller.
 MEM32(0x8DB028)=3;MEM32(0x8DAF70)=0;
 for(int i=0;i<6;i++)ngb_chapter_animation_draw_begin();
 for(int i=0;i<5;i++){ngb_chapter_animation_draw_end();assert(MEM32(0x8DB028)==2 && MEM32(0x8DAF70)==20);}
 ngb_chapter_animation_draw_end();assert(MEM32(0x8DB028)==3 && MEM32(0x8DAF70)==0);
 ngb_chapter_card_start_end();
 // Native frame-counter wrap does not extend or truncate the exit.
 ngb_chapter_card_start_begin();render(2,20,0xfffffff0u,2,20,1);ngb_chapter_card_start_ready();
 render(3,0,0xfffffff8u,2,20,1);render(3,0,11,2,1,1);render(3,0,12,3,0,0);
 ngb_chapter_card_start_end();ngb_chapter_animation_draw_end();return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(base),str(p/'test.c'),str(base/'chapter_card_state.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: native entrance untouched, draw-only hold before exit, actual loading controller advances independently; native20-tick exit follows readiness once, guide hidden until exit ends, nested state restored, fast/other loading and frame wrap retained; no extra screen draw')
