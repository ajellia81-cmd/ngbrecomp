# macOS-on-iOS port with VF3 Smooth Motion

The game engine is compiled for macOS ARM64 from master 5fc57d84 (0.9.29),
then converted by macos-on-ios c2562a333d4de4c08da4ceb4a4f1485bddb84e04.
The iPhoneOS SDK builds compatibility UI/haptic libraries only. It does not
build the game engine. See migration.json for included and excluded features.

3.0.14 was confirmed on-device to fix Korean startup, slowdown and stutter.
Smooth Motion now uses the unchanged latest VF3 production interpolation core,
with an NGB scene/replay bridge and a display-only 120 Hz cadence. It defaults
to Off. The original Mac shader compiler, cache and prewarm methods remain
unchanged. No loading acceleration, shader-ready gates, extra motion prewarm,
demo-delay gates or old NGB adaptive cooldown are included. See
../../docs/SMOOTH_MOTION_PORTING.md for exact boundaries and verification.

Portable Korean data/code hooks and the accepted text/sprite layout changes are
carried separately from the old iOS runtime. Native screen dimensions and the
selected aspect ratio determine the optional iPhone Native render target.
The Mac app enters the game directly, avoiding execv and a separate Qt installer
inside iOS. A small versioned callback table connects the original installer
and SDL virtual controller API to the UIKit compatibility bridge.

The app name and approved cover icon are retained. Save/config folders are
retained, imported game data is not deleted, and a separate shaders-macos-on-ios
folder prevents reuse of shaders from the old port. Only encrypted IPA artifacts
are uploaded. Private disc input, generated C and compiler diagnostics stay on
the disposable runner. User passwords and disc URLs are never printed.

An IPA compiling successfully does not establish device Smooth Motion correctness
or sustained 120 FPS. Compare Off and On after installation using actual gameplay
and the submitted-picture FPS counter, not display-link callbacks alone.


## Corrections after the first device comparison

The 3.0.12 Korean boot trace stopped at the DirectSound effects-image wait:
command 3 was at the relocated mailbox, not the original fixed-address probe.
The language adapter now services only the two verified title wait sites,
checks the actual operand and mapped range, and preserves explicit off/manual
policies. No guest scheduler or kernel timing changes are needed.

The English comparison ran near 30 FPS even in menus. Both boot-thread samples
were blocked in CAMetalDrawable acquisition during synchronous vkQueueSubmit.
The converted frontend now selects MoltenVK's supported asynchronous submission
policy, keeps a spare swapchain image within driver limits, and explicitly
requests the base game's 60 Hz UIKit cadence. The native Mac path keeps its
original image policy. Shader generation, compilation, seed/cache and prewarm
functions remain identical to the Mac baseline; this is presentation work.
The packaged driver configuration is read back in CI without creating a window
or playing sound. Device throughput must still be measured after installation.
