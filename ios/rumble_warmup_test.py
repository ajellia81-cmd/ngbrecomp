"""Exercise the production connection warmup without a controller or vibration."""
from pathlib import Path
import subprocess
import tempfile
from presenter_descriptor_test import function
ROOT=Path(__file__).resolve().parents[1]
PREFIX=r'''
#include <stdio.h>
#include <assert.h>
typedef int SDL_GameController;
typedef unsigned long long Uint64;
static int supported,status,calls;
int SDL_GameControllerHasRumble(SDL_GameController *c) { (void)c;return supported; }
Uint64 SDL_GetTicks64(void) { return calls; }
int SDL_GameControllerRumble(SDL_GameController *c,int low,int high,int ms) {
 (void)c;
 if(!calls) assert(low==1 && high==1 && ms==1);
 else assert(low==0 && high==0 && ms==0);
 calls++;return status;
}
'''
MAIN=r'''
int main(void) {
 int pad=0;
 for(int failure=0;failure<2;failure++) {
  supported=1;status=failure ? -1 : 0;calls=0;
  warm_controller_rumble(&pad);assert(calls==2);
 }
 supported=0;calls=0;warm_controller_rumble(&pad);assert(calls==0);
 return 0;
}
'''
def main():
    source=ROOT/'xboxrecomp/src/input/xinput_device.c'
    warm=function(source.read_text(),'warm_controller_rumble')
    with tempfile.TemporaryDirectory() as directory:
        source=Path(directory)/'test.c';exe=Path(directory)/'test'
        source.write_text(PREFIX+warm+MAIN)
        subprocess.run(['cc','-std=c11',str(source),'-o',str(exe)],check=True,capture_output=True)
        subprocess.run([str(exe)],check=True,capture_output=True)
    print('Rumble warmup: minimum two-motor start immediately stops, failure still stops, unsupported pad skipped; no hardware was driven')
if __name__=='__main__': main()
