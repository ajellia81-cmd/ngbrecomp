/* UIKit services around the real SDL/main.c engine, not a separate harness. */
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreText/CoreText.h>
#import <GameController/GameController.h>
#import <AVFoundation/AVFoundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <SDL.h>
#include <SDL_syswm.h>
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <dlfcn.h>
#include <math.h>
#include <unistd.h>
#include "../src/install.h"
#include "platform/vcpu.h"
void ngb_ios_log_build(void) {
    fprintf(stderr,"[iOS Build] version %s; source %s\n",NGB_IOS_BUILD_VERSION,NGB_IOS_BUILD_COMMIT);
    NSString *documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    if (documents) {
        const char *saves=getenv("NGB_SAVE_DIR");
        NSString *info=[NSString stringWithFormat:@"Version: %s\nSource: %s\nCurrent log: %s/log.txt\n",
            NGB_IOS_BUILD_VERSION,NGB_IOS_BUILD_COMMIT,saves ? saves : "unknown"];
        [info writeToFile:[documents stringByAppendingPathComponent:@"build-info.txt"]
            atomically:YES encoding:NSUTF8StringEncoding error:nil];
#ifdef NGB_IOS_KOREAN
        extern int ngb_korean_active(void);
        if (ngb_korean_active()) {
            NSDictionary *state=[NSDictionary dictionaryWithContentsOfFile:[documents stringByAppendingPathComponent:@"korean-game.state.plist"]];
            fprintf(stderr,"[Korean] active overlay layout: %s\n",[state[@"layout"] UTF8String] ?: "legacy");
        }
#endif
    }
}
// A preparation window must not return the LiveContainer scene to portrait
// just before SDL creates its Metal surface.
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
            errorHandler:^(NSError *error) { fprintf(stderr,"[iOS Orientation] landscape request: %s\n",error.localizedDescription.UTF8String); }];
        else [UIViewController attemptRotationToDeviceOrientation];
    } else [UIViewController attemptRotationToDeviceOrientation];
}
static int safeMillion[4]={80000,30000,80000,20000};
void ngb_ios_safe_area(float *left,float *top,float *right,float *bottom) {
    float *out[4]={left,top,right,bottom};
    for (int i=0;i<4;i++) *out[i]=__atomic_load_n(&safeMillion[i],__ATOMIC_RELAXED)/1000000.0f;
}
void ngb_ios_quit(void) {
    void (^close)(void)=^{
        extern void apu_host_pause(int);
        apu_host_pause(1); fflush(NULL); _exit(0);
    };
    if (NSThread.isMainThread) close(); else dispatch_async(dispatch_get_main_queue(),close);
}
// Match the translation's own lettering; the private capsule carries only
// scalable lettering rasterized into the existing tab-art mask.
int ngb_ios_settings_tab_mask(uint8_t *mask, int width, int height) {
    @autoreleasepool {
        // Match the dark, slanted Korean tab lettering without enlarging
        // the Options sprite's tiny, differently styled white glyphs.
        if (!mask || width<240 || height<128) return 0;
        memset(mask,0,(size_t)width*height);
        CGColorSpaceRef space=CGColorSpaceCreateDeviceGray();
        CGContextRef context=CGBitmapContextCreate(mask,width,height,8,width,space,kCGImageAlphaNone);
        CGColorSpaceRelease(space);
        if (!context) return 0;
        CTFontRef font=CTFontCreateWithName(CFSTR("AppleSDGothicNeo-Regular"),80,NULL);
        NSDictionary *attributes=@{(__bridge NSString *)kCTFontAttributeName:(__bridge id)font,
            (__bridge NSString *)kCTForegroundColorFromContextAttributeName:@YES};
        NSAttributedString *text=[[NSAttributedString alloc] initWithString:@"설정" attributes:attributes];
        CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text);
        CGContextSetShouldAntialias(context,true);
        CGContextSetGrayFillColor(context,1,1);
        CGContextSetTextMatrix(context,CGAffineTransformMake(1,0,.18,1,0,0));
        CGContextSetTextPosition(context,12,30);
        CTLineDraw(line,context);
        CFRelease(line);CFRelease(font);CGContextRelease(context);
        // The consumer already uses this bitmap's row order. A second row
        // reversal here flips only the Korean word on the finished plate.
        return 1;
    }
}
static int displayMilliHz=60000;
static unsigned motionEpoch;
double ngb_ios_display_hz(void) { return __atomic_load_n(&displayMilliHz,__ATOMIC_RELAXED)/1000.0; }
unsigned ngb_ios_motion_epoch(void) { return __atomic_load_n(&motionEpoch,__ATOMIC_RELAXED); }
@interface NGBDisplayCadence : NSObject
@property(nonatomic,strong) CADisplayLink *link;
- (void)tick:(CADisplayLink *)link;
@end
@implementation NGBDisplayCadence
- (void)tick:(CADisplayLink *)link {
    CFTimeInterval duration=link.targetTimestamp-link.timestamp;
    if (duration>0 && duration<1) __atomic_store_n(&displayMilliHz,(int)lrint(1000.0/duration),__ATOMIC_RELAXED);
}
@end
static NGBDisplayCadence *displayCadence;
static void request_display_cadence(void) {
    if (displayCadence) return;
    displayCadence=[NGBDisplayCadence new];
    displayCadence.link=[CADisplayLink displayLinkWithTarget:displayCadence selector:@selector(tick:)];
    float maximum=(float)UIScreen.mainScreen.maximumFramesPerSecond;
    __atomic_store_n(&displayMilliHz,(int)(maximum*1000),__ATOMIC_RELAXED);
    displayCadence.link.preferredFrameRateRange=CAFrameRateRangeMake(maximum>=100 ? 80 : maximum,maximum,maximum);
    [displayCadence.link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

#ifdef NGB_IOS_KOREAN
#include "../src/launcher.h"
#include "language_policy.h"
extern BOOL ngb_prepare_korean(NSString *, NSString *, NSString *, void (^)(NSString *), NSError **);
static int koreanActive;
static int textOverride=-1;
int ngb_ios_text_override(void) { return textOverride; }
int ngb_korean_active(void) { return koreanActive; }

void ngb_ios_language_changed(int language)
{
    fprintf(stderr,"[iOS] Options text language saved; relaunch for overlay changes\n");
    (void)language;
    dispatch_async(dispatch_get_main_queue(),^{
        UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Restart required"
            message:@"Quit and reopen to apply the text language." preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Later" style:UIAlertActionStyleCancel handler:nil]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Quit" style:UIAlertActionStyleDefault
            handler:^(UIAlertAction *action) { (void)action; ngb_ios_quit(); }]];
        UIWindow *window=UIApplication.sharedApplication.keyWindow;
        UIViewController *controller=window.rootViewController;
        while (controller.presentedViewController) controller=controller.presentedViewController;
        [controller presentViewController:alert animated:YES completion:nil];
    });
}

static NSString *prepare_language(NSString *documents, NSString *original)
{
    NgbSettings settings; ngb_settings_load(&settings);
    textOverride=settings.text_language_options ? (int)ngb_language_game_index(settings.text_language) : -1;
    setenv("NGB_TEXT_LANGUAGE",settings.text_language==1 ? "English" : "Japanese",1);
    if (settings.text_language!=2) return original;
    NSString *destination=[documents stringByAppendingPathComponent:@"korean-game"];
    NSString *assets=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"KoreanPatch"];
    UIWindow *window=ngb_landscape_window();
    window.rootViewController=[NGBLandscapeController new];
    window.rootViewController.view.backgroundColor=UIColor.systemBackgroundColor;
    UILabel *status=[UILabel new]; status.numberOfLines=0; status.textAlignment=NSTextAlignmentCenter;
    status.text=@"Preparing Korean game data…\nThe original game files will be preserved.";
    status.translatesAutoresizingMaskIntoConstraints=NO;
    [window.rootViewController.view addSubview:status];
    [NSLayoutConstraint activateConstraints:@[
        [status.centerXAnchor constraintEqualToAnchor:window.rootViewController.view.centerXAnchor],
        [status.centerYAnchor constraintEqualToAnchor:window.rootViewController.view.centerYAnchor],
        [status.widthAnchor constraintLessThanOrEqualToConstant:520]]];
    [window makeKeyAndVisible];
    ngb_request_landscape(window);
    __block BOOL done=NO, ok=NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        NSError *error=nil;
        BOOL result=ngb_prepare_korean(original,destination,assets,^(NSString *message) {
            dispatch_async(dispatch_get_main_queue(), ^{ status.text=message; });
        },&error);
        dispatch_async(dispatch_get_main_queue(), ^{
            ok=result;
            if (ok) { done=YES; return; }
            fprintf(stderr,"[Korean] preparation failed: %s\n",error.localizedDescription.UTF8String);
            UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Korean Data Not Prepared"
                message:error.localizedDescription ?: @"The original game was preserved."
                preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"Continue in English" style:UIAlertActionStyleDefault
                handler:^(UIAlertAction *action) { (void)action; done=YES; }]];
            [window.rootViewController presentViewController:alert animated:YES completion:nil];
        });
    });
    while (!done) @autoreleasepool {
        [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    window.hidden=YES;
    if (!ok) {
        settings.text_language=1; settings.text_language_options=1; textOverride=1; ngb_settings_save(&settings);
        setenv("NGB_TEXT_LANGUAGE","English",1);
        return original;
    }
    // The title installs AFS files on its emulated HDD. Keep that cache per
    // language while sharing UserData/TitleData so progress is retained.
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *saves=[documents stringByAppendingPathComponent:@"saves"];
    NSString *localizedSaves=[documents stringByAppendingPathComponent:@"saves-korean"];
    [fm createDirectoryAtPath:localizedSaves withIntermediateDirectories:YES attributes:nil error:nil];
    for (NSString *name in @[@"UserData",@"TitleData"]) {
        NSString *source=[saves stringByAppendingPathComponent:name];
        NSString *target=[localizedSaves stringByAppendingPathComponent:name];
        [fm createDirectoryAtPath:source withIntermediateDirectories:YES attributes:nil error:nil];
        if (![fm fileExistsAtPath:target])
            [fm createSymbolicLinkAtPath:target withDestinationPath:source error:nil];
    }
    setenv("NGB_SAVE_DIR",localizedSaves.fileSystemRepresentation,1);
    koreanActive=1; textOverride=0;
    fprintf(stderr,"[Korean] verified text/font code and private archive overlay selected\n");
    return destination;
}
#endif

extern void apu_host_pause(int paused);
extern volatile int apu_host_paused;
extern void nv2a_vk_set_background(bool background);
static BOOL gameActive = YES;
static int previousAudioPause;
static int screenWidth = 1, screenHeight = 1;

void ngb_ios_screen_size(int *width, int *height)
{
    *width = screenWidth;
    *height = screenHeight;
}


/* Only the UI thread runs this. Guest and renderer threads can park without
 * preventing UIKit from delivering the did-become-active notification. */
void ngb_ios_wait_foreground(void)
{
    while (!gameActive) @autoreleasepool {
        [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
}

static void game_set_active(BOOL active)
{
    if (gameActive == active) return;
    gameActive = active;
    if (!active) {
        extern void nv2a_present_cancel_pairs(void);
        nv2a_present_cancel_pairs();
        previousAudioPause = apu_host_paused;
        apu_host_pause(1);
        int stopped = vcpu_set_paused(1);
        vcpu_game_clock_pause(1);
        nv2a_vk_set_background(true);
        fprintf(stderr, "[iOS] inactive: audio paused, guest %s, clock frozen, GPU drained\n",
                stopped ? "parked" : "pause pending");
    } else {
        nv2a_vk_set_background(false);
        vcpu_game_clock_pause(0);
        [AVAudioSession.sharedInstance setActive:YES error:nil];
        apu_host_pause(previousAudioPause);
        vcpu_set_paused(0);
        fprintf(stderr, "[iOS] active: GPU, clock, audio and guest resumed\n");
    }
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
    if (wasHidden!=physical) fprintf(stderr,"[iOS Touch] %s; connected game controllers %lu\n",
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
    if (![NSStringFromClass(window.class) hasPrefix:@"SDL"]) return;
    if (!window.rootViewController || touchControls.superview == window.rootViewController.view) return;
    [touchControls disconnectJoystick];
    [touchControls removeFromSuperview];
    touchControls = [[NGBTouch alloc] initWithFrame:window.rootViewController.view.bounds];
    [window.rootViewController.view addSubview:touchControls];
    [touchControls refresh];
    fprintf(stderr,"[iOS Touch] overlay attached %.0fx%.0f; %s; virtual input %s\n",
        touchControls.bounds.size.width,touchControls.bounds.size.height,
        touchControls.hidden ? "hidden" : "visible",touchControls.joystick ? "ready" : "off");
}

static void ngb_sync_metal_layers(UIView *view) {
    if ([view.layer isKindOfClass:CAMetalLayer.class]) {
        CAMetalLayer *layer=(CAMetalLayer *)view.layer;
        CGSize size=CGSizeMake(floor(view.bounds.size.width*layer.contentsScale),floor(view.bounds.size.height*layer.contentsScale));
        if (size.width>0 && size.height>0 && !CGSizeEqualToSize(size,layer.drawableSize)) {
            fprintf(stderr,"[iOS Drawable] repair layer %.0fx%.0f -> %.0fx%.0f; view %.0fx%.0f\n",
                layer.drawableSize.width,layer.drawableSize.height,size.width,size.height,view.bounds.size.width,view.bounds.size.height);
            layer.drawableSize=size;
        }
    }
    for (UIView *child in view.subviews) ngb_sync_metal_layers(child);
}
// Called only by SDL's main-thread event pump, never while holding the Vulkan
// presentation/queue mutex. UIKit geometry and CAMetalLayer stay on this thread.
void ngb_ios_sync_drawable(SDL_Window *sdlWindow) {
    @autoreleasepool {
    if (!NSThread.isMainThread || !sdlWindow) return;
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(sdlWindow,&info) || info.subsystem!=SDL_SYSWM_UIKIT) return;
    UIWindow *window=info.info.uikit.window;
    UIView *root=window.rootViewController.view;
    if (!window || !root) return;
    static CFTimeInterval next_request;
    CFTimeInterval now=CACurrentMediaTime();
    if (root.bounds.size.width<root.bounds.size.height && now>=next_request) {
        next_request=now+2;
        fprintf(stderr,"[iOS Orientation] request landscape; window %.0fx%.0f; root %.0fx%.0f; scene %ld\n",
            window.bounds.size.width,window.bounds.size.height,root.bounds.size.width,root.bounds.size.height,
            (long)window.windowScene.interfaceOrientation);
        ngb_request_landscape(window);
    }
    [window layoutIfNeeded];
    [root layoutIfNeeded];
    ngb_sync_metal_layers(root);
    attach_controls(window);
    static CFTimeInterval next_controls;
    if (now>=next_controls) {
        next_controls=now+0.25;
        [touchControls refresh];
    }
    }
}

int ngb_ios_prepare(void)
{
    @autoreleasepool {
        // Keep Vulkan queue calls short: synchronous Metal encoding under the
        // shared queue lock stalls the game producer behind presentation.
        setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS","0",0);
        fprintf(stderr,"[iOS GPU] synchronous queue submissions: %s\n",getenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS"));
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        request_display_cadence();
        CGSize pixels = UIScreen.mainScreen.nativeBounds.size;
        screenWidth = (int)MAX(pixels.width, pixels.height);
        screenHeight = (int)MIN(pixels.width, pixels.height);
        fprintf(stderr, "[iOS] native landscape pixels: %d x %d\n", screenWidth, screenHeight);
        NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        if (!documents) return 0;
        NSFileManager *fm = NSFileManager.defaultManager;
        NSString *game = [documents stringByAppendingPathComponent:@"game"];
        NSArray *names = @[@"saves", @"config", @"shaders"];
        const char *variables[] = {"NGB_SAVE_DIR", "NGB_CONFIG_DIR", "NV2A_SHADER_CACHE"};
        for (NSUInteger i = 0; i < names.count; i++) {
            NSString *path = [documents stringByAppendingPathComponent:names[i]];
            if (![fm createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil]) return 0;
            setenv(variables[i], path.fileSystemRepresentation, 1);
        }
        setenv("NGB_GAME_DIR", game.fileSystemRepresentation, 1);
        setenv("NGB_RENDERER", "d3d", 1);
        if (ngb_check_xbe(game.fileSystemRepresentation) != 0) {
            NGBImport *importer = [NGBImport new]; importer.destination = game;
            if (![importer run]) return 0;
        }
#ifdef NGB_IOS_KOREAN
        game=prepare_language(documents,game);
        setenv("NGB_GAME_DIR",game.fileSystemRepresentation,1);
#endif
        NSString *framework = [NSBundle.mainBundle.privateFrameworksPath stringByAppendingPathComponent:@"MoltenVK.framework/MoltenVK"];
        void *module = dlopen(framework.fileSystemRepresentation, RTLD_NOW | RTLD_LOCAL);
        PFN_vkGetInstanceProcAddr loader = module ? (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr") : NULL;
        if (!loader) { fprintf(stderr, "Cannot load the bundled MoltenVK framework.\n"); return 0; }
        volkInitializeCustom(loader);
        setenv("SDL_VULKAN_LIBRARY", framework.fileSystemRepresentation, 1);
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient error:nil];
        [AVAudioSession.sharedInstance setPreferredSampleRate:48000 error:nil];
        NSNotificationCenter *notifications = NSNotificationCenter.defaultCenter;
        [notifications addObserverForName:UIWindowDidBecomeKeyNotification object:nil queue:NSOperationQueue.mainQueue
                              usingBlock:^(NSNotification *note) { attach_controls(note.object); }];
        for (NSNotificationName name in @[GCControllerDidConnectNotification, GCControllerDidDisconnectNotification])
            [notifications addObserverForName:name object:nil queue:NSOperationQueue.mainQueue
                                  usingBlock:^(NSNotification *note) { (void)note; [touchControls refresh]; }];
        [notifications addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:NSOperationQueue.mainQueue
                              usingBlock:^(NSNotification *note) {
            (void)note; __atomic_add_fetch(&motionEpoch,1,__ATOMIC_RELAXED); [touchControls releaseInputs]; game_set_active(NO);
        }];
        [notifications addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue
                              usingBlock:^(NSNotification *note) { (void)note; game_set_active(YES); }];
        /* Import/patch work may have finished after the owner went Home.
         * Do not launch the renderer before foreground permission returns. */
        if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive)
            game_set_active(NO);
        ngb_ios_wait_foreground();
        return 1;
    }
}
