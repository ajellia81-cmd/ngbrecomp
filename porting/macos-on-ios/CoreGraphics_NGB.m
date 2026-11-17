// CoreGraphics display APIs (macOS-only): one display, the iPad screen, one mode.
#import <UIKit/UIKit.h>
#import <Metal/Metal.h>

typedef uint32_t CGDirectDisplayID;
// SDL retains display modes through kCFTypeArrayCallBacks. A raw C pointer
// cannot be returned here: CFRetain would read it as a Core Foundation object.
typedef CFDictionaryRef CGDisplayModeRef;
static CGDisplayModeRef DisplayMode(void) {
    static CFDictionaryRef mode;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        mode = CFDictionaryCreate(NULL, NULL, NULL, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    });
    return mode;
}
enum { kShimDisplay = 1 };

// Measured once on the main thread at load (game threads must not touch UIKit).
static CGSize gPixels = {2752, 2064};
static double gHz = 120, gScale = 2;
__attribute__((constructor)) static void MeasureScreen(void) {
    CGSize s = UIScreen.mainScreen.nativeBounds.size;
    if (UIScreen.mainScreen.nativeScale > 0) gScale = UIScreen.mainScreen.nativeScale;
    if (s.width > 0) gPixels = s.width >= s.height ? s : CGSizeMake(s.height, s.width);  // games run landscape
    if (UIScreen.mainScreen.maximumFramesPerSecond > 0) gHz = UIScreen.mainScreen.maximumFramesPerSecond;
}
static CGSize ScreenPixels(void) { return gPixels; }

#define EXPORT __attribute__((visibility("default")))

EXPORT int32_t CGGetActiveDisplayList(uint32_t max, CGDirectDisplayID *displays, uint32_t *count) {
    if (displays && max) displays[0] = kShimDisplay;
    if (count) *count = 1;
    return 0;
}
EXPORT int32_t CGGetOnlineDisplayList(uint32_t max, CGDirectDisplayID *displays, uint32_t *count) {
    return CGGetActiveDisplayList(max, displays, count);
}
EXPORT CGDirectDisplayID CGMainDisplayID(void) { return kShimDisplay; }
EXPORT CGDisplayModeRef CGDisplayCopyDisplayMode(CGDirectDisplayID d) { return (CGDisplayModeRef)CFRetain(DisplayMode()); }
EXPORT CFArrayRef CGDisplayCopyAllDisplayModes(CGDirectDisplayID d, CFDictionaryRef options) {
    const void *modes[] = { DisplayMode() };
    return CFArrayCreate(NULL, modes, 1, &kCFTypeArrayCallBacks);
}
EXPORT void CGDisplayModeRelease(CGDisplayModeRef m) { if (m) CFRelease(m); }
EXPORT CGDisplayModeRef CGDisplayModeRetain(CGDisplayModeRef m) { return m ? (CGDisplayModeRef)CFRetain(m) : NULL; }
EXPORT size_t CGDisplayModeGetWidth(CGDisplayModeRef m) { return ScreenPixels().width / gScale; }   // points
EXPORT size_t CGDisplayModeGetHeight(CGDisplayModeRef m) { return ScreenPixels().height / gScale; }
EXPORT size_t CGDisplayModeGetPixelWidth(CGDisplayModeRef m) { return ScreenPixels().width; }
EXPORT size_t CGDisplayModeGetPixelHeight(CGDisplayModeRef m) { return ScreenPixels().height; }
// SDL's Cocoa driver compares and releases this Copy result unconditionally.
EXPORT CFStringRef CGDisplayModeCopyPixelEncoding(CGDisplayModeRef m) {
    return CFStringCreateCopy(NULL, CFSTR("--------RRRRRRRRGGGGGGGGBBBBBBBB"));
}
EXPORT double CGDisplayModeGetRefreshRate(CGDisplayModeRef m) { return gHz; }
EXPORT CGRect CGDisplayBounds(CGDirectDisplayID d) { CGSize p = ScreenPixels(); return CGRectMake(0, 0, p.width / gScale, p.height / gScale); }
EXPORT size_t CGDisplayPixelsWide(CGDirectDisplayID d) { return ScreenPixels().width; }
EXPORT size_t CGDisplayPixelsHigh(CGDirectDisplayID d) { return ScreenPixels().height; }
EXPORT int32_t CGWarpMouseCursorPosition(CGPoint p) { return 0; }
EXPORT int32_t CGAssociateMouseAndMouseCursorPosition(int connected) { return 0; }
EXPORT int CGDisplayIsBuiltin(CGDirectDisplayID d) { return 1; }
EXPORT int CGDisplayIsInMirrorSet(CGDirectDisplayID d) { return 0; }
EXPORT int CGDisplayModeIsUsableForDesktopGUI(CGDisplayModeRef m) { return 1; }
EXPORT uint32_t CGDisplayModeGetIOFlags(CGDisplayModeRef m) { return 0x3; }  // valid + safe
EXPORT uint32_t CGDisplayVendorNumber(CGDirectDisplayID d) { return 0x610; }  // Apple
EXPORT uint32_t CGDisplayModelNumber(CGDirectDisplayID d) { return 1; }
EXPORT uint32_t CGDisplaySerialNumber(CGDirectDisplayID d) { return 1; }
EXPORT CGSize CGDisplayScreenSize(CGDirectDisplayID d) {  // millimetres; iPad Pro/Air panels are 264 ppi
    CGSize p = ScreenPixels(); return CGSizeMake(p.width / 264.0 * 25.4, p.height / 264.0 * 25.4);
}
EXPORT id<MTLDevice> CGDirectDisplayCopyCurrentMetalDevice(CGDirectDisplayID d) NS_RETURNS_RETAINED {
    return MTLCreateSystemDefaultDevice();
}
