#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>
#import <AVFoundation/AVFoundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "bridge.h"
#include "korean/haptics.h"
#define NGB_IOS_PORT_DIAG(...) ((void)0)
typedef void SDL_Joystick;typedef int32_t SDL_JoystickID;typedef int16_t Sint16;
enum {SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,
SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_GUIDE,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_LEFTSTICK,
SDL_CONTROLLER_BUTTON_RIGHTSTICK,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
SDL_CONTROLLER_BUTTON_MAX=21};
enum {SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY,
SDL_CONTROLLER_AXIS_TRIGGERLEFT,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,SDL_CONTROLLER_AXIS_MAX};
enum {SDL_INIT_GAMECONTROLLER=0x2000,SDL_JOYSTICK_TYPE_GAMECONTROLLER=1};
static NgbConversionHost host;
#define ngb_check_xbe host.check_xbe
#define ngb_install host.install
#define ngb_install_set_progress host.progress
#define SDL_WasInit host.was_init
#define SDL_InitSubSystem host.init_subsystem
#define SDL_JoystickAttachVirtual host.attach_virtual
#define SDL_JoystickOpen host.open_joystick
#define SDL_JoystickSetVirtualAxis host.axis
#define SDL_JoystickSetVirtualButton host.button
#define SDL_JoystickInstanceID host.instance
#define SDL_JoystickClose host.close_joystick
#define SDL_NumJoysticks host.num_joysticks
#define SDL_JoystickGetDeviceInstanceID host.device_instance
#define SDL_JoystickDetachVirtual host.detach_virtual
static int safeMillion[4]={0};
@interface NGBLandscapeController : UIViewController
@end
@implementation NGBLandscapeController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation { return UIInterfaceOrientationLandscapeRight; }
- (BOOL)shouldAutorotate { return YES; }
@end
static UIWindowScene *ngb_window_scene(UIWindow *window) {
    if (window.windowScene) return window.windowScene;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class] && scene.activationState==UISceneActivationStateForegroundActive)
            return (UIWindowScene *)scene;
    return nil;
}
static UIWindow *ngb_landscape_window(void) {
    UIWindowScene *scene=ngb_window_scene(nil);
    UIWindow *window=scene ? [[UIWindow alloc] initWithWindowScene:scene] : [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    if (scene) window.frame=scene.coordinateSpace.bounds;
    return window;
}
static void ngb_request_landscape(UIWindow *window) {
    UIViewController *controller=window.rootViewController;
    if (@available(iOS 16.0,*)) {
        [controller setNeedsUpdateOfSupportedInterfaceOrientations];
        UIWindowScene *scene=ngb_window_scene(window);
        if (scene) [scene requestGeometryUpdateWithPreferences:
            [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape]
            errorHandler:^(NSError *error) { NGB_IOS_PORT_DIAG("[iOS Orientation] landscape request: %s\n",error.localizedDescription.UTF8String); }];
        else [UIViewController attemptRotationToDeviceOrientation];
    } else [UIViewController attemptRotationToDeviceOrientation];
}
@interface NGBImport : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, strong) UILabel *status;
@property(nonatomic, copy) NSString *destination;
@property(nonatomic, strong) NSArray<UIButton *> *actions;
@property BOOL importing;
@property BOOL done;
@property BOOL ok;
@end

static void import_progress(uint64_t done, uint64_t total, void *context)
{
    NGBImport *importer = (__bridge NGBImport *)context;
    if (total) dispatch_async(dispatch_get_main_queue(), ^{
        importer.status.text = [NSString stringWithFormat:@"Importing game files… %.0f%%", 100.0 * done / total];
    });
}

@implementation NGBImport
- (void)choose:(UIButton *)sender
{
    if (self.importing) return;
    /* Some Files providers classify ISO/XISO as disk images or dynamic types
     * without public.data conformance. Let the importer validate the selection
     * instead of disabling those files in the picker. */
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[sender.tag == 1 ? UTTypeItem : UTTypeFolder] asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self.window.rootViewController presentViewController:picker animated:YES completion:nil];
}
- (void)cancel { if (!self.importing) self.done = YES; }
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
    (void)controller;
    self.status.text = @"Choose your own ISO or the extracted game folder.";
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    (void)controller;
    NSURL *source = urls.firstObject;
    if (!source || self.importing) return;
    self.importing = YES;
    for (UIButton *button in self.actions) button.enabled = NO;
    BOOL scoped = [source startAccessingSecurityScopedResource];
    self.status.text = @"Importing game files…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSNumber *directory = nil;
        NSError *error = nil;
        [source getResourceValue:&directory forKey:NSURLIsDirectoryKey error:&error];
        NSFileManager *fm = NSFileManager.defaultManager;
        BOOL existed = [fm fileExistsAtPath:self.destination];
        BOOL success = NO;
        if (existed) {
            error = [NSError errorWithDomain:@"NGBImport" code:1 userInfo:@{
                NSLocalizedDescriptionKey: @"The existing game folder is invalid. Move it out of Documents/game before importing again."}];
        } else if (directory.boolValue) {
            success = [fm copyItemAtURL:source toURL:[NSURL fileURLWithPath:self.destination] error:&error];
            if (success) success = ngb_check_xbe(self.destination.fileSystemRepresentation) == 0;
        } else if ([@[@"iso", @"xiso"] containsObject:source.pathExtension.lowercaseString]) {
            char resolved[4096];
            ngb_install_set_progress(import_progress, (__bridge void *)self);
            success = ngb_install(source.fileSystemRepresentation, self.destination.fileSystemRepresentation,
                                  resolved, sizeof resolved) == 0;
            ngb_install_set_progress(NULL, NULL);
        }
        if (!success && !existed) [fm removeItemAtPath:self.destination error:nil];
        if (scoped) [source stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (success) {
                self.ok = YES;
                self.done = YES;
            } else {
                self.importing = NO;
                for (UIButton *button in self.actions) button.enabled = YES;
                self.status.text = error.localizedDescription ?: @"Import failed. Choose the supported USA disc or a complete extracted game folder.";
            }
        });
    });
}
- (BOOL)run
{
    self.window = ngb_landscape_window();
    UIViewController *controller = [NGBLandscapeController new];
    controller.view.backgroundColor = UIColor.systemBackgroundColor;
    self.window.rootViewController = controller;
    self.status = [UILabel new];
    self.status.numberOfLines = 0;
    self.status.textAlignment = NSTextAlignmentCenter;
    self.status.text = @"Import your own Ninja Gaiden Black USA game data.\nThe game is not included.";
    UIButton *iso = [UIButton buttonWithType:UIButtonTypeSystem];
    [iso setTitle:@"Import ISO" forState:UIControlStateNormal];
    iso.tag = 1;
    [iso addTarget:self action:@selector(choose:) forControlEvents:UIControlEventTouchUpInside];
    UIButton *folder = [UIButton buttonWithType:UIButtonTypeSystem];
    [folder setTitle:@"Import extracted game folder" forState:UIControlStateNormal];
    [folder addTarget:self action:@selector(choose:) forControlEvents:UIControlEventTouchUpInside];
    UIButton *cancel = [UIButton buttonWithType:UIButtonTypeSystem];
    [cancel setTitle:@"Cancel" forState:UIControlStateNormal];
    [cancel addTarget:self action:@selector(cancel) forControlEvents:UIControlEventTouchUpInside];
    self.actions = @[iso, folder, cancel];
    UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[self.status, iso, folder, cancel]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 20;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [controller.view addSubview:stack];
    UILayoutGuide *safe = controller.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [stack.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [stack.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
        [stack.widthAnchor constraintLessThanOrEqualToConstant:480],
        [stack.leadingAnchor constraintGreaterThanOrEqualToAnchor:safe.leadingAnchor constant:24],
        [stack.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-24]]];
    [self.window makeKeyAndVisible];
    ngb_request_landscape(self.window);
    while (!self.done) @autoreleasepool {
        [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    self.window.hidden = YES;
    self.window = nil;
    return self.ok;
}
@end

@interface NGBStick : UIView
@property SDL_Joystick *joystick;
@property int axis;
@property CGPoint position;
@property(nonatomic, strong) UITouch *tracking;
@end
@implementation NGBStick
- (void)drawRect:(CGRect)rect
{
    (void)rect;
    [[UIColor colorWithWhite:0 alpha:0.25] setFill];
    [[UIBezierPath bezierPathWithOvalInRect:self.bounds] fill];
    CGFloat radius = self.bounds.size.width * 0.22;
    CGPoint center = CGPointMake(self.bounds.size.width / 2, self.bounds.size.height / 2);
    [[UIColor colorWithWhite:1 alpha:0.5] setFill];
    [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(center.x + self.position.x * radius * 1.5 - radius,
        center.y + self.position.y * radius * 1.5 - radius, radius * 2, radius * 2)] fill];
}
- (void)updateTouch:(UITouch *)touch
{
    CGPoint p = [touch locationInView:self];
    CGFloat radius = self.bounds.size.width / 2;
    CGFloat x = (p.x - radius) / radius, y = (p.y - radius) / radius;
    CGFloat length = hypot(x, y);
    if (length > 1) { x /= length; y /= length; }
    self.position = CGPointMake(x, y);
    if (self.joystick) {
        SDL_JoystickSetVirtualAxis(self.joystick, self.axis, (Sint16)lrint(x * 32767));
        SDL_JoystickSetVirtualAxis(self.joystick, self.axis + 1, (Sint16)lrint(y * 32767));
    }
    [self setNeedsDisplay];
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{ (void)event; if (!self.tracking) { self.tracking = touches.anyObject; [self updateTouch:self.tracking]; } }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{ (void)event; if ([touches containsObject:self.tracking]) [self updateTouch:self.tracking]; }
- (void)reset
{
    self.tracking = nil;
    self.position = CGPointZero;
    if (self.joystick) {
        SDL_JoystickSetVirtualAxis(self.joystick, self.axis, 0);
        SDL_JoystickSetVirtualAxis(self.joystick, self.axis + 1, 0);
    }
    [self setNeedsDisplay];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{ (void)event; if ([touches containsObject:self.tracking]) [self reset]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{ (void)touches; (void)event; [self reset]; }
@end

@interface NGBTouch : UIView
@property SDL_Joystick *joystick;
@property(nonatomic, strong) NSMutableArray<UIButton *> *buttons;
@property(nonatomic, strong) NGBStick *left;
@property(nonatomic, strong) NGBStick *right;
- (void)refresh;
- (void)releaseInputs;
@end
@implementation NGBTouch
- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.multipleTouchEnabled = YES;
    self.opaque = NO;
    self.backgroundColor = UIColor.clearColor;
    self.buttons = [NSMutableArray new];
    NSArray *titles = @[@"A", @"B", @"X", @"Y", @"White", @"Black", @"LT", @"RT", @"Back", @"Start",
        @"↑", @"↓", @"←", @"→", @"L3", @"R3"];
    const int tags[] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        100 + SDL_CONTROLLER_AXIS_TRIGGERLEFT, 100 + SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
        SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK };
    for (NSUInteger i = 0; i < titles.count; i++) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.tag = tags[i];
        [button setTitle:titles[i] forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont boldSystemFontOfSize:14];
        button.backgroundColor = [UIColor colorWithWhite:0 alpha:0.4];
        button.layer.cornerRadius = 24;
        [button addTarget:self action:@selector(down:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(up:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];
        [self addSubview:button];
        [self.buttons addObject:button];
    }
    self.left = [NGBStick new]; self.left.axis = SDL_CONTROLLER_AXIS_LEFTX;
    self.right = [NGBStick new]; self.right.axis = SDL_CONTROLLER_AXIS_RIGHTX;
    self.left.opaque = self.right.opaque = NO;
    [self addSubview:self.left]; [self addSubview:self.right];
    return self;
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.hidden) return NO;
    for (UIView *view in self.subviews)
        if ([view pointInside:[view convertPoint:point fromView:self] withEvent:event]) return YES;
    return NO;
}
- (void)layoutSubviews
{
    [super layoutSubviews];
    UIEdgeInsets insets = self.safeAreaInsets;
    CGFloat fractions[4]={insets.left/MAX(1,self.bounds.size.width),insets.top/MAX(1,self.bounds.size.height),
        insets.right/MAX(1,self.bounds.size.width),insets.bottom/MAX(1,self.bounds.size.height)};
    for (int i=0;i<4;i++) __atomic_store_n(&safeMillion[i],(int)lrint(fractions[i]*1000000),__ATOMIC_RELAXED);
    CGFloat width = self.bounds.size.width, bottom = self.bounds.size.height - insets.bottom - 20;
    self.left.frame = CGRectMake(insets.left + 20, bottom - 150, 150, 150);
    self.right.frame = CGRectMake(width - insets.right - 330, bottom - 120, 120, 120);
    CGFloat cx = width - insets.right - 90, cy = bottom - 90;
    CGPoint centers[] = {{cx,cy+48},{cx+48,cy},{cx-48,cy},{cx,cy-48},
        {insets.left+48,insets.top+30},{width-insets.right-48,insets.top+30},
        {insets.left+116,insets.top+30},{width-insets.right-116,insets.top+30},
        {width/2-44,insets.top+30},{width/2+44,insets.top+30},
        {insets.left+250,bottom-112},{insets.left+250,bottom-28},
        {insets.left+208,bottom-70},{insets.left+292,bottom-70},
        {insets.left+95,bottom-180},{width-insets.right-270,bottom-150}};
    for (NSUInteger i = 0; i < self.buttons.count; i++)
        self.buttons[i].frame = CGRectMake(centers[i].x - 25, centers[i].y - 24, 50, 48);
}
- (void)setButton:(UIButton *)button pressed:(BOOL)pressed
{
    if (!self.joystick) return;
    if (button.tag >= 100) SDL_JoystickSetVirtualAxis(self.joystick, (int)button.tag - 100, pressed ? 32767 : -32768);
    else SDL_JoystickSetVirtualButton(self.joystick, (int)button.tag, pressed ? 1 : 0);
}
- (void)down:(UIButton *)button { [self setButton:button pressed:YES]; }
- (void)up:(UIButton *)button { [self setButton:button pressed:NO]; }
- (void)releaseInputs
{
    for (UIButton *button in self.buttons) [self setButton:button pressed:NO];
    [self.left reset]; [self.right reset];
}
- (void)disconnectJoystick
{
    if (!self.joystick) return;
    [self releaseInputs];
    SDL_JoystickID instance = SDL_JoystickInstanceID(self.joystick);
    self.left.joystick = self.right.joystick = NULL;
    SDL_JoystickClose(self.joystick); self.joystick = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == instance) { SDL_JoystickDetachVirtual(i); break; }
}
- (void)refresh
{
    BOOL physical = GCController.controllers.count > 0;
    BOOL wasHidden=self.hidden;
    self.hidden = physical;
    if (wasHidden!=physical) NGB_IOS_PORT_DIAG("[iOS Touch] %s; connected game controllers %lu\n",
        physical ? "hidden" : "visible",(unsigned long)GCController.controllers.count);
    if (physical && self.joystick) {
        [self disconnectJoystick];
    } else if (!physical && !self.joystick) {
        if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER)&SDL_INIT_GAMECONTROLLER) &&
            SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
        int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        if (device >= 0) self.joystick = SDL_JoystickOpen(device);
        self.left.joystick = self.right.joystick = self.joystick;
        [self releaseInputs];
    }
}
@end

static NGBTouch *touchControls;
static void attach_controls(UIWindow *window)
{
    if (!window || [window.rootViewController isKindOfClass:NGBLandscapeController.class]) return;
    if (!window.rootViewController || touchControls.superview == window.rootViewController.view) return;
    [touchControls disconnectJoystick];
    [touchControls removeFromSuperview];
    touchControls = [[NGBTouch alloc] initWithFrame:window.rootViewController.view.bounds];
    [window.rootViewController.view addSubview:touchControls];
    [touchControls refresh];
    NGB_IOS_PORT_DIAG("[iOS Touch] overlay attached %.0fx%.0f; %s; virtual input %s\n",
        touchControls.bounds.size.width,touchControls.bounds.size.height,
        touchControls.hidden ? "hidden" : "visible",touchControls.joystick ? "ready" : "off");
}


static int displayMilliHz=60000,motionEnabled;
static unsigned motionEpoch;
double ngb_macos_on_ios_display_hz(void){return __atomic_load_n(&displayMilliHz,__ATOMIC_RELAXED)/1000.0;}
unsigned ngb_macos_on_ios_motion_epoch(void){return __atomic_load_n(&motionEpoch,__ATOMIC_RELAXED);}
// Cadence is display-only: the game and audio still run at their original rate.
@interface NgbFrameCadence : NSObject
@property(nonatomic,strong) CADisplayLink *link;
@property CFTimeInterval lastReport;
@end
@implementation NgbFrameCadence
- (void)tick:(CADisplayLink *)link {
    CFTimeInterval duration=link.targetTimestamp-link.timestamp;
    if(duration>0 && duration<1)__atomic_store_n(&displayMilliHz,(int)lrint(1000.0/duration),__ATOMIC_RELAXED);
    if(link.timestamp-self.lastReport>=5 && duration>0){
        fprintf(stderr,"[Display] UIKit cadence %.1f Hz (Smooth Motion %s)\n",1.0/duration,motionEnabled ? "on" : "off");
        self.lastReport=link.timestamp;
    }
}
@end
static NgbFrameCadence *frameCadence;
static void request_frame_cadence(void){
    if(frameCadence)return;
    frameCadence=[NgbFrameCadence new];
    frameCadence.link=[CADisplayLink displayLinkWithTarget:frameCadence selector:@selector(tick:)];
    float rate=MIN(60.0f,MAX(1.0f,(float)UIScreen.mainScreen.maximumFramesPerSecond));
    frameCadence.link.preferredFrameRateRange=CAFrameRateRangeMake(rate,rate,rate);
    [frameCadence.link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    fprintf(stderr,"[Display] requested %.0f Hz; screen maximum %ld; low power %d; thermal %ld\n",rate,(long)UIScreen.mainScreen.maximumFramesPerSecond,NSProcessInfo.processInfo.lowPowerModeEnabled,(long)NSProcessInfo.processInfo.thermalState);
}

void ngb_macos_on_ios_motion_enabled(int enabled){
    enabled=!!enabled;if(enabled==motionEnabled)return;motionEnabled=enabled;
    if(host.motion_cancel)host.motion_cancel();
    __atomic_add_fetch(&motionEpoch,1,__ATOMIC_RELAXED);
    float maximum=(float)UIScreen.mainScreen.maximumFramesPerSecond;
    float rate=enabled && maximum>=100 ? maximum : MIN(60.0f,maximum);
    frameCadence.link.preferredFrameRateRange=CAFrameRateRangeMake(rate,rate,rate);
    fprintf(stderr,"[Display] requested %.0f Hz; Smooth Motion %s\n",rate,enabled ? "on" : "off");
}
static int active=1,previousAudioPause;
int ngb_macos_on_ios_is_foreground(void){return __atomic_load_n(&active,__ATOMIC_ACQUIRE);}
void ngb_macos_on_ios_screen_size(int *w,int *h){CGSize p=UIScreen.mainScreen.nativeBounds.size;*w=(int)MAX(p.width,p.height);*h=(int)MIN(p.width,p.height);}
void ngb_macos_on_ios_tick(void){for(UIWindow *window in UIApplication.sharedApplication.windows)if(window.isKeyWindow)attach_controls(window);[touchControls refresh];}
void ngb_macos_on_ios_wait_foreground(void){while(!__atomic_load_n(&active,__ATOMIC_ACQUIRE))@autoreleasepool{if(NSThread.isMainThread)[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];else usleep(50000);}}
int ngb_macos_on_ios_prepare(const NgbConversionHost *api){@autoreleasepool{
    if(!api || api->version!=2 || api->size!=sizeof(host))return 0;host=*api;
    NSString *documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    if(!documents)return 0;NSFileManager *fm=NSFileManager.defaultManager;
    NSArray *names=@[@"saves",@"config",@"shaders-macos-on-ios"];
    const char *variables[]={"NGB_SAVE_DIR","NGB_CONFIG_DIR","NV2A_SHADER_CACHE"};
    for(NSUInteger i=0;i<names.count;i++){NSString *p=[documents stringByAppendingPathComponent:names[i]];if(![fm createDirectoryAtPath:p withIntermediateDirectories:YES attributes:nil error:nil])return 0;setenv(variables[i],p.fileSystemRepresentation,1);}
    NSString *game=[documents stringByAppendingPathComponent:@"game"];setenv("NGB_GAME_DIR",game.fileSystemRepresentation,1);
    if(ngb_check_xbe(game.fileSystemRepresentation)!=0){NGBImport *importer=[NGBImport new];importer.destination=game;if(![importer run])return 0;}
    request_frame_cadence();
    fprintf(stderr,"[Startup] game data validated; configuring audio session\n");
    [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient error:nil];
    [AVAudioSession.sharedInstance setPreferredSampleRate:48000 error:nil];
    NSError *audioError=nil;
    [AVAudioSession.sharedInstance setActive:YES error:&audioError];
    fprintf(stderr,"[Audio] iOS session active: %s\n",audioError ? audioError.localizedDescription.UTF8String : "yes");
    // Use the original Mac hang sampler, twice only, to locate a stalled boot.
    for(unsigned seconds=25;seconds<=45;seconds+=20)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)seconds*NSEC_PER_SEC),dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        struct sigaction action={0};
        if(__atomic_load_n(&active,__ATOMIC_ACQUIRE) && sigaction(SIGUSR1,NULL,&action)==0 && action.sa_handler!=SIG_DFL && action.sa_handler!=SIG_IGN){
            fprintf(stderr,"[Startup] sampling original Mac boot thread\n");kill(getpid(),SIGUSR1);
        }
    });
    NSNotificationCenter *nc=NSNotificationCenter.defaultCenter;
    [nc addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *n){(void)n;__atomic_store_n(&active,0,__ATOMIC_RELEASE);if(host.motion_cancel)host.motion_cancel();__atomic_add_fetch(&motionEpoch,1,__ATOMIC_RELAXED);frameCadence.link.paused=YES;[touchControls releaseInputs];ngb_ios_haptics_active(0);previousAudioPause=host.audio_paused();host.audio_pause(1);}];
    [nc addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *n){(void)n;__atomic_store_n(&active,1,__ATOMIC_RELEASE);frameCadence.link.paused=NO;ngb_ios_haptics_active(1);host.audio_pause(previousAudioPause);}];
    for(NSNotificationName name in @[GCControllerDidConnectNotification,GCControllerDidDisconnectNotification])
      [nc addObserverForName:name object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *n){(void)n;[touchControls refresh];for(unsigned i=0;i<4;i++)ngb_ios_haptics_prepare(i);}];
    fprintf(stderr,"[Startup] preparing controller haptics\n");
    for(unsigned i=0;i<4;i++)ngb_ios_haptics_prepare(i);
    fprintf(stderr,"[Startup] UIKit preparation complete\n");
    return 1;
}}

void ngb_macos_on_ios_haptics_set(unsigned port,uint16_t low,uint16_t high){ngb_ios_haptics_set(port,low,high);}

// A Mac SDL message box cannot present a UIKit alert through AppKit stubs.
void ngb_macos_on_ios_language_changed(int language){
    dispatch_async(dispatch_get_main_queue(),^{
        static BOOL showing;
        if(showing)return;
        UIWindow *window=nil;
        for(UIWindow *candidate in UIApplication.sharedApplication.windows)if(candidate.isKeyWindow){window=candidate;break;}
        UIViewController *controller=window.rootViewController;
        while(controller.presentedViewController)controller=controller.presentedViewController;
        if(!controller){fprintf(stderr,"[Language] restart notice has no active window\n");return;}
        BOOL korean=language==2;
        UIAlertController *alert=[UIAlertController alertControllerWithTitle:korean ? @"재시작 필요" : @"Restart required"
            message:korean ? @"표시 언어를 적용하려면 앱을 완전히 종료한 뒤 다시 실행해 주세요." : @"Fully close and reopen the app to apply the display language."
            preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:korean ? @"확인" : @"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){(void)action;showing=NO;}]];
        showing=YES;[controller presentViewController:alert animated:YES completion:nil];
        fprintf(stderr,"[Language] native restart notice presented\n");
    });
}
