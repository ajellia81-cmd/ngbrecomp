"""Check native motor mailbox behavior and the actual nonblocking input bridge."""
from pathlib import Path
import subprocess
import tempfile
from presenter_descriptor_test import function
ROOT=Path(__file__).resolve().parents[1]
PREFIX=r'''
#include <stdint.h>
#include <assert.h>
#include "haptics_policy.h"
#define __APPLE__ 1
#define TARGET_OS_IPHONE 1
#define XBOX_MAX_CONTROLLERS 4
#define ERROR_DEVICE_NOT_CONNECTED 1167
#define ERROR_SUCCESS 0
typedef uint32_t DWORD;
typedef struct { uint16_t wLeftMotorSpeed,wRightMotorSpeed; } XBOX_VIBRATION;
typedef int SDL_GameController;
static int mutex,locked,calls;
static int *g_pad_lock=&mutex;
static SDL_GameController controller,*g_pads[4];
static uint64_t last;
void SDL_LockMutex(int *m) { (void)m;assert(!locked);locked=1; }
void SDL_UnlockMutex(int *m) { (void)m;assert(locked);locked=0; }
void ngb_ios_haptics_set(unsigned port,uint16_t low,uint16_t high) {
 assert(locked && port==0);calls++;last=ngb_haptic_command(low,high,100);
}
// There is deliberately no SDL rumble, player, engine or dispatch initializer
// in this link: the production guest path must only publish the latest value.
'''
MAIN=r'''
int main(void) {
 XBOX_VIBRATION vibration={65535,1234};uint16_t low,high;
 assert(xbox_InputSetState(0,&vibration)==ERROR_DEVICE_NOT_CONNECTED && calls==0);
 g_pads[0]=&controller;
 for(unsigned i=0;i<10000;i++) { vibration.wRightMotorSpeed=i;assert(xbox_InputSetState(0,&vibration)==ERROR_SUCCESS); }
 assert(!locked && calls==10000);
 ngb_haptic_values(last,100,1,&low,&high);assert(low==65535 && high==9999);
 ngb_haptic_values(last,1099,1,&low,&high);assert(low==65535);
 ngb_haptic_values(last,1100,1,&low,&high);assert(!low && !high);
 ngb_haptic_values(last,100,0,&low,&high);assert(!low && !high);
 uint64_t wrap=ngb_haptic_command(42,43,UINT32_MAX-499);
 ngb_haptic_values(wrap,499,1,&low,&high);assert(low==42 && high==43);
 ngb_haptic_values(wrap,500,1,&low,&high);assert(!low && !high);
 assert(!ngb_haptic_retry_ready(0,UINT32_MAX));assert(ngb_haptic_retry_ready(0,0));
 assert(!ngb_haptic_retry_ready(500,499));assert(ngb_haptic_retry_ready(500,500));
 assert(xbox_InputSetState(4,&vibration)==ERROR_DEVICE_NOT_CONNECTED);
 assert(xbox_InputSetState(0,0)==ERROR_DEVICE_NOT_CONNECTED);
 return 0;
}
'''
def main():
    source=(ROOT/'xboxrecomp/src/input/xinput_device.c').read_text()
    # The first definition is the Windows backend. Extract the actual SDL one.
    source=source[source.index('/* ====================  SDL2 GameController backend'):]
    bridge=function(source.replace('DWORD xbox_InputSetState(', 'static DWORD xbox_InputSetState(',1),'xbox_InputSetState')
    with tempfile.TemporaryDirectory() as directory:
        source=Path(directory)/'test.c';exe=Path(directory)/'test'
        source.write_text(PREFIX+bridge+MAIN)
        subprocess.run(['cc','-std=c11','-I'+str(ROOT/'ios'),str(source),'-o',str(exe)],check=True,capture_output=True)
        subprocess.run([str(exe)],check=True,capture_output=True)
    print('iOS haptics: production guest bridge has no engine/SDL call; latest of 10000 commands, one-second expiry, foreground silence and timer/retry wrap pass; no hardware driven')
if __name__=='__main__': main()
