#import <UIKit/UIKit.h>
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
    label.text = [NSString stringWithFormat:@"Ninja Gaiden Black\\niOS bring-up\\n\\n%@\\n\\nRuntime integration pending", status];
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
