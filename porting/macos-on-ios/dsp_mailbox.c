/* Complete the original passthrough-DSP handshake at the title's verified
 * wait sites. Korean font allocations relocate the mailbox, so a fixed
 * address is not valid for every language or contiguous-memory layout. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern ptrdiff_t g_xbox_mem_offset;
int ngb_conversion_dsp_wait(unsigned loop,unsigned base,unsigned parameter)
{
    if(loop!=0x0037DC15u && loop!=0x0037DC78u)return 0;
    const char *policy=getenv("RECOMP_DSP_ACK");
    if(!policy || strcmp(policy,"auto"))return 0;
    uint32_t address=loop==0x0037DC15u ? base : parameter+0x10u;
    if(!g_xbox_mem_offset || (address&3u) || address<0x80000000u ||
       (uint64_t)address+4u>0x88000000ull)return 0;
    uint32_t command=loop==0x0037DC15u ? 3u : 2u,expected=command;
    uint32_t *slot=(uint32_t *)((uintptr_t)g_xbox_mem_offset+address);
    if(!__atomic_compare_exchange_n(slot,&expected,0,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED))return 0;
    static unsigned reported;
    if(!(__atomic_fetch_or(&reported,1u<<command,__ATOMIC_RELAXED)&(1u<<command)))
        fprintf(stderr,"[Startup] DSP command %u completed at observed mailbox %08X\n",command,address);
    return 1;
}
