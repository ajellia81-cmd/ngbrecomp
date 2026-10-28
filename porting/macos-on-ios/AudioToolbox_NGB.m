#import <AudioToolbox/AudioToolbox.h>
#include <dlfcn.h>
#include <stdio.h>
#define EXPORT __attribute__((visibility("default")))
static void *NgbAudioToolboxSymbol(const char *name) {
    return dlsym(dlopen("/System/Library/Frameworks/AudioToolbox.framework/AudioToolbox",RTLD_LAZY),name);
}
EXPORT OSStatus NgbAudioQueueSetProperty(AudioQueueRef,AudioQueuePropertyID,const void *,UInt32) __asm__("_AudioQueueSetProperty");
OSStatus NgbAudioQueueSetProperty(AudioQueueRef queue,AudioQueuePropertyID property,const void *data,UInt32 size) {
    // The synthetic desktop UID means the active iOS output route. Passing
    // that UID to iOS AudioQueue would reject an otherwise valid queue.
    if(property==kAudioQueueProperty_CurrentDevice)return noErr;
    OSStatus (*original)(AudioQueueRef,AudioQueuePropertyID,const void *,UInt32)=NgbAudioToolboxSymbol("AudioQueueSetProperty");
    return original ? original(queue,property,data,size) : (OSStatus)'unim';
}
EXPORT OSStatus NgbAudioQueueNewOutput(const AudioStreamBasicDescription *,AudioQueueOutputCallback,void *,CFRunLoopRef,CFStringRef,UInt32,AudioQueueRef *) __asm__("_AudioQueueNewOutput");
OSStatus NgbAudioQueueNewOutput(const AudioStreamBasicDescription *format,AudioQueueOutputCallback callback,void *context,CFRunLoopRef loop,CFStringRef mode,UInt32 flags,AudioQueueRef *queue) {
    OSStatus (*original)(const AudioStreamBasicDescription *,AudioQueueOutputCallback,void *,CFRunLoopRef,CFStringRef,UInt32,AudioQueueRef *)=NgbAudioToolboxSymbol("AudioQueueNewOutput");
    OSStatus status=original ? original(format,callback,context,loop,mode,flags,queue) : (OSStatus)'unim';
    fprintf(stderr,"[Audio] iOS output queue: status=%d sample-rate=%.0f channels=%u\n",(int)status,format ? format->mSampleRate : 0,format ? (unsigned)format->mChannelsPerFrame : 0);
    return status;
}
