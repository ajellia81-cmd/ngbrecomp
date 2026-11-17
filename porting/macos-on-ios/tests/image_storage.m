// Run the real compatibility classes headlessly, without UIKit or game data.
#define NGB_IMAGE_STORAGE_TEST 1
#import "../AppKit_NGBImages.m"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    @autoreleasepool {
        __weak NSBitmapImageRep *weakRep;
        NSCursor *cursor;
        @autoreleasepool {
            NSBitmapImageRep *rep=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                pixelsWide:3 pixelsHigh:2 bitsPerSample:8 samplesPerPixel:4
                hasAlpha:YES isPlanar:NO colorSpaceName:@"NSDeviceRGBColorSpace"
                bytesPerRow:16 bitsPerPixel:32];
            assert(rep && rep.bitmapData && rep.bytesPerRow==16);
            unsigned char rgba[32];memset(rgba,255,sizeof rgba);
            // SDL copies the full pitched surface, including row padding.
            memcpy(rep.bitmapData,rgba,sizeof rgba);
            assert(memcmp(rep.bitmapData,rgba,sizeof rgba)==0);
            CGImageRef cg=rep.CGImage;
            assert(cg && CGImageGetWidth(cg)==3 && CGImageGetHeight(cg)==2);
            NSImage *image=[[NSImage alloc] initWithSize:CGSizeMake(3,2)];
            [image addRepresentation:rep];
            assert(image.isValid);
            cursor=[[NSCursor alloc] initWithImage:image hotSpot:CGPointMake(1,1)];
            weakRep=rep;
        }
        assert(weakRep && cursor.image.representations.count==1);
        NSBitmapImageRep *held=(NSBitmapImageRep *)cursor.image.representations.firstObject;
        assert(held.bitmapData[31]==255);
        assert([cursor.image CGImageForProposedRect:NULL context:nil hints:nil]);
        assert(NSCursor.arrowCursor==NSCursor.currentCursor);
        unsigned char external[16]={0};unsigned char *planes[]={external};
        NSBitmapImageRep *borrowed=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:planes
            pixelsWide:2 pixelsHigh:2 bitsPerSample:8 samplesPerPixel:4
            hasAlpha:YES isPlanar:NO colorSpaceName:nil bytesPerRow:8 bitsPerPixel:32];
        assert(borrowed.bitmapData==external);
        puts("SDL cursor image storage: copy, pitch, CGImage and ownership checks passed");
    }
    return 0;
}
