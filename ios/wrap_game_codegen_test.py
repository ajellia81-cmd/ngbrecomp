import tempfile
import unittest
import os
import subprocess
from pathlib import Path
from wrap_game_codegen import wrap, CACHE_WAIT, CACHE_PROBES, PRODUCER_WAIT, SHADER_READY, DEMO_READY, adapt_cache_wait

class HooksTest(unittest.TestCase):
    def test_cache_wait_keeps_pause_stop_and_other_waits(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);source=p/'policy.c';exe=p/'policy'
            hooks=(Path(__file__).resolve().parent/'game_hooks.c').read_text()
            helper='unsigned ngb_ios_cache_copy_delay'+hooks.split('unsigned ngb_ios_cache_copy_delay',1)[1].split('\n}',1)[0]+'\n}\n'
            helper+='unsigned ngb_ios_cache_producer_fast'+hooks.split('unsigned ngb_ios_cache_producer_fast',1)[1].split('\n}',1)[0]+'\n}\n'
            helper+='unsigned ngb_ios_shader_loading_ready'+hooks.split('unsigned ngb_ios_shader_loading_ready',1)[1].split('\n}',1)[0]+'\n}\n'
            source.write_text('''#include <assert.h>
#include <stdint.h>
#include "cache_copy_policy.h"
static unsigned char ram[16*1024*1024];
#define MEM8(a) (*(uint8_t *)(ram+(a)))
#define MEM32(a) (*(uint32_t *)(ram+(a)))
static unsigned prepared,queries;
static unsigned ngb_ios_shader_prewarm_ready(void){queries++;return prepared;}
'''+helper+'''
int main(void) {
 assert(ngb_cache_copy_wait(50,0,1)==1);
 assert(ngb_cache_copy_wait(50,0,0)==50);
 for(unsigned state=1;state<4;state++)assert(ngb_cache_copy_wait(50,state,1)==50);
 assert(ngb_cache_copy_wait(50,0xFFFFFFFFu,1)==50);
 for(unsigned wait=0;wait<100;wait++)if(wait!=50)assert(ngb_cache_copy_wait(wait,0,1)==wait);
 for(unsigned stage=0;stage<7;stage++)for(unsigned gate=0;gate<2;gate++) {
  MEM32(0x8DB160)=stage;MEM8(0xD52F2D)=gate;
  assert(ngb_ios_cache_copy_delay(50,0)==(gate && stage>=1 && stage<=3 ? 1 : 50));
  assert(ngb_ios_cache_copy_delay(50,1)==50);
  for(prepared=0;prepared<2;prepared++) {
   queries=0;
   assert(ngb_ios_shader_loading_ready()==(stage<1 || stage>3 || prepared));
   assert(queries==(stage>=1 && stage<=3));
  }
  for(unsigned state=0;state<4;state++)for(unsigned copying=0;copying<5;copying++)for(unsigned stopping=0;stopping<2;stopping++) {
   MEM32(0xC0608C)=state;MEM32(0xC06098)=copying;MEM32(0x45EC3C)=stopping;
   assert(ngb_ios_cache_producer_fast()==(gate && stage>=1 && stage<=3 && state==0 && copying==1 && !stopping));
  }
 }
 return 0;
}''')
            subprocess.run([os.environ.get('CC','cc'),'-I'+str(Path(__file__).resolve().parent),str(source),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_preserves_calls_writes_and_unrelated_reads(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            source='void sub_001E6AE0(void) {}\nvoid sub_0017E8A0(void) {}\n'
            source+='void caller(void) { sub_001E6AE0(); }\n'
            source+='void f(void) {\n'+'x=MEM32(0xDB9890);\n'*11
            source+='MEM32(0xDB9890)=1;\nx=MEM32(0xDB9894);\n}\n'
            source+='void movie_tick(void) { RECOMP_ENTRY(0x0019A7D0u); }\n'
            source+='void movie_cue(void) { loc_0019A9CD: ; }\n'
            source+='void sub_00232D40(void) { RECOMP_ENTRY(0x00232D40u); }\n'
            source+='void movie_userdata(void) { loc_0031E1C5: ; }\n'
            source+='void cache_worker(void) {\n'+CACHE_WAIT+'\n}\n'
            source+='void producer(void) {\n'+PRODUCER_WAIT+'\n}\n'
            source+='void idle(void) {\n'+DEMO_READY+'\n}\n'
            source+='void loading(void) {\n'+'\n'.join(SHADER_READY)+'\n}\n'
            source+='void cache_probes(void) {\n'+'\n'.join(CACHE_PROBES)+'\n}\n'
            (root/'recomp_0000.c').write_text(source)
            (root/'recomp_funcs.h').write_text('')
            wrap(root)
            result=(root/'recomp_0000.c').read_text()
            self.assertIn('void ngb_base_001E6AE0(void)',result)
            self.assertIn('void ngb_base_00232D40(void)',result)
            self.assertIn('sub_001E6AE0();',result)
            self.assertEqual(result.count('ngb_ios_game_text_language(MEM32(0xDB9890))'),11)
            self.assertIn('MEM32(0xDB9890)=1;',result)
            self.assertIn('MEM32(0xDB9894)',result)
            self.assertEqual(result.count('ngb_ios_movie_probe('),4)
            self.assertEqual(result.count('ngb_ios_cache_copy_delay('),1)
            pause='loc_0031C825: ;\n    PUSH32(esp, 0x32);'
            self.assertEqual(adapt_cache_wait(pause),pause)
            # The other library thread's display wait and all call targets
            # outside the pinned service-thread site remain byte-for-byte.
            other=PRODUCER_WAIT.replace('00333085','00333005')
            self.assertEqual(adapt_cache_wait(other),other)
            self.assertIn(PRODUCER_WAIT,result)
            self.assertEqual(result.count('ngb_ios_cache_producer_fast()'),1)
            self.assertIn(DEMO_READY+'\n    if (!ngb_ios_shader_prewarm_ready()) goto loc_00165DF9;',result)
            for marker,destination in SHADER_READY.items():
                self.assertIn(marker+'\n    if (!ngb_ios_shader_loading_ready()) goto '+destination+';',result)
if __name__=='__main__':unittest.main()
