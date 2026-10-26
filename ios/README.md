# Ninja Gaiden Black for iOS

An experimental iPhoneOS ARM64 port of the real game engine using SDL UIKit
and MoltenVK. This is the game target, separate from the older bring-up harness.
Use your own supported game data. Compiling and packaging an IPA does not
establish compatibility or sustained performance on every device.

## Release build

`ios/build_engine.sh` selects and verifies CMake's Release configuration.
`ios/package_ipa.sh` checks the linked engine, runtime/build identifiers,
Release metadata, bundled framework, app name and iPhone icon catalog before
packaging. The app is named **Ninja Gaiden Black**. Its icon is a square crop
of the supplied cover, rasterized to Apple's required sizes without stretching.

The production workflow delivers an encrypted unsigned IPA artifact. Download
and installation instructions are in [INSTALL.md](INSTALL.md). Code signing
or a compatible configured host is required; no developer certificate is
provided. Original game files and generated game C are excluded from source
exports and raw-data IPA packaging checks.

Release builds do not create developer console logs, build-info text files or
F7 draw captures. Periodic kernel and Smooth Motion diagnostics are disabled.
The staged IPA excludes development logs, headers, module maps, static/object
archives and debug-symbol bundles; runtime assets and licenses are retained.
The optional in-game FPS display remains a user setting.

## Features and limits

- Original game engine with an iOS Vulkan/MoltenVK renderer.
- Optional Korean support prepared by the private build pipeline.
- Physical controller support and a touch overlay when no controller is connected.
- Experimental high-refresh Smooth Motion; game simulation remains at 60 Hz.
- FPS display can be enabled or disabled in settings.

Smooth Motion adds rendering work and cannot guarantee 120 FPS in every scene.
See [120FPS.md](120FPS.md) for its architecture and limits. For another project,
start with [the agent handoff](../docs/SMOOTH_MOTION_PORTING.md) and
`tools/export_smooth_motion.py`; the unchanged VF3 common core is reusable,
while another renderer needs its own scene adapter.

Keep private game inputs, saves, device reports and captures out of public
issues and source commits. Preserve the repository and third-party licenses
when distributing source or applications.
