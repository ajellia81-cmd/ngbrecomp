/* Private on-device language overlay. Original disc files are never edited. */
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static NSString *const USAHash = @"5fa3997b0d518a9db3867ca6ae2942c99e5349fd138f536ff56772e1a0202200";

static BOOL stop(NSError **error, NSString *message)
{
    if (error) *error = [NSError errorWithDomain:@"NGBKorean" code:1
        userInfo:@{NSLocalizedDescriptionKey: message}];
    return NO;
}

static BOOL safe_name(NSString *name)
{
    return name.length && ![name isEqual:@"."] && ![name isEqual:@".."] &&
        [name rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"/\\"]].location == NSNotFound;
}

static NSString *hex_digest(const unsigned char *digest)
{
    NSMutableString *text = [NSMutableString stringWithCapacity:64];
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) [text appendFormat:@"%02x", digest[i]];
    return text;
}

static NSString *hash_data(NSData *data)
{
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
    return hex_digest(digest);
}

static NSString *hash_range(FILE *file, uint64_t offset, uint64_t size)
{
    if (fseeko(file, (off_t)offset, SEEK_SET)) return nil;
    unsigned char buffer[65536], digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_CTX context; CC_SHA256_Init(&context);
    while (size) {
        size_t amount = size > sizeof buffer ? sizeof buffer : (size_t)size;
        if (fread(buffer, 1, amount, file) != amount) return nil;
        CC_SHA256_Update(&context, buffer, (CC_LONG)amount); size -= amount;
    }
    CC_SHA256_Final(digest, &context);
    return hex_digest(digest);
}

static uint32_t word(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put_word(unsigned char *p, uint32_t value)
{
    for (unsigned i=0;i<4;i++) p[i]=(unsigned char)(value>>(8*i));
}

static BOOL write_at(FILE *file, uint64_t offset, const void *bytes, size_t count)
{
    return !fseeko(file, (off_t)offset, SEEK_SET) && fwrite(bytes, 1, count, file) == count;
}

static BOOL patch_blob(NSString *source, NSString *destination, NSDictionary *spec,
                       NSString *sourceHash, NSString *outputHash, NSError **error)
{
    NSMutableData *data = [[NSData dataWithContentsOfFile:source options:0 error:error] mutableCopy];
    if (!data) return NO;
    if (![[hash_data(data) lowercaseString] isEqual:sourceHash])
        return stop(error, [@"Incompatible original file: " stringByAppendingString:source.lastPathComponent]);
    for (NSDictionary *change in spec[@"changes"]) {
        NSUInteger offset = [change[@"offset"] unsignedIntegerValue];
        NSData *bytes = [[NSData alloc] initWithBase64EncodedString:change[@"data"] options:0];
        if (!bytes || offset > data.length || offset + bytes.length > 64*1024*1024)
            return stop(error, @"Invalid Korean executable/resource patch range.");
        if (offset + bytes.length > data.length) data.length = offset + bytes.length;
        [data replaceBytesInRange:NSMakeRange(offset, bytes.length) withBytes:bytes.bytes];
    }
    if (spec[@"output_length"]) data.length = [spec[@"output_length"] unsignedIntegerValue];
    if (![hash_data(data) isEqual:outputHash]) return stop(error, @"Korean output fingerprint mismatch.");
    return [data writeToFile:destination options:NSDataWritingAtomic error:error];
}

static BOOL copy_range(FILE *input, FILE *output, uint64_t from, uint64_t to, uint64_t count)
{
    if (fseeko(input,(off_t)from,SEEK_SET) || fseeko(output,(off_t)to,SEEK_SET)) return NO;
    unsigned char buffer[65536];
    while (count) {
        size_t amount=count>sizeof buffer ? sizeof buffer : (size_t)count;
        if (fread(buffer,1,amount,input)!=amount || fwrite(buffer,1,amount,output)!=amount) return NO;
        count-=amount;
    }
    return YES;
}

static BOOL patch_afs(NSString *source, NSString *destination, NSDictionary *spec,
                      NSString *assets, NSError **error)
{
    FILE *input=fopen(source.fileSystemRepresentation,"rb");
    if (!input) return stop(error,@"Cannot open original game archive.");
    unsigned char header[8];
    BOOL ok=fread(header,1,8,input)==8 && !memcmp(header,"AFS\0",4);
    uint32_t count=ok ? word(header+4) : 0;
    if (!ok || !count || count>65536) { fclose(input); return stop(error,@"Invalid AFS header."); }
    NSMutableData *table=[NSMutableData dataWithLength:(NSUInteger)count*8+8];
    if (fread(table.mutableBytes,1,table.length,input)!=table.length) {
        fclose(input); return stop(error,@"Truncated AFS table.");
    }
    unsigned char *entries=table.mutableBytes;
    uint32_t namesAt=word(entries+count*8),namesSize=word(entries+count*8+4);
    fseeko(input,0,SEEK_END); uint64_t originalSize=(uint64_t)ftello(input);
    if (namesSize<(uint64_t)count*48 || namesSize>64*1024*1024 || (uint64_t)namesAt+namesSize>originalSize) {
        fclose(input); return stop(error,@"Invalid AFS filename table.");
    }
    NSMutableData *names=[NSMutableData dataWithLength:namesSize];
    if (fseeko(input,namesAt,SEEK_SET) || fread(names.mutableBytes,1,names.length,input)!=names.length) {
        fclose(input); return stop(error,@"Cannot read AFS filenames.");
    }
    NSMutableDictionary<NSString *,NSNumber *> *indices=[NSMutableDictionary new];
    unsigned char *nameBytes=names.mutableBytes;
    NSArray *expected=spec[@"entries"];
    if (expected && expected.count!=count) {
        fclose(input); return stop(error,@"The archive member count differs from the Korean layout.");
    }
    uint32_t first=UINT32_MAX,expectedFirst=UINT32_MAX;
    for (uint32_t i=0;i<count;i++) {
        size_t length=strnlen((const char *)nameBytes+i*48,32);
        NSString *name=[[NSString alloc] initWithBytes:nameBytes+i*48 length:length encoding:NSASCIIStringEncoding];
        if (!safe_name(name) || indices[name]) { fclose(input); return stop(error,@"Invalid/duplicate AFS member name."); }
        indices[name]=@(i);
        uint32_t offset=word(entries+i*8),size=word(entries+i*8+4);
        if (offset<16+(uint64_t)count*8 || (uint64_t)offset+size>originalSize) {
            fclose(input); return stop(error,@"Invalid AFS member range.");
        }
        if (size && offset<first) first=offset;
        if (expected) {
            NSDictionary *row=expected[i];
            if (![row[@"name"] isEqual:name] || [row[@"size"] unsignedLongLongValue]!=size) {
                fclose(input); return stop(error,@"The archive member order or size differs from the Korean layout.");
            }
            uint32_t at=[row[@"offset"] unsignedIntValue];
            if (size && at<expectedFirst) expectedFirst=at;
        }
    }
    if (first==UINT32_MAX || (expected && first!=expectedFirst)) {
        fclose(input); return stop(error,@"The archive data prefix differs from the Korean layout.");
    }
    NSMutableDictionary<NSNumber *,NSData *> *replacements=[NSMutableDictionary new];
    for (NSDictionary *member in spec[@"members"]) {
        NSString *name=member[@"name"]; NSNumber *index=indices[name];
        if (!index || !safe_name(name)) { fclose(input); return stop(error,@"Required Korean archive member is absent."); }
        unsigned i=index.unsignedIntValue;
        NSString *originalHash=hash_range(input,word(entries+i*8),word(entries+i*8+4));
        if (![originalHash isEqual:member[@"source_sha256"]]) {
            fclose(input); return stop(error,[@"Incompatible USA archive member: " stringByAppendingString:name]);
        }
        NSString *path=[[[assets stringByAppendingPathComponent:@"members"] stringByAppendingPathComponent:spec[@"path"]] stringByAppendingPathComponent:name];
        NSData *data=[NSData dataWithContentsOfFile:path options:0 error:error];
        if (!data || ![hash_data(data) isEqual:member[@"output_sha256"]]) {
            fclose(input); return stop(error,@"Bundled Korean member fingerprint mismatch.");
        }
        replacements[index]=data;
    }
    // The patched game's resource tables use the release's rebuilt offsets.
    // Appending replacements keeps an AFS directory valid but makes those
    // offsets point into unrelated texture bytes. Rebuild every member in order.
    FILE *output=fopen(destination.fileSystemRepresentation,"wb+");
    if (!output) { fclose(input); return stop(error,@"Cannot write the generated Korean archive."); }
    ok=copy_range(input,output,0,0,first);
    uint64_t end=first;
    for (uint32_t i=0;ok && i<count;i++) {
        uint32_t oldOffset=word(entries+i*8),oldSize=word(entries+i*8+4);
        NSData *data=replacements[@(i)];
        uint64_t size=data ? data.length : oldSize;
        end=(end+2047)&~2047ULL;
        if (end+size>UINT32_MAX) { ok=NO; break; }
        ok=data ? write_at(output,end,data.bytes,data.length) : copy_range(input,output,oldOffset,end,oldSize);
        if (!ok) break;
        put_word(entries+i*8,(uint32_t)end); put_word(entries+i*8+4,(uint32_t)size);
        // This field is a size only in some AFS variants; preserve other metadata.
        if (word(nameBytes+i*48+44)==oldSize) put_word(nameBytes+i*48+44,(uint32_t)size);
        end+=size;
    }
    end=(end+2047)&~2047ULL;
    if (ok && end+names.length<=UINT32_MAX) {
        put_word(entries+count*8,(uint32_t)end); put_word(entries+count*8+4,(uint32_t)names.length);
        ok=write_at(output,end,names.bytes,names.length) && write_at(output,8,entries,table.length);
        uint64_t namesEnd=end+names.length,padded=(namesEnd+2047)&~2047ULL;
        unsigned char zero=0;
        if (ok && padded>namesEnd) ok=write_at(output,padded-1,&zero,1);
    } else ok=NO;
    fclose(input);
    if (fclose(output)) ok=NO;
    if (!ok) [NSFileManager.defaultManager removeItemAtPath:destination error:nil];
    return ok ? YES : stop(error,@"Cannot finish the Korean archive; check free storage.");
}

static NSString *const AFSLayoutVersion=@"canonical-afs-v2";
static BOOL reset_korean_cache(NSString *destination,NSError **error)
{
    // Only disposable Xbox cache files. UserData and TitleData hold shared saves.
    NSString *cache=[[destination stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"saves-korean/Cache"];
    NSFileManager *fm=NSFileManager.defaultManager;
    return ![fm fileExistsAtPath:cache] || [fm removeItemAtPath:cache error:error];
}

BOOL ngb_prepare_korean(NSString *original, NSString *destination, NSString *assets,
                        void (^progress)(NSString *), NSError **error)
{
    NSFileManager *fm=NSFileManager.defaultManager;
    NSData *json=[NSData dataWithContentsOfFile:[assets stringByAppendingPathComponent:@"manifest.json"] options:0 error:error];
    NSDictionary *manifest=json ? [NSJSONSerialization JSONObjectWithData:json options:0 error:error] : nil;
    if (!manifest || ![manifest[@"source_xbe_sha256"] isEqual:USAHash]) return stop(error,@"Korean patch capsule is unavailable.");
    NSMutableDictionary *signatures=[NSMutableDictionary new];
    for (NSDictionary *spec in manifest[@"archives"]) {
        NSString *name=spec[@"path"];
        if (!safe_name(name)) return stop(error,@"Unsafe Korean archive name.");
        NSDictionary *attributes=[fm attributesOfItemAtPath:[original stringByAppendingPathComponent:name] error:error];
        if (!attributes) return NO;
        signatures[name]=@[attributes[NSFileSize],@([attributes[NSFileModificationDate] timeIntervalSince1970])];
    }
    NSString *marker=[destination stringByAppendingString:@".state.plist"];
    NSDictionary *previous=[NSDictionary dictionaryWithContentsOfFile:marker];
    if ([previous[@"layout"] isEqual:AFSLayoutVersion] && [previous[@"patch"] isEqual:manifest[@"patch_zip_sha256"]] && [previous[@"sources"] isEqual:signatures] &&
        [fm fileExistsAtPath:[destination stringByAppendingPathComponent:@"default.xbe"]]) return YES;
    NSString *temporary=[destination stringByAppendingString:@".pending"];
    [fm removeItemAtPath:temporary error:nil];
    if (![fm createDirectoryAtPath:temporary withIntermediateDirectories:YES attributes:nil error:error]) return NO;
    NSMutableSet<NSString *> *patched=[NSMutableSet setWithObject:@"default.xbe"];
    for (NSDictionary *spec in manifest[@"archives"]) [patched addObject:spec[@"path"]];
    for (NSDictionary *spec in manifest[@"root_patches"]) [patched addObject:spec[@"path"]];
    BOOL ok=YES;
    for (NSString *name in [fm contentsOfDirectoryAtPath:original error:error]) {
        if ([patched containsObject:name]) continue;
        if (![fm createSymbolicLinkAtPath:[temporary stringByAppendingPathComponent:name]
            withDestinationPath:[original stringByAppendingPathComponent:name] error:error]) { ok=NO; break; }
    }
    if (ok) ok=patch_blob([original stringByAppendingPathComponent:@"default.xbe"],
        [temporary stringByAppendingPathComponent:@"default.xbe"],manifest[@"xbe"],USAHash,manifest[@"output_xbe_sha256"],error);
    unsigned current=0; NSArray *archives=manifest[@"archives"];
    for (NSDictionary *spec in archives) {
        if (!ok) break;
        NSString *name=spec[@"path"];
        if (!safe_name(name)) { ok=stop(error,@"Unsafe Korean archive path."); break; }
        progress([NSString stringWithFormat:@"Preparing Korean game data… %u/%lu",++current,(unsigned long)archives.count]);
        ok=patch_afs([original stringByAppendingPathComponent:name], [temporary stringByAppendingPathComponent:name],spec,assets,error);
    }
    for (NSDictionary *spec in manifest[@"root_patches"]) {
        if (!ok) break; NSString *name=spec[@"path"];
        if (!safe_name(name)) { ok=stop(error,@"Unsafe Korean resource path."); break; }
        ok=patch_blob([original stringByAppendingPathComponent:name],[temporary stringByAppendingPathComponent:name],
                      spec,spec[@"source_sha256"],spec[@"output_sha256"],error);
    }
    if (ok && [fm fileExistsAtPath:destination]) {
        if (!previous) ok=stop(error,@"Move the existing korean-game folder before preparing it.");
        else ok=[fm removeItemAtPath:destination error:error];
    }
    if (ok) ok=[fm moveItemAtPath:temporary toPath:destination error:error];
    if (ok) ok=reset_korean_cache(destination,error);
    if (ok) ok=[@{@"layout":AFSLayoutVersion,@"patch":manifest[@"patch_zip_sha256"],@"sources":signatures} writeToFile:marker atomically:YES];
    else [fm removeItemAtPath:temporary error:nil];
    return ok;
}
