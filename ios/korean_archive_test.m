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
        put_word(bytes+24,6144); put_word(bytes+28,96);
        memcpy(bytes+2048,"old",3); memcpy(bytes+4096,"keep",4);
        memcpy(bytes+6144,"text.mes",9); memcpy(bytes+6192,"other.bin",10);
        put_word(bytes+6144+44,3); put_word(bytes+6192+44,4);
        NSString *input=[scratch stringByAppendingPathComponent:@"source.afs"];
        NSString *output=[scratch stringByAppendingPathComponent:@"patched.afs"];
        CHECK([source writeToFile:input atomically:YES]);
        NSData *replacement=[@"translated synthetic text" dataUsingEncoding:NSUTF8StringEncoding];
        CHECK([replacement writeToFile:[assets stringByAppendingPathComponent:@"text.mes"] atomically:YES]);
        NSDictionary *member=@{@"name":@"text.mes",@"source_sha256":hash_data([NSData dataWithBytes:"old" length:3]),
                                @"output_sha256":hash_data(replacement)};
        NSDictionary *spec=@{@"path":@"test.afs",@"members":@[member]};
        NSError *error=nil;
        CHECK(patch_afs(input,output,spec,scratch,&error));
        NSData *result=[NSData dataWithContentsOfFile:output]; const unsigned char *resultBytes=result.bytes;
        CHECK([[NSData dataWithContentsOfFile:input] isEqual:source]);
        CHECK(word(resultBytes+4)==2 && word(resultBytes+8)>=source.length);
        CHECK(word(resultBytes+12)==replacement.length);
        CHECK(!memcmp(resultBytes+word(resultBytes+8),replacement.bytes,replacement.length));
        CHECK(word(resultBytes+16)==4096 && !memcmp(resultBytes+4096,"keep",4));
        CHECK(word(resultBytes+word(resultBytes+24)+44)==replacement.length);
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
        CHECK([fm removeItemAtPath:scratch error:nil]);
        puts("Korean AFS append/reindex, original preservation, preimage rejection and binary patch: OK");
    }
    return 0;
}
