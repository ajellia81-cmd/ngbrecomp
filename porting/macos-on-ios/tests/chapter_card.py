"""Check native readiness lifetime and language code-generation adapters."""
from pathlib import Path
import importlib.util,subprocess,tempfile,os
root=Path(__file__).resolve().parents[3];base=root/'porting/macos-on-ios/korean'
spec=importlib.util.spec_from_file_location('card',base/'chapter_card_codegen.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)
 (p/'state.c').write_text('''#include <assert.h>
#include "chapter_card_state.h"
int main(void){
 assert(!ngb_chapter_card_pending());
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
 assert(ngb_chapter_card_pending());ngb_chapter_card_draw_end();return 0;
}''')
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(base),str(p/'state.c'),str(base/'chapter_card_state.c'),'-o',str(p/'state')],check=True)
 subprocess.run([str(p/'state')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 gen=p/'gen';gen.mkdir();(gen/'recomp_funcs.h').write_text('/* fixture */\n')
 def fixture(prefix):
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
'''
 (gen/'recomp_a.c').write_text(fixture('ngb_original_'))
 (gen/'recomp_korean.c').write_text(fixture('kr_'))
 mod.patch(gen)
 for file in gen.glob('*.c'):
  source=file.read_text();assert source.count('ngb_chapter_card_update_begin();')==1
  assert source.index('goto loc_001F3D50;')<source.index('ngb_chapter_card_input_ready();')<source.index('confirm();')
  assert 'ngb_chapter_card_draw_end();' in source
 before={f:f.read_text() for f in gen.iterdir()}
 try:mod.patch(gen)
 except ValueError:pass
 else:raise AssertionError('repeated application must fail')
 assert all(f.read_text()==s for f,s in before.items())
print('PASS: readiness is consumed per draw, native locks and world scope respected; both language bodies adapted and invalid generation leaves files untouched')
