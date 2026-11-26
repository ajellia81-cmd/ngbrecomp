"""Check native readiness lifetime and language code-generation adapters."""
from pathlib import Path
import importlib.util,subprocess,tempfile,os,ast,re
from types import SimpleNamespace
root=Path(__file__).resolve().parents[3];base=root/'porting/macos-on-ios/korean'
spec=importlib.util.spec_from_file_location('card',base/'chapter_card_codegen.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
# Render the public upstream emitter's own template, without importing private input.
lifter=ast.parse((root/'xboxrecomp/tools/recomp/lifter.py').read_text())
method=next(n for n in ast.walk(lifter) if isinstance(n,ast.FunctionDef) and n.name=='_lift_call')
calls=[n for n in ast.walk(method) if isinstance(n,ast.JoinedStr) and
       'PUSH32(esp' in ''.join(v.value for v in n.values if isinstance(v,ast.Constant) and isinstance(v.value,str)) and
       'RECOMP_ABI_CALL' in ''.join(v.value for v in n.values if isinstance(v,ast.Constant) and isinstance(v.value,str))]
assert len(calls)==1
call_expr=compile(ast.Expression(calls[0]),'<upstream native-call template>','eval')
def native_gate(name):
 return eval(call_expr,{'__builtins__':{}},{'ret_va':0x002DFCB5,'insn':SimpleNamespace(call_target=0x002ECC50),'name':name})
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)
 (p/'state.c').write_text('''#include <assert.h>
#include "chapter_card_state.h"
int main(void){
 assert(!ngb_chapter_card_pending());
 ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(3);
 ngb_chapter_card_footer_begin(0x1F5AF6,3);
 assert(!ngb_chapter_card_hide_footer()); // non-chapter loading is unchanged
 ngb_chapter_card_footer_end();ngb_chapter_card_draw_end();
 ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(2);
 assert(ngb_chapter_card_pending());assert(!ngb_chapter_card_can_confirm());
 ngb_chapter_card_draw_end();assert(!ngb_chapter_card_pending());
 ngb_chapter_card_update_begin();ngb_chapter_card_input_ready();
 ngb_chapter_card_draw_begin(2);assert(ngb_chapter_card_can_confirm());
 ngb_chapter_card_draw_begin(2);assert(ngb_chapter_card_can_confirm());
 ngb_chapter_card_draw_end();ngb_chapter_card_draw_end();
 ngb_chapter_card_draw_begin(2);assert(ngb_chapter_card_pending());
 ngb_chapter_card_draw_end();
 ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(0);
 assert(!ngb_chapter_card_pending());ngb_chapter_card_draw_end();
 ngb_chapter_card_update_begin();ngb_chapter_card_input_ready();
 ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(2);
 assert(ngb_chapter_card_pending());ngb_chapter_card_draw_end();
 ngb_chapter_card_start_begin();
 ngb_chapter_card_footer_begin(0x1F5AF6,3);assert(ngb_chapter_card_hide_footer());
 ngb_chapter_card_footer_end(); // exact route also covers generated draw continuations
 for(int i=0;i<3;i++){
  ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(3);
  assert(ngb_chapter_card_pending());
  ngb_chapter_card_footer_begin(0x1F5AF6,3);assert(ngb_chapter_card_hide_footer());
  ngb_chapter_card_footer_end();ngb_chapter_card_draw_end();
 }
 ngb_chapter_card_start_ready();
 for(int i=0;i<3;i++){
  ngb_chapter_card_update_begin();ngb_chapter_card_draw_begin(3);
  assert(ngb_chapter_card_can_confirm());ngb_chapter_card_draw_end();
 }
 ngb_chapter_card_start_end();assert(!ngb_chapter_card_route());return 0;
}''')
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(base),str(p/'state.c'),str(base/'chapter_card_state.c'),'-o',str(p/'state')],check=True)
 subprocess.run([str(p/'state')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 gen=p/'gen';gen.mkdir();(gen/'recomp_funcs.h').write_text('/* fixture */\n')
 def fixture(prefix):
  gate_call=native_gate('kr_sub_002ECC50' if prefix=='kr_' else 'sub_002ECC50')
  return f'''void {prefix}sub_001F3AD0(void) {{
 RECOMP_ENTRY(0x001F3AD0u);
 if (CMP_NE(_fa, _fb)) goto loc_001F3D50;
 confirm();
 loc_001F3D50: return;
}}
void {prefix}sub_001F3ED0(void) {{
 RECOMP_ENTRY(0x001F3ED0u);
 if (early) return;
 draw();
}}
void {prefix}sub_002DF930(void) {{
 RECOMP_ENTRY(0x002DF930u);
 if (early) return;
 {gate_call}
 wait_loop();
}}
void {prefix}sub_001F5E40(void) {{
 RECOMP_ENTRY(0x001F5E40u);
 caption_draw();
}}
void {prefix}sub_002060A0(void) {{
 RECOMP_ENTRY(0x002060A0u);
 {prefix}sub_00206670();
}}
void {prefix}sub_00206670(void) {{
 RECOMP_ENTRY(0x00206670u);
 if (skip_native) goto loc_0020668D;
 native_loading_draw();
 loc_0020668D: ;
}}
void {prefix}sub_001F97F0(void) {{
 RECOMP_ENTRY(0x001F97F0u);
 indicator_draw();
}}
void {prefix}sub_00205350(void) {{
 RECOMP_ENTRY(0x00205350u);
 prose_draw();
}}
'''
 (gen/'recomp_a.c').write_text('void ngb_original_sub_00206670(void);\n'+fixture('ngb_original_'))
 (gen/'recomp_korean.c').write_text('void kr_sub_00206670(void);\n'+fixture('kr_'))
 mod.patch(gen)
 for file in gen.glob('*.c'):
  source=file.read_text();assert source.count('ngb_chapter_card_update_begin();')==1
  assert source.index('goto loc_001F3D50;')<source.index('ngb_chapter_card_input_ready();')<source.index('confirm();')
  assert 'ngb_chapter_card_draw_end();' in source
  assert 'ngb_chapter_loading_visual();' not in source
  assert 'ngb_chapter_indicator_begin(esp,MEM32(esp));' in source
  assert 'ngb_chapter_indicator_end();' in source
  assert 'ngb_chapter_loading_visual_active()' not in source
  assert source.index('RECOMP_ABI_CALL(0x002ECC50u,')<source.index('ngb_chapter_card_start_ready();')<source.index('wait_loop();')
  assert 'ngb_chapter_card_footer_begin(MEM32(esp),MEM32(0x8DB028));' in source

 prefix='''#include "chapter_card_state.h"
#include "recomp_funcs.h"
extern unsigned _fa,_fb,early,mode,footer_caller,esp,skip_native,ecx;
#define RECOMP_ENTRY(x) ((void)0)
#define CMP_NE(a,b) ((a)!=(b))
#define MEM32(a) ((a)==0x8DB028u?mode:footer_caller)
void confirm(void);void draw(void);void wait_loop(void);void caption_draw(void);
void sub_002ECC50(void);void kr_sub_002ECC50(void);
void native_loading_draw(void);void prose_draw(void);void indicator_draw(void);
/* This fixture models UI pumping; real guest memory/ABI is tested separately. */
#define PUSH32(sp,value) ((void)(sp),(void)(value))
#define RECOMP_ABI_CALL(va, fn) (fn)()
'''
 for file in gen.glob('*.c'):file.write_text(prefix+file.read_text())
 main='''#include <assert.h>
#include "chapter_card_state.h"
unsigned _fa=1,_fb,early,mode=3,footer_caller=0x1F5AF6,esp=0x10000,skip_native,ecx;
static unsigned captions,expect_hidden,native_calls,prose_calls,indicator_calls,indicator_depth;
void ngb_original_sub_001F3AD0(void);void ngb_original_sub_001F3ED0(void);
void ngb_original_sub_002DF930(void);void ngb_original_sub_001F5E40(void);
void kr_sub_001F3AD0(void);void kr_sub_001F3ED0(void);
void kr_sub_002DF930(void);void kr_sub_001F5E40(void);
void ngb_original_sub_002060A0(void);void kr_sub_002060A0(void);
void ngb_original_sub_00205350(void);void kr_sub_00205350(void);
void native_loading_draw(void){native_calls++;}
void prose_draw(void){prose_calls++;esp+=4;}
void indicator_draw(void){assert(indicator_depth==1);indicator_calls++;esp+=4;}
void ngb_chapter_indicator_begin(unsigned stack,unsigned caller){assert(stack==100 && caller==0x1F5AF6);indicator_depth++;}
void ngb_chapter_indicator_end(void){assert(indicator_depth==1);indicator_depth--;}
void ngb_original_sub_001F97F0(void);void kr_sub_001F97F0(void);
static void (*update)(void),(*card)(void),(*footer)(void);
static void pump(void){update();card();assert(!ngb_chapter_card_hide_footer());}
void confirm(void){assert(0);}
void caption_draw(void){assert(ngb_chapter_card_hide_footer()==(int)expect_hidden);captions++;}
void draw(void){footer();}
void sub_002ECC50(void){expect_hidden=1;for(unsigned i=0;i<3;i++)pump();}
void kr_sub_002ECC50(void){sub_002ECC50();}
void wait_loop(void){expect_hidden=0;for(unsigned i=0;i<3;i++)pump();}
int main(void){
 for(int korean=0;korean<2;korean++){
  update=korean?kr_sub_001F3AD0:ngb_original_sub_001F3AD0;
  card=korean?kr_sub_001F3ED0:ngb_original_sub_001F3ED0;
  footer=korean?kr_sub_001F5E40:ngb_original_sub_001F5E40;
  void (*start)(void)=korean?kr_sub_002DF930:ngb_original_sub_002DF930;
  unsigned before=captions;start();assert(captions==before+6 && !ngb_chapter_card_route());
  early=1;start();assert(captions==before+6 && !ngb_chapter_card_route());early=0;
  void (*frame)(void)=korean?kr_sub_002060A0:ngb_original_sub_002060A0;
  void (*prose)(void)=korean?kr_sub_00205350:ngb_original_sub_00205350;
  unsigned n=native_calls;skip_native=0;frame();
  assert(native_calls==n+1);
  skip_native=1;frame();assert(native_calls==n+1);
  unsigned pc=prose_calls;esp=100;prose();assert(esp==104 && prose_calls==pc+1);
  esp=100;ecx=123;unsigned ic=indicator_calls;
  (korean?kr_sub_001F97F0:ngb_original_sub_001F97F0)();
  assert(esp==104 && indicator_calls==ic+1 && !indicator_depth);
 }
 return 0;
}
'''
 (p/'route.c').write_text(main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(base),'-I'+str(gen),str(p/'route.c'),*[str(f) for f in gen.glob('*.c')],str(base/'chapter_card_state.c'),'-o',str(p/'route')],check=True)
 subprocess.run([str(p/'route')],check=True)
 before={f:f.read_text() for f in gen.iterdir()}
 try:mod.patch(gen)
 except ValueError:pass
 else:raise AssertionError('repeated application must fail')
 assert all(f.read_text()==s for f,s in before.items())
print('PASS: compiled original/Korean setup pumps hide the exact footer before preparation and show it during the native wait loop; readiness is consumed per draw, native locks and world scope respected; both language bodies adapted and invalid generation leaves files untouched')
