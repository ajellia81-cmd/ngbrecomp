#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#include <SDL.h>
#include <dlfcn.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bridge.h"
#include "launcher.h"
#include "install.h"
extern void apu_host_pause(int);
extern volatile int apu_host_paused;
static int audio_paused(void){return apu_host_paused;}
extern BOOL ngb_prepare_korean(NSString *,NSString *,NSString *,void (^)(NSString *),NSError **);
static int active,override=-1,converted,w=1280,h=720;
int ngb_korean_active(void){return active;}
int ngb_conversion_is_ios(void){return converted;}
int ngb_ios_text_override(void){return override;}
void ngb_ios_language_changed(int language){
    void (*notice)(int)=(void(*)(int))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_language_changed");
    if(notice){notice(language);return;}
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Restart required","Quit and reopen to apply the text language.",NULL);
}
void ngb_ios_screen_size(int *width,int *height){*width=w;*height=h;}
extern int ngb_pc_setting(int);
static void (*bridge_tick)(void),(*bridge_wait)(void),(*bridge_motion)(int);
static double (*bridge_hz)(void);
static unsigned (*bridge_epoch)(void);
static int (*bridge_foreground)(void);
static void (*bridge_safe_area)(float *,float *,float *,float *);
void ngb_ios_safe_area(float *left,float *top,float *right,float *bottom){if(bridge_safe_area)bridge_safe_area(left,top,right,bottom);else{*left=*right=.08f;*top=.03f;*bottom=.02f;}}
extern void nv2a_present_cancel_pairs(void);
int ngb_conversion_is_foreground(void){return bridge_foreground ? bridge_foreground() : 1;}
double ngb_ios_display_hz(void){return bridge_hz ? bridge_hz() : 60;}
unsigned ngb_ios_motion_epoch(void){return bridge_epoch ? bridge_epoch() : 0;}
void ngb_conversion_tick(void){if(bridge_motion)bridge_motion(ngb_pc_setting(26));if(bridge_tick)bridge_tick();}
void ngb_conversion_wait_foreground(void){if(bridge_wait)bridge_wait();}
int ngb_ios_settings_tab_mask(uint8_t *mask, int width, int height) {
    @autoreleasepool {
        if (!mask || width<240 || height<128) return 0;
        const char *fixture=getenv("NGB_SETTINGS_ART");
        NSString *path=fixture ? [NSString stringWithUTF8String:fixture] :
            [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"SettingsTab-Korean.png"];
        NSData *data=[NSData dataWithContentsOfFile:path];
        if (!data) return 0;
        CGImageSourceRef source=CGImageSourceCreateWithData((__bridge CFDataRef)data,NULL);
        if (!source) return 0;
        CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,NULL);
        CFRelease(source);
        if (!image) return 0;
        size_t iw=CGImageGetWidth(image),ih=CGImageGetHeight(image);
        // This independently generated lettering contains no original game pixels.
        // The fixed crop excludes its transparent margins and faint edge noise.
        if (iw!=1774 || ih!=887) { CGImageRelease(image); return 0; }
        uint8_t *rgba=calloc(iw*ih,4);
        CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
        CGContextRef context=rgba ? CGBitmapContextCreate(rgba,iw,ih,8,iw*4,space,
            kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big) : NULL;
        CGColorSpaceRelease(space);
        if (!context) { free(rgba); CGImageRelease(image); return 0; }
        CGContextDrawImage(context,CGRectMake(0,0,iw,ih),image);
        CGImageRelease(image);CGContextRelease(context);
        memset(mask,0,(size_t)width*height);
        const unsigned w=240,h=112;
        for (unsigned y=0;y<h;y++) for (unsigned x=0;x<w;x++) {
            double x0=503+x*768.0/w,x1=503+(x+1)*768.0/w;
            double y0=277+y*357.0/h,y1=277+(y+1)*357.0/h;
            double coverage=0;
            for (unsigned sy=(unsigned)floor(y0);sy<(unsigned)ceil(y1);sy++)
                for (unsigned sx=(unsigned)floor(x0);sx<(unsigned)ceil(x1);sx++) {
                    double area=(fmin(sx+1,x1)-fmax(sx,x0))*
                                (fmin(sy+1,y1)-fmax(sy,y0));
                    coverage+=rgba[(sy*iw+sx)*4+3]*area;
                }
            mask[y*width+x]=(uint8_t)lround(coverage/((x1-x0)*(y1-y0)));
        }
        // Lightly thin the generated strokes before the final plate resample.
        // Work on coverage, preserving the transparent image and its orientation.
        uint8_t original[240*112];
        for (unsigned y=0;y<h;y++) memcpy(original+y*w,mask+y*width,w);
        for (unsigned y=1;y+1<h;y++) for (unsigned x=1;x+1<w;x++) {
            unsigned v=original[y*w+x],edge=v;
            for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++)
                if (original[(y+dy)*w+x+dx]<edge) edge=original[(y+dy)*w+x+dx];
            mask[y*width+x]=edge;
        }
        free(rgba);
        return 1;
    }
}


int ngb_conversion_startup(void){@autoreleasepool {
    NgbConversionPrepare prepare=(NgbConversionPrepare)dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_prepare");
    converted=prepare!=NULL;
    if(converted){
        NSString *documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
        NSString *log=[documents stringByAppendingPathComponent:@"macos-on-ios-startup.log"];
        if(log){
            FILE *fresh=fopen(log.fileSystemRepresentation,"w");if(fresh)fclose(fresh);
            freopen(log.fileSystemRepresentation,"a",stdout);
            freopen(log.fileSystemRepresentation,"a",stderr);
            setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
        }
        fprintf(stderr,"[Startup] 3.0 diagnostic: starting UIKit bridge\n");
        setenv("NV2A_FPS_LOG","1",0);
        // Presentation may wait for UIKit to retire a drawable. Keep that
        // wait on MoltenVK's submission worker, outside the game thread.
        setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS","0",0);
        fprintf(stderr,"[Display] asynchronous presentation submits: %s\n",getenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS"));
        setenv("RECOMP_DSP_ACK","auto",0);
        [NSApplication sharedApplication];
        [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        NgbConversionHost api={2,sizeof(NgbConversionHost),ngb_check_xbe,ngb_install,ngb_install_set_progress,
            SDL_WasInit,SDL_InitSubSystem,(int(*)(int,int,int,int))SDL_JoystickAttachVirtual,(void *(*)(int))SDL_JoystickOpen,
            (int(*)(void*,int,int16_t))SDL_JoystickSetVirtualAxis,(int(*)(void*,int,uint8_t))SDL_JoystickSetVirtualButton,
            (int32_t(*)(void*))SDL_JoystickInstanceID,(void(*)(void*))SDL_JoystickClose,SDL_NumJoysticks,
            SDL_JoystickGetDeviceInstanceID,SDL_JoystickDetachVirtual,apu_host_pause,audio_paused,nv2a_present_cancel_pairs};
        fprintf(stderr,"[Startup] preparing import and device services\n");
        if(!prepare(&api)){fprintf(stderr,"[Startup] preparation failed or cancelled\n");return 0;}
        fprintf(stderr,"[Startup] import and device services complete\n");
        void (*screen)(int*,int*)=(void(*)(int*,int*))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_screen_size");
        if(screen)screen(&w,&h);
        bridge_safe_area=(void(*)(float*,float*,float*,float*))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_safe_area");
        bridge_foreground=(int(*)(void))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_is_foreground");
        bridge_motion=(void(*)(int))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_motion_enabled");
        bridge_hz=(double(*)(void))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_display_hz");
        bridge_epoch=(unsigned(*)(void))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_motion_epoch");
        bridge_tick=(void(*)(void))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_tick");
        bridge_wait=(void(*)(void))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_wait_foreground");
    }
    NSString *resources=NSBundle.mainBundle.resourcePath;
    NSString *seed=[resources stringByAppendingPathComponent:@"share/NinjaGaidenBlack/d3d-seed.bin"];
    NSString *vkseed=[resources stringByAppendingPathComponent:@"share/NinjaGaidenBlack/shader-seed"];
    NSString *icd=[resources stringByAppendingPathComponent:@"vulkan/icd.d/MoltenVK_icd.json"];
    NSString *loader=[resources stringByAppendingPathComponent:@"../Frameworks/libvulkan.1.dylib"];
    setenv("NGB_D3D_SEED",seed.fileSystemRepresentation,0);setenv("NV2A_SHADER_SEED",vkseed.fileSystemRepresentation,0);
    setenv("VK_DRIVER_FILES",icd.fileSystemRepresentation,0);setenv("SDL_VULKAN_LIBRARY",loader.fileSystemRepresentation,0);
    fprintf(stderr,"[Startup] loading settings\n");
    NgbSettings settings;ngb_settings_load(&settings);
    if(bridge_motion)bridge_motion(settings.smooth_motion);
    override=settings.text_language_options ? (settings.text_language==1 ? 1 : 0) : -1;
    setenv("NGB_TEXT_LANGUAGE",settings.text_language==1 ? "English" : "Japanese",1);
    if(settings.text_language!=2){fprintf(stderr,"[Startup] entering original Mac engine\n");return 1;}
    fprintf(stderr,"[Startup] preparing Korean resources\n");
    const char *original=getenv("NGB_GAME_DIR");if(!original)return 1;
    NSString *documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    NSString *output=[documents stringByAppendingPathComponent:@"korean-game"];
    NSString *assets=[resources stringByAppendingPathComponent:@"KoreanPatch"];
    __block BOOL done=NO,ok=NO;__block NSError *preparationError=nil;
    NSString *originalPath=[NSString stringWithUTF8String:original];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSError *failure=nil;
        BOOL result=ngb_prepare_korean(originalPath,output,assets,^(NSString *status){(void)status;},&failure);
        dispatch_async(dispatch_get_main_queue(),^{ok=result;preparationError=failure;done=YES;});
    });
    while(!done)@autoreleasepool{[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];}
    if(!ok){fprintf(stderr,"Korean game data preparation failed: %s\n",preparationError.localizedDescription.UTF8String);return 0;}
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *saves=[documents stringByAppendingPathComponent:@"saves"];
    NSString *localized=[documents stringByAppendingPathComponent:@"saves-korean"];
    [fm createDirectoryAtPath:localized withIntermediateDirectories:YES attributes:nil error:nil];
    for(NSString *name in @[@"UserData",@"TitleData"]){
        NSString *source=[saves stringByAppendingPathComponent:name],*target=[localized stringByAppendingPathComponent:name];
        [fm createDirectoryAtPath:source withIntermediateDirectories:YES attributes:nil error:nil];
        if(![fm fileExistsAtPath:target])[fm createSymbolicLinkAtPath:target withDestinationPath:source error:nil];
    }
    setenv("NGB_SAVE_DIR",localized.fileSystemRepresentation,1);setenv("NGB_GAME_DIR",output.fileSystemRepresentation,1);
    active=1;override=0;fprintf(stderr,"[Startup] Korean resources ready; entering original Mac engine\n");return 1;
}}

extern int ngb_pc_setting(int);
void ngb_conversion_render_size(int *width,int *height){
    int sw,sh;ngb_ios_screen_size(&sw,&sh);NgbSettings settings={0};settings.aspect=ngb_pc_setting(7);
    double ratio=ngb_settings_aspect_ratio(&settings);int w=sw,h=(int)lround(sw/ratio);
    if(h>sh){h=sh;w=(int)lround(sh*ratio);}*width=w;*height=h;
}
