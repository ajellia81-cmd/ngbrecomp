#import "../korean/game_links.h"
#include <assert.h>
#include <stdio.h>
static void check(BOOL ok, NSError *error) {
    if (!ok) { fprintf(stderr,"asset link fixture failed: %s\n",error.description.UTF8String); abort(); }
}
int main(int argc, char **argv) { @autoreleasepool {
    assert(argc==2);
    NSFileManager *fm=NSFileManager.defaultManager;
    NSString *root=[NSString stringWithUTF8String:argv[1]];
    NSString *container=[root stringByAppendingPathComponent:@"old"];
    NSString *original=[container stringByAppendingPathComponent:@"Documents/import/game"];
    NSString *output=[container stringByAppendingPathComponent:@"Documents/korean-game"];
    NSError *error=nil;
    check([fm createDirectoryAtPath:original withIntermediateDirectories:YES attributes:nil error:&error],error);
    check([fm createDirectoryAtPath:output withIntermediateDirectories:YES attributes:nil error:&error],error);
    NSData *data=[@"synthetic asset" dataUsingEncoding:NSUTF8StringEncoding];
    for (NSString *name in @[@"disc.dat",@"patched.dat"])
        check([data writeToFile:[original stringByAppendingPathComponent:name] options:0 error:&error],error);
    check([fm createDirectoryAtPath:[original stringByAppendingPathComponent:@"movies"] withIntermediateDirectories:NO attributes:nil error:&error],error);
    check([data writeToFile:[original stringByAppendingPathComponent:@"movies/intro.dat"] options:0 error:&error],error);
    NSString *patched=[output stringByAppendingPathComponent:@"patched.dat"];
    NSData *patch=[@"synthetic translated output" dataUsingEncoding:NSUTF8StringEncoding];
    check([patch writeToFile:patched options:0 error:&error],error);
    NSSet *excluded=[NSSet setWithObject:@"patched.dat"];
    // Legacy absolute link becomes dangling when the entire container moves.
    check([fm createSymbolicLinkAtPath:[output stringByAppendingPathComponent:@"disc.dat"]
          withDestinationPath:[original stringByAppendingPathComponent:@"disc.dat"] error:&error],error);
    NSString *moved=[root stringByAppendingPathComponent:@"new"];
    check([fm moveItemAtPath:container toPath:moved error:&error],error);
    original=[moved stringByAppendingPathComponent:@"Documents/import/game"];
    output=[moved stringByAppendingPathComponent:@"Documents/korean-game"];
    assert(![fm fileExistsAtPath:[output stringByAppendingPathComponent:@"disc.dat"]]);
    check(ngb_prepare_game_links(original,output,excluded,&error),error);
    assert([[NSData dataWithContentsOfFile:[output stringByAppendingPathComponent:@"disc.dat"]] isEqual:data]);
    assert([[NSData dataWithContentsOfFile:[output stringByAppendingPathComponent:@"movies/intro.dat"]] isEqual:data]);
    assert([[NSData dataWithContentsOfFile:[output stringByAppendingPathComponent:@"patched.dat"]] isEqual:patch]);
    assert([[fm destinationOfSymbolicLinkAtPath:[output stringByAppendingPathComponent:@"disc.dat"] error:nil]
            isEqualToString:@"../import/game/disc.dat"]);
    // New relative links keep working on a second relocation, before repair.
    NSString *again=[root stringByAppendingPathComponent:@"again"];
    check([fm moveItemAtPath:moved toPath:again error:&error],error);
    original=[again stringByAppendingPathComponent:@"Documents/import/game"];
    output=[again stringByAppendingPathComponent:@"Documents/korean-game"];
    assert([[NSData dataWithContentsOfFile:[output stringByAppendingPathComponent:@"movies/intro.dat"]] isEqual:data]);
    check(ngb_prepare_game_links(original,output,excluded,&error),error);
    // A missing source directory must fail instead of passing a cached marker.
    assert(!ngb_prepare_game_links([root stringByAppendingPathComponent:@"absent"],output,excluded,&error));
    puts("Korean asset relocation and patch preservation fixtures passed");
    return 0;
}}
