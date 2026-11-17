#ifndef NGB_KOREAN_GAME_LINKS_H
#define NGB_KOREAN_GAME_LINKS_H
#import <Foundation/Foundation.h>

static NSString *ngb_relative_asset_path(NSString *source, NSString *directory) {
    NSArray<NSString *> *from=directory.stringByStandardizingPath.pathComponents;
    NSArray<NSString *> *to=source.stringByStandardizingPath.pathComponents;
    NSUInteger shared=0;
    while (shared<from.count && shared<to.count && [from[shared] isEqualToString:to[shared]]) shared++;
    NSMutableArray<NSString *> *parts=[NSMutableArray new];
    for (NSUInteger i=shared;i<from.count;i++) [parts addObject:@".."];
    for (NSUInteger i=shared;i<to.count;i++) [parts addObject:to[i]];
    return parts.count ? [NSString pathWithComponents:parts] : @".";
}

/* Patch outputs are real files. Other disc entries remain links to the import.
 * Check those links even when the patch marker is current: iOS can relocate an
 * app container without changing either the marker or the source timestamps. */
static BOOL ngb_prepare_game_links(NSString *original, NSString *destination,
                                   NSSet<NSString *> *patched, NSError **error) {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSArray<NSString *> *names=[fm contentsOfDirectoryAtPath:original error:error];
    if (!names) return NO;
    for (NSString *name in names) {
        if ([patched containsObject:name]) continue;
        NSString *source=[original stringByAppendingPathComponent:name];
        NSString *target=[destination stringByAppendingPathComponent:name];
        NSString *relative=ngb_relative_asset_path(source,destination);
        NSString *link=[fm destinationOfSymbolicLinkAtPath:target error:nil];
        if (link && ![link isEqualToString:relative]) {
            if (![fm removeItemAtPath:target error:error]) return NO;
            link=nil;
        }
        if (!link && ![fm fileExistsAtPath:target]) {
            if (![fm createSymbolicLinkAtPath:target withDestinationPath:relative error:error]) return NO;
        }
        if (![fm fileExistsAtPath:target]) {
            if (error) *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadNoSuchFileError
                userInfo:@{NSFilePathErrorKey:target}];
            return NO;
        }
    }
    return YES;
}
#endif
