#define _GNU_SOURCE
#define _DARWIN_C_SOURCE 1
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
ptrdiff_t g_xbox_mem_offset;
int ngb_conversion_dsp_wait(unsigned,unsigned,unsigned);
int main(void){
    size_t size=128u<<20;
    unsigned char *ram=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(ram!=MAP_FAILED);
    g_xbox_mem_offset=(ptrdiff_t)((uintptr_t)ram-0x80000000u);
    uint32_t *old=(uint32_t *)(ram+0x6BF8810),*moved=(uint32_t *)(ram+0x6C28810);
    setenv("RECOMP_DSP_ACK","auto",1);
    *old=3;assert(ngb_conversion_dsp_wait(0x0037DC15,0x86BF8810,0) && *old==0);
    *old=0xDEADBEEF;*moved=3;assert(ngb_conversion_dsp_wait(0x0037DC15,0x86C28810,0) && *moved==0 && *old==0xDEADBEEF);
    *moved=3;assert(!ngb_conversion_dsp_wait(1,0x86C28810,0) && *moved==3);
    *moved=2;assert(!ngb_conversion_dsp_wait(0x0037DC15,0x86C28810,0) && *moved==2);
    assert(ngb_conversion_dsp_wait(0x0037DC78,0,0x86C28800) && *moved==0);
    *moved=3;
    for(unsigned i=0;i<3;i++){const char *policies[]={"","0x86BF8810","off"};setenv("RECOMP_DSP_ACK",policies[i],1);assert(!ngb_conversion_dsp_wait(0x0037DC15,0x86C28810,0) && *moved==3);}
    setenv("RECOMP_DSP_ACK","auto",1);
    assert(!ngb_conversion_dsp_wait(0x0037DC15,0x86C28811,0));
    assert(!ngb_conversion_dsp_wait(0x0037DC15,0x88000000,0));
    assert(!ngb_conversion_dsp_wait(0x0037DC15,0x1000,0));
    assert(!ngb_conversion_dsp_wait(0x0037DC78,0,0xfffffff0));
    assert(*old==0xDEADBEEF && *moved==3);
    munmap(ram,size);puts("DSP mailbox: original/relocated addresses, commands, wait sites, bounds and manual/off policies passed");return 0;
}
