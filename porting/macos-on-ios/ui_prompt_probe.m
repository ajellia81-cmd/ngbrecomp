/* Bounded UI metadata only. No screenshots, texture bytes or GPU queries. */
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#include <stdint.h>

void ngb_ui_prompt_record(uint32_t frame,uint32_t sprite,uint32_t texture,int letter,
                         int tw,int th,float x0,float y0,float x1,float y1,
                         float u0,float v0,float u1,float v1,uint32_t caller,uint32_t parent)
{
    static dispatch_queue_t queue;
    static dispatch_once_t once;
    dispatch_once(&once,^{ queue=dispatch_queue_create("ngb.ui.prompt",DISPATCH_QUEUE_SERIAL); });
    dispatch_async(queue,^{ @autoreleasepool {
        static NSFileHandle *file;
        static int attempted;
        if(!attempted){
            attempted=1;
            NSString *documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
            NSString *path=[documents stringByAppendingPathComponent:@"ui-result-diagnostics.log"];
            [NSFileManager.defaultManager createFileAtPath:path contents:nil attributes:nil];
            file=[NSFileHandle fileHandleForWritingAtPath:path];
        }
        if(!file)return;
        NSDictionary *row=@{@"frame":@(frame),@"caller":@(caller),@"parent":@(parent),@"sprite":@(sprite),@"texture":@(texture),@"ascii":@(letter),
            @"texture_size":@[@(tw),@(th)],@"rect":@[@(x0),@(y0),@(x1),@(y1)],@"uv":@[@(u0),@(v0),@(u1),@(v1)]};
        NSData *data=[NSJSONSerialization dataWithJSONObject:row options:0 error:nil];
        if(data){@try{[file writeData:data];[file writeData:[@"\n" dataUsingEncoding:NSUTF8StringEncoding]];}@catch(NSException *exception){(void)exception;file=nil;}}
    }});
}
