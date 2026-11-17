#import "../CoreAudio_NGB.m"
#include <assert.h>
#include <stdio.h>
int main(void){@autoreleasepool{
    NgbAudioAddress address={'dev#','glob',0};UInt32 size=0,device=0;
    assert(NgbAudioObjectGetPropertyDataSize(1,&address,0,NULL,&size)==noErr && size==sizeof device);
    assert(NgbAudioObjectGetPropertyData(1,&address,0,NULL,&size,&device)==noErr && device==2);
    address.selector='dOut';device=0;assert(NgbAudioObjectGetPropertyData(1,&address,0,NULL,&size,&device)==noErr && device==2);
    address.selector='dIn ';assert(NgbAudioObjectGetPropertyData(1,&address,0,NULL,&size,&device)==noErr && device==0);
    address.selector='slay';address.scope='outp';AudioBufferList buffers;size=sizeof buffers;
    assert(NgbAudioObjectGetPropertyData(2,&address,0,NULL,&size,&buffers)==noErr && buffers.mNumberBuffers==1 && buffers.mBuffers[0].mNumberChannels==2);
    address.scope='inpt';assert(NgbAudioObjectGetPropertyData(2,&address,0,NULL,&size,&buffers)==noErr && buffers.mBuffers[0].mNumberChannels==0);
    address.selector='uid ';address.scope='glob';CFStringRef uid=NULL;size=sizeof uid;
    assert(NgbAudioObjectGetPropertyData(2,&address,0,NULL,&size,&uid)==noErr && uid && CFStringGetLength(uid)>0);CFRelease(uid);
    address.selector='nsrt';Float64 rate=0;size=sizeof rate;
    assert(NgbAudioObjectGetPropertyData(2,&address,0,NULL,&size,&rate)==noErr && rate==48000);
    size=1;assert(NgbAudioObjectGetPropertyData(2,&address,0,NULL,&size,&rate)!=noErr && size==sizeof rate);
    puts("Mac audio-device bridge: default output, stereo format, UID ownership and buffer-size checks passed (no audio played)");
}return 0;}
