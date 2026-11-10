#ifndef NGB_IOS_SAVE_PATHS_H
#define NGB_IOS_SAVE_PATHS_H
#import <Foundation/Foundation.h>

/* Relative links survive container relocation during an IPA update or restore.
 * Repair only links; an existing real save directory always remains untouched. */
static BOOL ngb_prepare_save_paths(NSString *documents, NSError **error) {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *saves=[documents stringByAppendingPathComponent:@"saves"];
    NSString *localized=[documents stringByAppendingPathComponent:@"saves-korean"];
    if (![fm createDirectoryAtPath:localized withIntermediateDirectories:YES attributes:nil error:error]) return NO;
    for (NSString *name in @[@"UserData",@"TitleData"]) {
        NSString *source=[saves stringByAppendingPathComponent:name];
        NSString *target=[localized stringByAppendingPathComponent:name];
        NSString *relative=[@"../saves" stringByAppendingPathComponent:name];
        if (![fm createDirectoryAtPath:source withIntermediateDirectories:YES attributes:nil error:error]) return NO;
        NSString *link=[fm destinationOfSymbolicLinkAtPath:target error:nil];
        if (link) {
            if (![link isEqualToString:relative]) {
                if (![fm removeItemAtPath:target error:error]) return NO;
                if (![fm createSymbolicLinkAtPath:target withDestinationPath:relative error:error]) return NO;
            }
        } else {
            BOOL directory=NO;
            if ([fm fileExistsAtPath:target isDirectory:&directory]) {
                if (!directory) {
                    if (error) *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteFileExistsError
                        userInfo:@{NSFilePathErrorKey:target}];
                    return NO;
                }
            } else if (![fm createSymbolicLinkAtPath:target withDestinationPath:relative error:error]) return NO;
        }
        BOOL directory=NO;
        if (![fm fileExistsAtPath:target isDirectory:&directory] || !directory) {
            if (error) *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadNoSuchFileError
                userInfo:@{NSFilePathErrorKey:target}];
            return NO;
        }
    }
    return YES;
}
#endif
