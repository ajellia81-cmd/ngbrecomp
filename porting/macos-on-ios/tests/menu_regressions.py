"""Drive the actual pause-tab input function and compile both real menu tables."""
from pathlib import Path
import re,subprocess,tempfile,os
root=Path(__file__).resolve().parents[3]
s=(root/'src/ingame_menu.c').read_text()
def block(s,token):
 a=s.index(token);i=s.index('{',a)+1;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
function=block(s,'int ngb_pause_tab_key(')
keys=re.search(r'enum \{ KEY_UP.*?\};',s,re.S)[0]
prefix=r'''
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#define XBOX_GAMEPAD_DPAD_UP 1
#define XBOX_GAMEPAD_DPAD_DOWN 2
#define XBOX_GAMEPAD_DPAD_LEFT 4
#define XBOX_GAMEPAD_DPAD_RIGHT 8
#define XBOX_GAMEPAD_START 16
static uint64_t s_tab_seen=1000,s_tab_rat;
static uint32_t s_tab_prev,s_tab_rbit,bits;
static int s_tab_focus,s_open,usb_gamepad_block,queued;
static bool s_wait_release;
static uint64_t mono_ms(void){return 1000;}
static uint32_t pad_bits(void){return bits;}
static int tab_pop(void){int q=queued;queued=0;return q;}
int ngb_bind_capturing(void){return 0;}
int xbox_InputKeyboardActive(void){return 0;}
'''
main=r'''
int main(void){
 unsetenv("NGB_OPTIONS_TEST");unsetenv("NGB_OPTIONS_TEST_GO");
 for(unsigned bit=16;bit<=20;bit+=4){
  int focus=0;s_tab_focus=0;s_tab_prev=0;bits=0;
  assert(ngb_pause_tab_key(&focus)==0 && !focus);
  bits=1u<<bit;assert(ngb_pause_tab_key(&focus)==0 && focus && usb_gamepad_block);
  assert(ngb_pause_tab_key(&focus)==0); // held confirm must not activate a row
  bits=0;assert(ngb_pause_tab_key(&focus)==0);
  bits=1u<<bit;assert(ngb_pause_tab_key(&focus)==(bit==16?KEY_OK:KEY_ALT_OK));
  bits=0;ngb_pause_tab_key(&focus);queued=KEY_CLEAR;
  assert(ngb_pause_tab_key(&focus)==KEY_CLEAR); // Delete is not a destructive confirm
 }
 assert(S_SMOOTH==26);
 int pause=0,pc=0;
 for(int i=0;i<CAT_ROWS && s_cat_rows[0][i]>=0;i++)pause+=s_cat_rows[0][i]==S_SMOOTH;
 for(unsigned i=0;i<sizeof s_rows_gfx;i++)pc+=s_rows_gfx[i]==ROW_SMOOTH;
 assert(pause==1 && pc==1);
 for(unsigned i=0;i<sizeof s_rows_disp;i++)assert(s_rows_disp[i]!=ROW_SMOOTH);
 for(int i=0;i<CAT_ROWS && s_cat_rows[1][i]>=0;i++)assert(s_cat_rows[1][i]!=S_SMOOTH);
 return 0;
}
'''
pause=(root/'src/pause_tab.c').read_text();pc=(root/'src/pc_menu.c').read_text()
tables=re.search(r'enum \{ S_RENDER.*?\};',pause,re.S)[0]+ '\n#define CATS 6\n#define CAT_ROWS 7\n'+block(pause,'static const signed char s_cat_rows')+';\n'
tables+=re.search(r'enum \{ ROW_SCREEN.*?\};',pc,re.S)[0]+'\n'
for name in ('s_rows_gfx','s_rows_disp'):tables+=block(pc,'static const uint8_t '+name)+';\n'
# Verify consumers distinguish the controller's alternate confirm from Delete.
assert 'key == K_ALT_OK) key = s_cat == CAT_CTL && s_row >= 0 ? K_CLEAR : K_OK' in pause
assert pause.count('if (key == K_ALT_OK) key = K_OK;')==2
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)/'test.c';p.write_text(prefix+keys+'\n'+function+'\n'+tables+main)
 for define in ([],['-DNGB_CONVERTED_DISPLAY=1']):
  exe=Path(folder)/'test';subprocess.run([os.environ.get('CC','cc'),'-std=c11',*define,str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('PASS: production A/X focus and row input, held-button debounce, Delete separation, Smooth Motion Graphics in both menus')
