// AppKit alerts used by SDL must wait for a real UIKit button selection.
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include <stdio.h>

@interface NgbDialogButton : NSObject
@property(nonatomic,copy) NSString *title,*keyEquivalent;
@property NSUInteger keyEquivalentModifierMask;
@property BOOL enabled;
@end
@implementation NgbDialogButton
@end

@interface NSAlert : NSObject
@property(nonatomic,copy) NSString *messageText,*informativeText;
@property NSInteger alertStyle;
@property(nonatomic,strong) id icon;
@property(nonatomic,strong) NSMutableArray<NgbDialogButton *> *buttons;
@end

static UIViewController *NgbDialogController(void){
    for(UIWindow *window in UIApplication.sharedApplication.windows)if(window.isKeyWindow){
        UIViewController *controller=window.rootViewController;
        while(controller.presentedViewController)controller=controller.presentedViewController;
        if(![controller isKindOfClass:UIAlertController.class])return controller;
    }
    return nil;
}
static void NgbPresentAlert(NSAlert *model,void (^completion)(NSInteger)){
    UIViewController *controller=NgbDialogController();
    if(!controller){fprintf(stderr,"[Dialog] no active presenter; returning cancellation\n");completion(-1001);return;}
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:model.messageText ?: @"Ninja Gaiden Black"
        message:model.informativeText preferredStyle:UIAlertControllerStyleAlert];
    NSArray<NgbDialogButton *> *buttons=model.buttons;
    if(!buttons.count){NgbDialogButton *ok=[NgbDialogButton new];ok.title=@"OK";ok.enabled=YES;buttons=@[ok];}
    __block BOOL hasCancel=NO;
    [buttons enumerateObjectsUsingBlock:^(NgbDialogButton *button,NSUInteger index,BOOL *stop){
        (void)stop;
        UIAlertActionStyle style=!hasCancel && [button.keyEquivalent isEqualToString:@"\033"] ? UIAlertActionStyleCancel : UIAlertActionStyleDefault;
        if(style==UIAlertActionStyleCancel)hasCancel=YES;
        UIAlertAction *action=[UIAlertAction actionWithTitle:button.title ?: @"OK" style:style handler:^(UIAlertAction *selected){
            (void)selected;fprintf(stderr,"[Dialog] selected button %lu\n",(unsigned long)index);completion(1000+(NSInteger)index);
        }];
        action.enabled=button.enabled;[alert addAction:action];
        if([button.keyEquivalent isEqualToString:@"\r"])alert.preferredAction=action;
    }];
    [controller presentViewController:alert animated:YES completion:nil];
    fprintf(stderr,"[Dialog] native alert presented: %lu buttons\n",(unsigned long)buttons.count);
}
static BOOL sheetDone;
static NSInteger sheetResponse;
static NSInteger NgbRunSheet(id self,SEL command,id window){
    (void)self;(void)command;(void)window;
    while(!sheetDone)CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.02,true);
    NSInteger response=sheetResponse;sheetDone=NO;return response;
}
static void NgbStopSheet(id self,SEL command,NSInteger response){(void)self;(void)command;sheetResponse=response;sheetDone=YES;}
static void NgbAbortSheet(id self,SEL command){NgbStopSheet(self,command,-1001);}
__attribute__((constructor)) static void NgbInstallModalApplicationMethods(void){
    Class application=objc_getClass("NSApplication");
    class_addMethod(application,sel_registerName("runModalForWindow:"),(IMP)NgbRunSheet,"q@:@");
    class_addMethod(application,sel_registerName("stopModalWithCode:"),(IMP)NgbStopSheet,"v@:q");
    class_addMethod(application,sel_registerName("abortModal"),(IMP)NgbAbortSheet,"v@:");
}
@implementation NSAlert
- (instancetype)init{if((self=[super init]))_buttons=[NSMutableArray new];return self;}
- (id)addButtonWithTitle:(NSString *)title{
    NgbDialogButton *button=[NgbDialogButton new];button.title=title;button.keyEquivalent=@"";button.enabled=YES;
    [_buttons addObject:button];return button;
}
- (NSInteger)runModal{
    __block NSInteger response=-1001;__block BOOL done=NO;
    dispatch_semaphore_t semaphore=dispatch_semaphore_create(0);
    void (^present)(void)=^{NgbPresentAlert(self,^(NSInteger selected){response=selected;done=YES;dispatch_semaphore_signal(semaphore);});};
    if(NSThread.isMainThread){present();while(!done)CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.02,true);}
    else {dispatch_async(dispatch_get_main_queue(),present);dispatch_semaphore_wait(semaphore,DISPATCH_TIME_FOREVER);}
    return response;
}
- (void)beginSheetModalForWindow:(id)window completionHandler:(void (^)(NSInteger))handler{
    (void)window;sheetDone=NO;
    void (^present)(void)=^{NgbPresentAlert(self,^(NSInteger response){if(handler)handler(response);else NgbStopSheet(nil,NULL,response);});};
    if(NSThread.isMainThread)present();else dispatch_async(dispatch_get_main_queue(),present);
}
@end
