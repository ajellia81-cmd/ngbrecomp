// SDL's Cocoa cursor and window-icon paths use AppKit image storage even on a
// touch-only device. Keep the pixels alive for the lifetime of the image rep.
#if defined(NGB_IMAGE_STORAGE_TEST)
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#else
#import <UIKit/UIKit.h>
#endif
#import <ImageIO/ImageIO.h>
#include <stdint.h>

__attribute__((visibility("default")))
@interface NSImageRep : NSObject
@property CGSize size;
@property NSInteger pixelsWide, pixelsHigh;
@end
@implementation NSImageRep
@end

__attribute__((visibility("default")))
@interface NSBitmapImageRep : NSImageRep
@property(nonatomic, strong) NSMutableData *storage;
@property(nonatomic, strong) id snapshot;
@property unsigned char *pixels;
@property NSInteger bytesPerRow, bitsPerPixel, bitsPerSample, samplesPerPixel;
@property BOOL hasAlpha, planar;
- (instancetype)initWithBitmapDataPlanes:(unsigned char **)planes
    pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height
    bitsPerSample:(NSInteger)bits samplesPerPixel:(NSInteger)samples
    hasAlpha:(BOOL)alpha isPlanar:(BOOL)planar colorSpaceName:(NSString *)space
    bytesPerRow:(NSInteger)stride bitsPerPixel:(NSInteger)pixelBits;
- (unsigned char *)bitmapData;
- (CGImageRef)CGImage;
@end
@implementation NSBitmapImageRep
- (instancetype)initWithBitmapDataPlanes:(unsigned char **)planes
    pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height
    bitsPerSample:(NSInteger)bits samplesPerPixel:(NSInteger)samples
    hasAlpha:(BOOL)alpha isPlanar:(BOOL)planar colorSpaceName:(NSString *)space
    bytesPerRow:(NSInteger)stride bitsPerPixel:(NSInteger)pixelBits {
    (void)space;
    if (!(self=[super init])) return nil;
    // SDL converts to interleaved RGBA8 before making this request.
    if (width<=0 || height<=0 || bits!=8 || samples!=4 || planar) return nil;
    if (pixelBits && pixelBits!=32) return nil;
    if ((uint64_t)width>SIZE_MAX/4) return nil;
    size_t minimum=(size_t)width*4;
    if (!stride) stride=(NSInteger)minimum;
    if (stride<0 || (size_t)stride<minimum || (uint64_t)height>SIZE_MAX/(size_t)stride) return nil;
    self.pixelsWide=width;self.pixelsHigh=height;self.size=CGSizeMake(width,height);
    _bytesPerRow=stride;_bitsPerPixel=32;_bitsPerSample=bits;_samplesPerPixel=samples;
    _hasAlpha=alpha;_planar=NO;
    if (planes && planes[0]) _pixels=planes[0];
    else {
        _storage=[NSMutableData dataWithLength:(size_t)height*(size_t)stride];
        _pixels=_storage.mutableBytes;
    }
    return _pixels ? self : nil;
}
- (unsigned char *)bitmapData { return _pixels; }
- (void)getBitmapDataPlanes:(unsigned char **)planes { if(planes)planes[0]=_pixels; }
- (CGImageRef)CGImage {
    if (!_pixels) return NULL;
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(_pixels,self.pixelsWide,self.pixelsHigh,8,
        _bytesPerRow,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if (!context) return NULL;
    CGImageRef image=CGBitmapContextCreateImage(context);CGContextRelease(context);
    // A getter does not transfer ownership; retain its snapshot with this rep.
    if(!image)return NULL;
    self.snapshot=(__bridge id)image;CGImageRelease(image);
    return (__bridge CGImageRef)self.snapshot;
}
@end

__attribute__((visibility("default")))
@interface NSImage : NSObject
@property CGSize size;
@property(nonatomic, strong) NSMutableArray<NSImageRep *> *representations;
@property(nonatomic, strong) id snapshot;
- (instancetype)initWithSize:(CGSize)size;
- (void)addRepresentation:(NSImageRep *)rep;
@end
@implementation NSImage
- (instancetype)initWithSize:(CGSize)size {
    if((self=[super init])){_size=size;_representations=[NSMutableArray new];}return self;
}
- (instancetype)initWithCGImage:(CGImageRef)image size:(CGSize)size {
    if(!image)return nil;
    if((self=[self initWithSize:size]))_snapshot=(__bridge id)image;
    return self;
}
- (instancetype)initWithData:(NSData *)data {
    CGImageSourceRef source=data ? CGImageSourceCreateWithData((__bridge CFDataRef)data,NULL) : NULL;
    if(!source)return nil;
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,NULL);CFRelease(source);
    if(!image)return nil;
    self=[self initWithCGImage:image size:CGSizeMake(CGImageGetWidth(image),CGImageGetHeight(image))];
    CGImageRelease(image);return self;
}
- (instancetype)initWithContentsOfFile:(NSString *)path { return [self initWithData:[NSData dataWithContentsOfFile:path]]; }
- (void)addRepresentation:(NSImageRep *)rep { if(rep)[_representations addObject:rep]; }
- (BOOL)isValid { return _snapshot!=nil || _representations.count!=0; }
- (CGImageRef)CGImageForProposedRect:(CGRect *)rect context:(id)context hints:(NSDictionary *)hints {
    (void)rect;(void)context;(void)hints;
    if(_snapshot)return (__bridge CGImageRef)_snapshot;
    for(NSImageRep *rep in _representations)if([rep isKindOfClass:NSBitmapImageRep.class])return [(NSBitmapImageRep *)rep CGImage];
    return NULL;
}
@end

// iOS uses touch/controller input, but SDL still creates and retains Cocoa
// cursor objects during video initialization. No desktop cursor is displayed.
__attribute__((visibility("default")))
@interface NSCursor : NSObject
@property(nonatomic, strong) NSImage *image;
@property CGPoint hotSpot;
- (instancetype)initWithImage:(NSImage *)image hotSpot:(CGPoint)hotSpot;
@end
@implementation NSCursor
- (instancetype)initWithImage:(NSImage *)image hotSpot:(CGPoint)hotSpot {
    if((self=[super init])){_image=image;_hotSpot=hotSpot;}return self;
}
+ (instancetype)arrowCursor { static NSCursor *cursor;static dispatch_once_t once;dispatch_once(&once,^{cursor=[self new];});return cursor; }
+ (instancetype)currentCursor { return [self arrowCursor]; }
+ (instancetype)IBeamCursor { return [self arrowCursor]; }
+ (instancetype)crosshairCursor { return [self arrowCursor]; }
+ (instancetype)pointingHandCursor { return [self arrowCursor]; }
+ (instancetype)closedHandCursor { return [self arrowCursor]; }
+ (instancetype)openHandCursor { return [self arrowCursor]; }
+ (instancetype)resizeLeftRightCursor { return [self arrowCursor]; }
+ (instancetype)resizeUpDownCursor { return [self arrowCursor]; }
+ (instancetype)operationNotAllowedCursor { return [self arrowCursor]; }
+ (void)hide {}
+ (void)unhide {}
- (void)set {}
- (void)push {}
+ (void)pop {}
@end
