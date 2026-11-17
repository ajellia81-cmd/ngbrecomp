/* No GPU, audio, controller or game data. Exercise the policies used by the
 * actual vblank scheduler, overlay placement and motor mailbox. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../frame_clock.h"
#include "../korean/haptics_policy.h"
#include "../../../xboxrecomp/src/nv2a_vk/host/native_viewport.h"
int main(void) {
    const int64_t period=16666666;
    int64_t previous=1000000000,now=previous;
    for(int i=0;i<60;i++) {
        previous=ngb_frame_clock_rebase(previous,now,period,1)+period;
        assert(previous>now);now=previous;
    }
    now+=30000000000LL; // app and its timer were suspended for thirty seconds
    previous=ngb_frame_clock_rebase(previous,now,period,1)+period;
    assert(previous==now+period); // first resumed vblank is in the future
    for(int i=0;i<120;i++) { // a 120 Hz display cannot pay a simulation backlog
        assert(previous>now);
        now+=period;
        previous=ngb_frame_clock_rebase(previous,now,period,1)+period;
        assert(previous-now==period);
    }
    assert(ngb_frame_clock_rebase(previous,now,period,0)==now);
    assert(ngb_frame_clock_rebase(now+period,now,period,1)==now+period);
    const double aspects[]={4.0/3,16.0/9,16.0/10,21.0/9,2868.0/1320};
    for(unsigned i=0;i<sizeof aspects/sizeof *aspects;i++) {
        uint32_t x,y,w,h,cw,ch;
        ngb_native_viewport(2868,1320,aspects[i],&cw,&ch);
        ngb_safe_overlay(2868,1320,aspects[i],.06f,.04f,.02f,.01f,260,40,&x,&y,&w,&h);
        assert(x>=173 && y>=53 && x>=(2868-cw)/2 && y>=(1320-ch)/2);
        assert(w==260 && h==40 && x+w<2868 && y+h<1320);
    }
    uint32_t x,y,w,h;
    ngb_safe_overlay(80,40,4.0/3,0,0,0,0,260,40,&x,&y,&w,&h);
    assert(x+w<=80 && y+h<=40); // clipping retains the original source stride
    uint16_t low,high;
    uint64_t command=ngb_haptic_command(65535,1234,100);
    ngb_haptic_values(command,100000,1,&low,&high);assert(low==65535 && high==1234);
    ngb_haptic_values(command,100000,0,&low,&high);assert(!low && !high);
    ngb_haptic_values(0,100000,1,&low,&high);assert(!low && !high);
    assert(!ngb_haptic_retry_ready(0,UINT32_MAX) && ngb_haptic_retry_ready(0,0));
    puts("PASS: resume discards simulation debt; safe-area FPS for five aspects; persistent motor command, explicit silence and retry wrap");
}
