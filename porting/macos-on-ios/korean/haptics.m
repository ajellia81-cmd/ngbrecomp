/* Controller-only haptics: never initialize CoreHaptics on the guest/input lock. */
#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#include <time.h>
#include "haptics.h"
#include <stdio.h>
static unsigned hapticReports;
#define NGB_IOS_PORT_DIAG(...) do { if (hapticReports++<32) fprintf(stderr,__VA_ARGS__); } while (0)
#include "haptics_policy.h"
static uint64_t commands[4];
static int foreground=1;
static dispatch_queue_t hapticQueue;
static dispatch_source_t hapticTimer;
static int hapticReady;
static uint32_t haptic_ms(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint32_t)((uint64_t)t.tv_sec*1000+t.tv_nsec/1000000);
}

@interface NGBHapticMotor : NSObject
@property(nonatomic,strong) GCController *controller;
@property(nonatomic,copy) GCHapticsLocality locality;
@property(nonatomic,strong) CHHapticEngine *engine;
@property(nonatomic,strong) id<CHHapticAdvancedPatternPlayer> player;
@property(nonatomic) unsigned generation;
@property(nonatomic) uint32_t retryAt;
@property(nonatomic) BOOL retryPending;
@property(nonatomic) float intensity;
- (void)stop;
- (void)update:(float)value now:(uint32_t)now;
@end
@implementation NGBHapticMotor
- (void)stop {
    self.generation++;
    [self.player stopAtTime:0 error:nil];self.player=nil;
    [self.engine stopWithCompletionHandler:nil];self.engine=nil;
    self.intensity=-1;
}
- (void)failed:(NSString *)reason now:(uint32_t)now {
    NGB_IOS_PORT_DIAG("[iOS Haptics] %s: %s; retry in 100ms\n",self.controller.vendorName.UTF8String,reason.UTF8String);
    [self stop];self.retryAt=now+100;self.retryPending=YES;
}
- (BOOL)prepare:(uint32_t)now {
    if (self.engine && self.player) return YES;
    if (self.retryPending && !ngb_haptic_retry_ready(self.retryAt,now)) return NO;
    NSError *error=nil;
    CHHapticEngine *engine=[self.controller.haptics createEngineWithLocality:self.locality];
    if (!engine || ![engine startAndReturnError:&error]) {
        [self failed:error.localizedDescription ?: @"engine unavailable" now:now];return NO;
    }
    engine.autoShutdownEnabled=NO;
    self.engine=engine;
    unsigned token=++self.generation;
    __weak NGBHapticMotor *weakSelf=self;
    engine.stoppedHandler=^(CHHapticEngineStoppedReason reason) {
        dispatch_async(hapticQueue,^{
            NGBHapticMotor *motor=weakSelf;
            if (motor && motor.generation==token)
                [motor failed:[NSString stringWithFormat:@"engine stopped (%ld)",(long)reason] now:haptic_ms()];
        });
    };
    engine.resetHandler=^{
        dispatch_async(hapticQueue,^{
            NGBHapticMotor *motor=weakSelf;
            if (motor && motor.generation==token) [motor failed:@"engine reset" now:haptic_ms()];
        });
    };
    CHHapticEventParameter *intensity=[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1];
    CHHapticEvent *event=[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[intensity] relativeTime:0 duration:GCHapticDurationInfinite];
    CHHapticPattern *pattern=[[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
    id<CHHapticAdvancedPatternPlayer> player=pattern ? [engine createAdvancedPlayerWithPattern:pattern error:&error] : nil;
    player.completionHandler=^(NSError *failure) {
        dispatch_async(hapticQueue,^{NGBHapticMotor *motor=weakSelf;if(motor && motor.generation==token)[motor failed:failure.localizedDescription ?: @"player completed" now:haptic_ms()];});
    };
    CHHapticDynamicParameter *silent=[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:0 relativeTime:0];
    // Prepare the actual reusable player at zero intensity. No warmup pulse.
    if (!player || ![player sendParameters:@[silent] atTime:0 error:&error] || ![player startAtTime:0 error:&error]) {
        [self failed:error.localizedDescription ?: @"player unavailable" now:now];return NO;
    }
    self.player=player;self.intensity=0;self.retryPending=NO;
    NGB_IOS_PORT_DIAG("[iOS Haptics] %s / %s ready (silent prewarm)\n",self.controller.vendorName.UTF8String,self.locality.UTF8String);
    return YES;
}
- (void)update:(float)value now:(uint32_t)now {
    if (![self prepare:now] || value==self.intensity) return;
    CHHapticDynamicParameter *parameter=[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:value relativeTime:0];
    NSError *error=nil;
    if (![self.player sendParameters:@[parameter] atTime:0 error:&error]) {
        [self failed:error.localizedDescription ?: @"motor update failed" now:now];return;
    }
    self.intensity=value;
}
@end

@interface NGBHapticPad : NSObject
@property(nonatomic,strong) GCController *controller;
@property(nonatomic,strong) NGBHapticMotor *low;
@property(nonatomic,strong) NGBHapticMotor *high;
@end
@implementation NGBHapticPad
@end
static NSMutableDictionary<NSNumber *,NGBHapticPad *> *pads;
static unsigned preparedPorts;
static NGBHapticMotor *new_motor(GCController *controller,GCHapticsLocality locality) {
    NGBHapticMotor *motor=[NGBHapticMotor new];motor.controller=controller;motor.locality=locality;motor.intensity=-1;return motor;
}
static void haptic_tick(void) {
    @autoreleasepool {
        uint32_t now=haptic_ms();int active=__atomic_load_n(&foreground,__ATOMIC_ACQUIRE);
        NSArray<GCController *> *controllers=GCController.controllers;
        for (unsigned port=0;port<4;port++) {
            if (!(preparedPorts&(1u<<port))) continue;
            GCController *controller=nil;
            for (GCController *candidate in controllers)
                if (candidate.playerIndex==(GCControllerPlayerIndex)port) { controller=candidate;break; }
            if (!controller && port==0 && controllers.count==1 && controllers.firstObject.playerIndex==GCControllerPlayerIndexUnset) controller=controllers.firstObject;
            NGBHapticPad *pad=pads[@(port)];
            if (pad && (pad.controller!=controller || !active)) {
                [pad.low stop];[pad.high stop];[pads removeObjectForKey:@(port)];pad=nil;
                __atomic_store_n(&commands[port],0,__ATOMIC_RELEASE);
            }
            if (!active || !controller.haptics) continue;
            if (!pad) {
                NSSet<GCHapticsLocality> *localities=controller.haptics.supportedLocalities;
                BOOL dual=[localities containsObject:GCHapticsLocalityLeftHandle] && [localities containsObject:GCHapticsLocalityRightHandle];
                if (!dual && ![localities containsObject:GCHapticsLocalityDefault]) continue;
                pad=[NGBHapticPad new];pad.controller=controller;
                pad.low=new_motor(controller,dual ? GCHapticsLocalityLeftHandle : GCHapticsLocalityDefault);
                if (dual) pad.high=new_motor(controller,GCHapticsLocalityRightHandle);
                pads[@(port)]=pad;
            }
            uint16_t low,high;
            ngb_haptic_values(__atomic_load_n(&commands[port],__ATOMIC_ACQUIRE),now,active,&low,&high);
            [pad.low update:(pad.high ? low : MAX(low,high))/65535.0f now:now];
            [pad.high update:high/65535.0f now:now];
        }
    }
}
static void haptic_init(void) {
    static dispatch_once_t once;
    dispatch_once(&once,^{
        hapticQueue=dispatch_queue_create("ngb.controller.haptics",DISPATCH_QUEUE_SERIAL);
        pads=[NSMutableDictionary new];
        hapticTimer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,hapticQueue);
        dispatch_source_set_timer(hapticTimer,DISPATCH_TIME_NOW,NSEC_PER_SEC/120,NSEC_PER_MSEC);
        dispatch_source_set_event_handler(hapticTimer,^{ haptic_tick(); });
        dispatch_resume(hapticTimer);
        __atomic_store_n(&hapticReady,1,__ATOMIC_RELEASE);
    });
}
void ngb_ios_haptics_prepare(unsigned port) {
    if (port>=4) return;
    __atomic_store_n(&commands[port],0,__ATOMIC_RELEASE);
    haptic_init();
    dispatch_async(hapticQueue,^{ preparedPorts|=1u<<port;haptic_tick(); });
}
void ngb_ios_haptics_set(unsigned port,uint16_t low,uint16_t high) {
    if (port<4) __atomic_store_n(&commands[port],ngb_haptic_command(low,high,haptic_ms()),__ATOMIC_RELEASE);
}
void ngb_ios_haptics_active(int active) {
    if (__atomic_exchange_n(&foreground,!!active,__ATOMIC_ACQ_REL)==!!active) return;
    for (unsigned port=0;port<4;port++) __atomic_store_n(&commands[port],0,__ATOMIC_RELEASE);
    if (__atomic_load_n(&hapticReady,__ATOMIC_ACQUIRE)) dispatch_async(hapticQueue,^{ haptic_tick(); });
}
