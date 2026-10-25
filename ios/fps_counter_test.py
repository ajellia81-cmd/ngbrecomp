"""Compile the production FPS sampler with runtime-library platform definitions."""
from pathlib import Path
import subprocess
import tempfile
from presenter_descriptor_test import function, SOURCE

PREFIX=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint64_t now_ns=1000000000,mid,end;
volatile unsigned nv2a_pcrtc_start_writes;
unsigned long nv2a_early_presents;
static char s_fps_text[48]="-- FPS";
static int test_clock(int which,struct timespec *t) {
 (void)which;t->tv_sec=now_ns/1000000000;t->tv_nsec=now_ns%1000000000;return 0;
}
#define clock_gettime test_clock
void nv2a_present_pair_counts(uint64_t *m,uint64_t *e,uint64_t *s) { *m=mid;*e=end;*s=0; }
'''
MAIN=r'''
int main(void) {
 unsetenv("NV2A_FPS_LOG");fps_track();
 for(unsigned i=1;i<=30;i++) {
  now_ns=1000000000+(uint64_t)i*16666667;
  nv2a_pcrtc_start_writes=i;mid=end=i;fps_track();
 }
 unsigned display=0,game=0;
#if defined(__APPLE__) && TARGET_OS_IPHONE
 assert(sscanf(s_fps_text,"%u FPS game %u",&display,&game)==2);
 assert(display==120 && game==60);
#else
 assert(sscanf(s_fps_text,"%u FPS",&display)==1 && display==60);
 assert(!strstr(s_fps_text,"game"));
#endif
 // No new pairs in a static menu: do not continue advertising 120 FPS.
 for(unsigned i=31;i<=60;i++) {
  now_ns=1000000000+(uint64_t)i*16666667;
  nv2a_pcrtc_start_writes=i;fps_track();
 }
 assert(sscanf(s_fps_text,"%u FPS",&display)==1 && display==60);
 assert(!strstr(s_fps_text,"game"));
 return 0;
}
'''
def main():
    production=function(SOURCE.read_text(),'fps_track')
    guard='#if defined(__APPLE__) && TARGET_OS_IPHONE'
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        cases=[('ios',production,True,True),('old-app-flag',production.replace(guard,'#ifdef NGB_IOS_APP'),True,False),('desktop',production,False,True)]
        for name,body,ios,success in cases:
            source=root/(name+'.c');exe=root/name
            platform='\n#define __APPLE__ 1\n#define TARGET_OS_IPHONE 1\n' if ios else '\n#undef __APPLE__\n#define TARGET_OS_IPHONE 0\n'
            source.write_text(PREFIX+platform+body+MAIN)
            subprocess.run(['cc','-std=c11',str(source),'-lm','-o',str(exe)],check=True,capture_output=True)
            result=subprocess.run([str(exe)],capture_output=True)
            assert (result.returncode==0)==success,(name,result.stderr)
    print('FPS: runtime-library iOS reports 120 submitted pictures/game 60, menu returns to 60, desktop unchanged; missing app-flag regression rejected')
if __name__=='__main__': main()
