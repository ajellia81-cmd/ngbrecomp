#import "../save_paths.h"
#include <assert.h>
#include <stdio.h>
static void check(BOOL ok, NSError *error) {
    if (!ok) { fprintf(stderr,"save path fixture failed: %s\n",error.description.UTF8String); abort(); }
}
int main(int argc, char **argv) { @autoreleasepool {
    assert(argc==2);
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *root=[NSString stringWithUTF8String:argv[1]];
    NSString *documents=[root stringByAppendingPathComponent:@"original/Documents"];
    NSError *error=nil;
    check(ngb_prepare_save_paths(documents,&error),error);
    NSString *user=[documents stringByAppendingPathComponent:@"saves/UserData"];
    NSString *title=[documents stringByAppendingPathComponent:@"saves/TitleData"];
    NSString *localized=[documents stringByAppendingPathComponent:@"saves-korean"];
    NSData *payload=[@"synthetic save marker" dataUsingEncoding:NSUTF8StringEncoding];
    check([payload writeToFile:[user stringByAppendingPathComponent:@"marker"] options:0 error:&error],error);
    // Model a legacy absolute link, followed by an app container move.
    NSString *titleLink=[localized stringByAppendingPathComponent:@"TitleData"];
    check([fm removeItemAtPath:titleLink error:&error],error);
    check([fm createSymbolicLinkAtPath:titleLink withDestinationPath:title error:&error],error);
    NSString *moved=[root stringByAppendingPathComponent:@"relocated"];
    check([fm moveItemAtPath:[root stringByAppendingPathComponent:@"original"] toPath:moved error:&error],error);
    documents=[moved stringByAppendingPathComponent:@"Documents"];
    check(ngb_prepare_save_paths(documents,&error),error);
    localized=[documents stringByAppendingPathComponent:@"saves-korean"];
    titleLink=[localized stringByAppendingPathComponent:@"TitleData"];
    assert([[fm destinationOfSymbolicLinkAtPath:titleLink error:nil] isEqualToString:@"../saves/TitleData"]);
    assert([[NSData dataWithContentsOfFile:[localized stringByAppendingPathComponent:@"UserData/marker"]] isEqual:payload]);
    // A real localized save directory and its contents must remain intact.
    check([fm removeItemAtPath:titleLink error:&error],error);
    check([fm createDirectoryAtPath:titleLink withIntermediateDirectories:NO attributes:nil error:&error],error);
    NSString *marker=[titleLink stringByAppendingPathComponent:@"keep"];
    check([payload writeToFile:marker options:0 error:&error],error);
    check(ngb_prepare_save_paths(documents,&error),error);
    assert([[NSData dataWithContentsOfFile:marker] isEqual:payload]);
    assert([fm destinationOfSymbolicLinkAtPath:titleLink error:nil]==nil);
    // A file where a directory is needed must report failure, never delete it.
    check([fm removeItemAtPath:titleLink error:&error],error);
    check([payload writeToFile:titleLink options:0 error:&error],error);
    assert(!ngb_prepare_save_paths(documents,&error));
    assert([[NSData dataWithContentsOfFile:titleLink] isEqual:payload]);
    puts("save path relocation and preservation fixtures passed");
    return 0;
}}
