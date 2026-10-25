/* Exercise the actual on-device patcher with synthetic, non-game data. */
#import "korean.m"

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"Korean archive check failed at %d\n",__LINE__); return 1; } } while (0)
int main(void)
{
    @autoreleasepool {
        NSFileManager *fm=NSFileManager.defaultManager;
        NSString *scratch=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        NSString *assets=[scratch stringByAppendingPathComponent:@"members/test.afs"];
        CHECK([fm createDirectoryAtPath:assets withIntermediateDirectories:YES attributes:nil error:nil]);
        NSMutableData *source=[NSMutableData dataWithLength:8192];
        unsigned char *bytes=source.mutableBytes; memcpy(bytes,"AFS\0",4); put_word(bytes+4,2);
        put_word(bytes+8,2048); put_word(bytes+12,3); put_word(bytes+16,4096); put_word(bytes+20,4);
        put_word(bytes+24,6144); put_word(bytes+28,112);
        bytes[512]=0xA5;
        memcpy(bytes+6144+96,"metadata-padding",16);
        memcpy(bytes+2048,"old",3); memcpy(bytes+4096,"keep",4);
        memcpy(bytes+6144,"text.mes",9); memcpy(bytes+6192,"other.bin",10);
        put_word(bytes+6144+44,3); put_word(bytes+6192+44,4);
        NSString *input=[scratch stringByAppendingPathComponent:@"source.afs"];
        NSString *output=[scratch stringByAppendingPathComponent:@"patched.afs"];
        CHECK([source writeToFile:input atomically:YES]);
        NSMutableData *replacement=[NSMutableData dataWithLength:5001];
        memset(replacement.mutableBytes,'K',replacement.length);
        CHECK([replacement writeToFile:[assets stringByAppendingPathComponent:@"text.mes"] atomically:YES]);
        NSDictionary *member=@{@"name":@"text.mes",@"source_sha256":hash_data([NSData dataWithBytes:"old" length:3]),
                                @"output_sha256":hash_data(replacement)};
        NSArray *layout=@[@{@"index":@0,@"name":@"text.mes",@"offset":@2048,@"size":@3},
                          @{@"index":@1,@"name":@"other.bin",@"offset":@4096,@"size":@4}];
        NSDictionary *spec=@{@"path":@"test.afs",@"members":@[member],@"entries":layout};
        NSError *error=nil;
        CHECK(patch_afs(input,output,spec,scratch,&error));
        NSData *result=[NSData dataWithContentsOfFile:output]; const unsigned char *resultBytes=result.bytes;
        CHECK([[NSData dataWithContentsOfFile:input] isEqual:source]);
        // The first grown member shifts the following member. Appending the
        // translated bytes while retaining old offsets must fail this fixture.
        CHECK(result.length==12288 && word(resultBytes+4)==2);
        CHECK(word(resultBytes+8)==2048 && word(resultBytes+12)==5001);
        CHECK(word(resultBytes+16)==8192 && !memcmp(resultBytes+8192,"keep",4));
        CHECK(word(resultBytes+24)==10240 && word(resultBytes+28)==112);
        CHECK(word(resultBytes+10240+44)==5001);
        NSMutableData *reference=[NSMutableData dataWithLength:12288];
        unsigned char *ref=reference.mutableBytes;
        memcpy(ref,source.bytes,2048); memcpy(ref+2048,replacement.bytes,5001);
        memcpy(ref+8192,"keep",4); memcpy(ref+10240,bytes+6144,112);
        put_word(ref+12,5001); put_word(ref+16,8192); put_word(ref+24,10240);
        put_word(ref+10240+44,5001);
        CHECK([result isEqual:reference]);
        NSMutableData *metadataSource=[source mutableCopy];
        put_word((unsigned char *)metadataSource.mutableBytes+6144+44,0xAABBCCDD);
        NSString *metadataInput=[scratch stringByAppendingPathComponent:@"metadata.afs"];
        NSString *metadataOutput=[scratch stringByAppendingPathComponent:@"metadata-output.afs"];
        CHECK([metadataSource writeToFile:metadataInput atomically:YES]);
        CHECK(patch_afs(metadataInput,metadataOutput,spec,scratch,&error));
        NSData *metadataResult=[NSData dataWithContentsOfFile:metadataOutput];
        CHECK(word((const unsigned char *)metadataResult.bytes+10240+44)==0xAABBCCDD);
        NSArray *badOrder=@[layout[1],layout[0]];
        NSString *orderOutput=[scratch stringByAppendingPathComponent:@"wrong-order.afs"];
        CHECK(!patch_afs(input,orderOutput,@{@"path":@"test.afs",@"members":@[member],@"entries":badOrder},scratch,&error));
        CHECK(![fm fileExistsAtPath:orderOutput]);
        NSMutableDictionary *wrong=[member mutableCopy]; wrong[@"source_sha256"]=@"invalid";
        NSString *rejected=[scratch stringByAppendingPathComponent:@"rejected.afs"];
        CHECK(!patch_afs(input,rejected,@{@"path":@"test.afs",@"members":@[wrong]},scratch,&error));
        CHECK(![fm fileExistsAtPath:rejected]);
        NSString *blob=[scratch stringByAppendingPathComponent:@"source.bin"];
        NSString *patchedBlob=[scratch stringByAppendingPathComponent:@"patched.bin"];
        NSData *original=[@"abcdef" dataUsingEncoding:NSUTF8StringEncoding];
        NSData *expected=[@"abXXef" dataUsingEncoding:NSUTF8StringEncoding];
        CHECK([original writeToFile:blob atomically:YES]);
        CHECK(patch_blob(blob,patchedBlob,@{@"changes":@[@{@"offset":@2,@"data":@"WFg="}]},
                         hash_data(original),hash_data(expected),&error));
        CHECK([[NSData dataWithContentsOfFile:patchedBlob] isEqual:expected]);
        // Migrating the disposable Korean cache must not follow or erase the
        // links to the player's real save folders.
        NSString *localized=[scratch stringByAppendingPathComponent:@"saves-korean"];
        for (NSString *name in @[@"UserData",@"TitleData"]) {
            NSString *originalSave=[[scratch stringByAppendingPathComponent:@"saves"] stringByAppendingPathComponent:name];
            CHECK([fm createDirectoryAtPath:originalSave withIntermediateDirectories:YES attributes:nil error:nil]);
            CHECK([original writeToFile:[originalSave stringByAppendingPathComponent:@"save.bin"] atomically:YES]);
            CHECK([fm createDirectoryAtPath:localized withIntermediateDirectories:YES attributes:nil error:nil]);
            CHECK([fm createSymbolicLinkAtPath:[localized stringByAppendingPathComponent:name] withDestinationPath:originalSave error:nil]);
        }
        NSString *cache=[localized stringByAppendingPathComponent:@"Cache"];
        CHECK([fm createDirectoryAtPath:cache withIntermediateDirectories:YES attributes:nil error:nil]);
        CHECK([original writeToFile:[cache stringByAppendingPathComponent:@"obsolete.afs"] atomically:YES]);
        CHECK(reset_korean_cache([scratch stringByAppendingPathComponent:@"korean-game"],&error));
        CHECK(![fm fileExistsAtPath:cache]);
        for (NSString *name in @[@"UserData",@"TitleData"])
            CHECK([[NSData dataWithContentsOfFile:[[localized stringByAppendingPathComponent:name] stringByAppendingPathComponent:@"save.bin"]] isEqual:original]);
        CHECK([fm removeItemAtPath:scratch error:nil]);
        puts("Korean canonical AFS offsets/padding, metadata preservation, layout/preimage rejection and save-safe cache migration: OK");
    }
    return 0;
}
