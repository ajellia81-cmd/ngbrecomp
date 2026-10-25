"""Exercise production DSP completion and spin integration with synthetic RAM."""
from pathlib import Path
import re
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[1]
def function(path,name):
    source=path.read_text();m=re.search(r'^\w+ '+name+r'\([^;]*?\)\s*\{',source,re.M)
    if not m: raise ValueError(name)
    end=m.end();depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
PREFIX=r'''
#define _DEFAULT_SOURCE
#define XBOX_HAS_VCPU 0
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define XBOX_CONTIG_BASE 0x80000000u
#define XBOX_CONTIG_SIZE (128u*1024u*1024u)
static uint32_t g_ebx;
static ptrdiff_t g_xbox_mem_offset;
static int yields;
int SwitchToThread(void) { yields++;return 1; }
typedef int (*XBOX_SPIN_SERVICE)(uint32_t);
static XBOX_SPIN_SERVICE g_spin_service;
'''
MAIN=r'''
int main(void) {
    static uint32_t memory[0x30004/4];
    uint32_t *old=&memory[0],*moved=&memory[0x30000/4];
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory-(ptrdiff_t)0x86BF8810u;
    xbox_SetSpinService(ngb_ios_service_spin);
    setenv("RECOMP_DSP_ACK","auto",1);
    g_ebx=0x86BF8810u;*old=3;
    recomp_spin_pause(0x0037DC15u);assert(*old==0 && yields==0);
    *old=0xDEADBEEFu;*moved=3;g_ebx=0x86C28810u;
    recomp_spin_pause(0x0037DC15u);assert(*moved==0 && *old==0xDEADBEEFu && yields==0);
    *moved=3;recomp_spin_pause(0x1234);assert(*moved==3 && yields==1);
    *moved=2;recomp_spin_pause(0x0037DC15u);assert(*moved==2 && yields==2);
    *moved=3;setenv("RECOMP_DSP_ACK","",1);
    recomp_spin_pause(0x0037DC15u);assert(*moved==3 && yields==3);
    setenv("RECOMP_DSP_ACK","0x86BF8810",1);
    recomp_spin_pause(0x0037DC15u);assert(*moved==3 && yields==4);
    setenv("RECOMP_DSP_ACK","auto",1);g_ebx=0x86C28811u;
    recomp_spin_pause(0x0037DC15u);assert(*moved==3 && yields==5);
    g_ebx=0x1000;recomp_spin_pause(0x0037DC15u);assert(yields==6);
    return 0;
}
'''
def main():
    observer=function(ROOT/'src/main.c','ngb_ios_service_spin')
    spin=function(ROOT/'xboxrecomp/src/kernel/kernel_hal.c','recomp_spin_pause')
    setter=function(ROOT/'xboxrecomp/src/kernel/kernel_hal.c','xbox_SetSpinService')
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        for name,body,success in [('fixed',spin,True),('old',spin.replace(
                'if (g_spin_service && g_spin_service(loop_va)) return;',''),False)]:
            source=root/(name+'.c');exe=root/name;source.write_text(PREFIX+setter+observer+body+MAIN)
            subprocess.run(['cc','-std=c11',str(source),'-o',str(exe)],check=True,capture_output=True)
            result=subprocess.run([str(exe)],capture_output=True)
            assert (result.returncode==0)==success
        source=root/'no-hook.c';exe=root/'no-hook'
        source.write_text(PREFIX+setter+spin+'\nint main(void) { recomp_spin_pause(1); return yields!=1; }')
        subprocess.run(['cc','-std=c11',str(source),'-o',str(exe)],check=True,capture_output=True)
        subprocess.run([str(exe)],check=True)
    print('DSP: original/moved mailbox, unrelated word preservation, command/PC/address guards, explicit-off/manual policy and absent hook PASS; old spin rejected')
if __name__=='__main__':
    try: main()
    except subprocess.CalledProcessError as error:
        if error.stderr: sys.stderr.write(error.stderr.decode())
        raise
