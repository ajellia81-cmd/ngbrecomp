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
# Keep the new upstream settings numbers intact, plus only the Korean selector.
import collections
original=subprocess.check_output(['git','show','2606383704e2675b687fec89fb3dfa965b068d46:src/ingame_menu.c'],cwd=root,text=True)
for name in ('int ngb_pc_setting(', 'void ngb_pc_set_setting('):
 old=collections.Counter(re.findall(r'case (\d+):',block(original,name)))
 new=collections.Counter(re.findall(r'case (\d+):',block(s,name)))
 old['46']+=1
 assert old==new,(name,old,new)
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
 assert(S_AUDIOOUT==25 && S_INTERP==45);
 int pause=0,pc=0;
 for(int i=0;i<CAT_ROWS && s_cat_rows[0][i]>=0;i++)pause+=s_cat_rows[0][i]==S_INTERP;
 for(unsigned i=0;i<sizeof s_rows_gfx;i++)pc+=s_rows_gfx[i]==ROW_INTERP;
 assert(pause==1 && pc==1);
 int pw=0,pd=0,cw=0,cd=0;
 for(int i=0;i<CAT_ROWS && s_cat_rows[1][i]>=0;i++){pw+=s_cat_rows[1][i]==S_WINDOW;pd+=s_cat_rows[1][i]==S_DISPLAY;}
 for(unsigned i=0;i<sizeof s_rows_disp;i++){cw+=s_rows_disp[i]==ROW_WINDOW;cd+=s_rows_disp[i]==ROW_DISPLAY;}
#ifdef NGB_CONVERTED_DISPLAY
 assert(!pw && !pd && !cw && !cd && CTL_FIRST_COL==2);
#else
 assert(pw==1 && pd==1 && cw==1 && cd==1 && CTL_FIRST_COL==0);
#endif
 for(int cat=0;cat<CATS+1;cat++) {
  unsigned seen=0;
  for(int i=0;i<CAT_ROWS && s_cat_rows[cat][i]>=0;i++) {
   int item=s_cat_rows[cat][i];assert(item<S_COUNT);
   for(int j=0;j<i;j++)assert(s_cat_rows[cat][j]!=item);
   seen++;
  }
  if(cat==1) {
#ifdef NGB_CONVERTED_DISPLAY
   assert(seen==7);
#else
   assert(seen==9);
#endif
  }
 }
 int row=0;s_ctl_col=CTL_FIRST_COL;
 ctl_keys(K_LEFT,&row,0);assert(s_ctl_col==CTL_FIRST_COL);
 ctl_keys(K_RIGHT,&row,0);
#ifdef NGB_CONVERTED_DISPLAY
 assert(s_ctl_col==2);
#else
 assert(s_ctl_col==1);s_ctl_col=2;
#endif
 capture_slot=-1;ctl_keys(K_OK,&row,0);assert(capture_slot==2);
 changed_slot=-1;ctl_keys(K_CLEAR,&row,0);assert(changed_slot==2);
 row=1;capture_slot=-1;changed_slot=-1;
 ctl_keys(K_OK,&row,0);ctl_keys(K_CLEAR,&row,0);assert(capture_slot==-1 && changed_slot==-1);
 ctl_mouse(HIT_CTL+0*4+2,&row);assert(row==0 && s_ctl_col==2);
 assert(ctl_hit(s_ctl_cx[2],CTL_Y0+1)==HIT_CTL+2);
#ifdef NGB_CONVERTED_DISPLAY
 ctl_mouse(HIT_CTL+0*4+0,&row);assert(s_ctl_col==2);
 assert(ctl_hit(252,CTL_Y0+1)==HIT_CTL+3);
 assert(ctl_hit(343,CTL_Y0+1)==HIT_CTL+2);
#endif
 row=CTL_MOUSE;ctl_keys(K_RIGHT,&row,0);assert(sensitivity_steps==1);
 row=CTL_PROMPTS;ctl_keys(K_LEFT,&row,0);assert(prompt_steps==-1);
 return 0;
}
'''
pause=(root/'src/pause_tab.c').read_text();pc=(root/'src/pc_menu.c').read_text()
tables=re.search(r'enum \{ S_RENDER.*?\};',pause,re.S)[0]+ '\n#define CATS 6\n#define CAT_ROWS 9\n'+block(pause,'static const signed char s_cat_rows')+';\n'
tables+=re.search(r'enum \{ ROW_SCREEN.*?\};',pc,re.S)[0]+'\n'
for name in ('s_rows_gfx','s_rows_disp'):tables+=block(pc,'static const uint8_t '+name)+';\n'
control_prefix=r"""
#include <math.h>
#define A_COUNT 3
#define CTL_INVX A_COUNT
#define CTL_MOUSE (A_COUNT+5)
#define CTL_PROMPTS (A_COUNT+6)
#define CTL_DEF (A_COUNT+7)
#define CTL_ROWS (A_COUNT+8)
#define CTL_TOGGLE(r) ((r)>=CTL_INVX && (r)<=A_COUNT+4)
#define CTL_Y0 74
#define CTL_SHOWN 6
#define PAGE_W 450
#define ROW_H 30
#define HIT_CTL 128
static int capture_slot=-1,changed_slot=-1,sensitivity_steps,prompt_steps;
void play_sound(int n){(void)n;}
void ctl_toggle_flip(int n){(void)n;}
void ngb_bind_step_mouse_sensitivity(int n){sensitivity_steps+=n;}
void ngb_bind_step_prompts(int n){prompt_steps+=n;}
void ngb_bind_defaults(void){}
int ngb_bind_get(int row,int slot){return 42;}
void ngb_bind_set(int row,int slot,int n){changed_slot=slot;}
int ngb_bind_pad_fixed(int row){return row==1;}
void ngb_bind_capture_begin(int row,int slot){capture_slot=slot;}
"""
a=pause.index('#ifdef NGB_CONVERTED_DISPLAY',pause.index('#define CTL_NAME_X'))
b=pause.index('static void ctl_scroll',a)
control_source=re.search(r'enum \{ K_UP.*?\};',pause,re.S)[0]+control_prefix+pause[a:b]
for name in ('static int ctl_hit(', 'static int ctl_keys(', 'static void ctl_mouse('):
 control_source+=block(pause,name)+'\n'
assert 'K_ALT_OK' in pause
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)/'test.c';p.write_text(prefix+keys+'\n'+function+'\n'+tables+control_source+main)
 for define in ([],['-DNGB_CONVERTED_DISPLAY=1']):
  exe=Path(folder)/'test';subprocess.run([os.environ.get('CC','cc'),'-std=c11',*define,str(p),'-lm','-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('PASS: A/X focus, hidden iOS window rows, controller-only navigation/capture/clear/hit tests, fixed sticks and desktop controls retained')
