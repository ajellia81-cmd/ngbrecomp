# Clean macOS-on-iOS comparison build

The game engine is compiled for macOS ARM64 from master 5fc57d84 (0.9.29),
then converted by macos-on-ios c2562a333d4de4c08da4ceb4a4f1485bddb84e04.
The iPhoneOS SDK builds compatibility UI/haptic libraries only. It does not
build the game engine. See migration.json for included and excluded features.

This first device comparison deliberately has no Smooth Motion and no stutter
fixes. The original Mac shader generator, compiler, cache and prewarm functions
are checked against their original source. Original shader seed resources are
bundled through the normal Mac resource layout. No forced compilation policy,
loading acceleration, shader-ready gate or demo-delay gate is installed.

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

An IPA compiling successfully does not establish device compatibility or fix
stutter. The owner will compare the automatic real-time demo first; performance
investigation and Smooth Motion follow that baseline, as explicitly requested.
