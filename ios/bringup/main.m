#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#include "../../src/port.h"
@interface NGBAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end
@implementation NGBAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [UIViewController new];
    controller.view.backgroundColor = [UIColor colorWithRed:0.06 green:0.08 blue:0.12 alpha:1.0];
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectZero];
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *testPath = [documents stringByAppendingPathComponent:@"ngb-bringup-test.txt"];
    NSError *error = nil;
    BOOL writable = [@"NGB sandbox write test" writeToFile:testPath atomically:YES encoding:NSUTF8StringEncoding error:&error];
    NSString *status = writable ? @"Sandbox write: OK" : [NSString stringWithFormat:@"Sandbox write: FAILED (%@)", error.localizedDescription];
    char configPath[1024] = {0};
    ngb_config_dir(configPath, sizeof(configPath), 1);
    BOOL configOK = access(configPath, W_OK) == 0;
    NSUInteger controllers = GCController.controllers.count;
    NSString *diagnostics = [NSString stringWithFormat:@"%@\\nConfig directory: %@\\nControllers: %lu", status, configOK ? @"OK" : @"FAILED", (unsigned long)controllers];
    label.text = [NSString stringWithFormat:@"Ninja Gaiden Black\\niOS bring-up\\n\\n%@\\n\\nRuntime integration pending", diagnostics];
    label.textColor = UIColor.whiteColor;
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [controller.view addSubview:label];
    [NSLayoutConstraint activateConstraints:@[
      [label.centerXAnchor constraintEqualToAnchor:controller.view.centerXAnchor],
      [label.centerYAnchor constraintEqualToAnchor:controller.view.centerYAnchor],
      [label.leadingAnchor constraintGreaterThanOrEqualToAnchor:controller.view.leadingAnchor constant:24],
      [label.trailingAnchor constraintLessThanOrEqualToAnchor:controller.view.trailingAnchor constant:-24]
    ]];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}
@end
int main(int argc, char *argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(NGBAppDelegate.class)); }
}
