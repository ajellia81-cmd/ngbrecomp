// SDL's Mac CoreAudio backend needs a desktop-style logical output device.
// AudioQueue on iOS follows the active system route; no microphone is exposed.
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include <string.h>
#include <stdint.h>

typedef struct { UInt32 selector,scope,element; } NgbAudioAddress;
typedef OSStatus (*NgbAudioListener)(UInt32,UInt32,const NgbAudioAddress *,void *);
enum { NgbSystem=1,NgbOutput=2 };
static OSStatus NgbAudioProperty(UInt32 object,const NgbAudioAddress *address,UInt32 *size,void *data) {
    if(!address || !size)return (OSStatus)'!siz';
    UInt32 integer=0;Float64 rate=48000;CFStringRef text=NULL;
    AudioBufferList streams={0};const void *value=NULL;UInt32 needed=0;
    if(object==NgbSystem){
        switch(address->selector){
            case 'dev#':case 'dOut':case 'sOut':integer=NgbOutput;break;
            case 'dIn ':integer=0;break;
            default:return (OSStatus)'who?';
        }
        value=&integer;needed=sizeof integer;
    }else if(object==NgbOutput){
        switch(address->selector){
            case 'lnam':text=CFSTR("iOS System Output");value=&text;needed=sizeof text;break;
            case 'uid ':text=CFSTR("ngb-ios-default-output");value=&text;needed=sizeof text;break;
            case 'livn':integer=1;value=&integer;needed=sizeof integer;break;
            case 'oink':integer=UINT32_MAX;value=&integer;needed=sizeof integer;break;
            case 'fsiz':integer=480;value=&integer;needed=sizeof integer;break;
            case 'nsrt':value=&rate;needed=sizeof rate;break;
            case 'slay':
                streams.mNumberBuffers=1;
                streams.mBuffers[0].mNumberChannels=address->scope=='inpt' ? 0 : 2;
                value=&streams;needed=sizeof streams;break;
            default:return (OSStatus)'who?';
        }
    }else return (OSStatus)'!obj';
    if(data){
        if(*size<needed){*size=needed;return (OSStatus)'!siz';}
        // SDL releases both device-name and UID property results.
        if(text)CFRetain(text);
        memcpy(data,value,needed);
    }
    *size=needed;return noErr;
}
#define EXPORT __attribute__((visibility("default")))
EXPORT OSStatus NgbAudioObjectGetPropertyDataSize(UInt32,const NgbAudioAddress *,UInt32,const void *,UInt32 *) __asm__("_AudioObjectGetPropertyDataSize");
OSStatus NgbAudioObjectGetPropertyDataSize(UInt32 object,const NgbAudioAddress *address,UInt32 qualifierSize,const void *qualifier,UInt32 *size) {
    (void)qualifierSize;(void)qualifier;return NgbAudioProperty(object,address,size,NULL);
}
EXPORT OSStatus NgbAudioObjectGetPropertyData(UInt32,const NgbAudioAddress *,UInt32,const void *,UInt32 *,void *) __asm__("_AudioObjectGetPropertyData");
OSStatus NgbAudioObjectGetPropertyData(UInt32 object,const NgbAudioAddress *address,UInt32 qualifierSize,const void *qualifier,UInt32 *size,void *data) {
    (void)qualifierSize;(void)qualifier;if(!data)return (OSStatus)'!siz';return NgbAudioProperty(object,address,size,data);
}
EXPORT Boolean NgbAudioObjectHasProperty(UInt32,const NgbAudioAddress *) __asm__("_AudioObjectHasProperty");
Boolean NgbAudioObjectHasProperty(UInt32 object,const NgbAudioAddress *address) {UInt32 size=0;return NgbAudioProperty(object,address,&size,NULL)==noErr;}
EXPORT OSStatus NgbAudioObjectIsPropertySettable(UInt32,const NgbAudioAddress *,Boolean *) __asm__("_AudioObjectIsPropertySettable");
OSStatus NgbAudioObjectIsPropertySettable(UInt32 object,const NgbAudioAddress *address,Boolean *settable) {
    if(settable)*settable=false;return NgbAudioObjectHasProperty(object,address) ? noErr : (OSStatus)'who?';
}
EXPORT OSStatus NgbAudioObjectAddPropertyListener(UInt32,const NgbAudioAddress *,NgbAudioListener,void *) __asm__("_AudioObjectAddPropertyListener");
OSStatus NgbAudioObjectAddPropertyListener(UInt32 object,const NgbAudioAddress *address,NgbAudioListener listener,void *context) {
    (void)listener;(void)context;return NgbAudioObjectHasProperty(object,address) ? noErr : (OSStatus)'who?';
}
EXPORT OSStatus NgbAudioObjectRemovePropertyListener(UInt32,const NgbAudioAddress *,NgbAudioListener,void *) __asm__("_AudioObjectRemovePropertyListener");
OSStatus NgbAudioObjectRemovePropertyListener(UInt32 object,const NgbAudioAddress *address,NgbAudioListener listener,void *context) {
    (void)object;(void)address;(void)listener;(void)context;return noErr;
}
