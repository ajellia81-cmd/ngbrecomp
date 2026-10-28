/* Exercise the production motor with fake controller engines and players.
 * No hardware engine, audio session, window or game data is created. */
#import "../korean/haptics.m"
#include <assert.h>
@interface TestPlayer : NSObject
@property(copy) void (^completionHandler)(NSError *);
@property float value;
@property unsigned starts;
- (BOOL)sendParameters:(NSArray<CHHapticDynamicParameter *> *)parameters atTime:(NSTimeInterval)time error:(NSError **)error;
- (BOOL)startAtTime:(NSTimeInterval)time error:(NSError **)error;
- (BOOL)stopAtTime:(NSTimeInterval)time error:(NSError **)error;
@end
@implementation TestPlayer
- (BOOL)sendParameters:(NSArray<CHHapticDynamicParameter *> *)parameters atTime:(NSTimeInterval)time error:(NSError **)error {
    (void)time;(void)error;self.value=parameters.lastObject.value;return YES;
}
- (BOOL)startAtTime:(NSTimeInterval)time error:(NSError **)error {(void)time;(void)error;self.starts++;return YES;}
- (BOOL)stopAtTime:(NSTimeInterval)time error:(NSError **)error {
    (void)time;(void)error;if(self.completionHandler)self.completionHandler(nil);return YES;
}
@end
@interface TestEngine : NSObject
@property(copy) void (^stoppedHandler)(CHHapticEngineStoppedReason);
@property(copy) void (^resetHandler)(void);
@property BOOL autoShutdownEnabled;
@property(strong) TestPlayer *testPlayer;
- (BOOL)startAndReturnError:(NSError **)error;
- (void)stopWithCompletionHandler:(void (^)(NSError *))handler;
- (id<CHHapticAdvancedPatternPlayer>)createAdvancedPlayerWithPattern:(CHHapticPattern *)pattern error:(NSError **)error;
@end
@implementation TestEngine
- (BOOL)startAndReturnError:(NSError **)error {(void)error;return YES;}
- (void)stopWithCompletionHandler:(void (^)(NSError *))handler {(void)handler;if(self.stoppedHandler)self.stoppedHandler(CHHapticEngineStoppedReasonNotifyWhenFinished);}
- (id<CHHapticAdvancedPatternPlayer>)createAdvancedPlayerWithPattern:(CHHapticPattern *)pattern error:(NSError **)error {
    (void)error;assert(pattern);self.testPlayer=[TestPlayer new];return (id)self.testPlayer;
}
@end
@interface TestHaptics : NSObject
@property unsigned created;
@property(strong) TestEngine *latest;
- (CHHapticEngine *)createEngineWithLocality:(GCHapticsLocality)locality;
@end
@implementation TestHaptics
- (CHHapticEngine *)createEngineWithLocality:(GCHapticsLocality)locality {
    (void)locality;self.created++;self.latest=[TestEngine new];self.latest.autoShutdownEnabled=YES;return (id)self.latest;
}
@end
@interface TestController : NSObject
@property(strong) TestHaptics *haptics;
@property(readonly) NSString *vendorName;
@end
@implementation TestController
- (NSString *)vendorName {return @"synthetic controller";}
@end
static void flush_callbacks(void){dispatch_sync(hapticQueue,^{});}
int main(void){@autoreleasepool {
    hapticQueue=dispatch_queue_create("ngb.test.haptics",DISPATCH_QUEUE_SERIAL);
    TestController *controller=[TestController new];controller.haptics=[TestHaptics new];
    NGBHapticMotor *motor=[NGBHapticMotor new];motor.controller=(id)controller;motor.locality=GCHapticsLocalityDefault;
    [motor update:0 now:haptic_ms()];
    assert(controller.haptics.created==1 && motor.engine && motor.player);
    assert(controller.haptics.latest.testPlayer.value==0 && !controller.haptics.latest.autoShutdownEnabled);
    [motor update:.75f now:haptic_ms()];assert(controller.haptics.latest.testPlayer.value==.75f);
    TestPlayer *old=controller.haptics.latest.testPlayer;
    old.completionHandler(nil);flush_callbacks();assert(!motor.player && motor.retryPending);
    unsigned made=controller.haptics.created;
    [motor update:.75f now:motor.retryAt-1];assert(controller.haptics.created==made);
    [motor update:.75f now:motor.retryAt];flush_callbacks();
    assert(controller.haptics.created==made+1 && motor.player && controller.haptics.latest.testPlayer.value==.75f);
    old.completionHandler(nil);flush_callbacks();assert(motor.player); // stale completion cannot kill the replacement
    controller.haptics.latest.resetHandler();flush_callbacks();assert(!motor.player);
    [motor update:.5f now:motor.retryAt];flush_callbacks();assert(motor.player);
    controller.haptics.latest.stoppedHandler(CHHapticEngineStoppedReasonIdleTimeout);flush_callbacks();assert(!motor.player);
    [motor update:0 now:motor.retryAt];flush_callbacks();assert(motor.player && controller.haptics.latest.testPlayer.value==0);
    [motor stop];flush_callbacks();assert(!motor.engine && !motor.player);
    puts("PASS: actual haptic motor silently prepares, recovers completion/reset/stop, reapplies state and rejects stale callbacks; no hardware used");
}}
