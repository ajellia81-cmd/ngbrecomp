# Ninja Gaiden Black iOS port

An experimental unsigned **real-engine IPA** has been built. Physical-device
startup, title screen and gameplay are **not verified**; this is not a claim of
a completed playable port. The older NGBBringup diagnostic app is a separate target.

- Build source: `9e28c13556311063e58d209c351f4e12bddb6916`, branch `ios-port`.
- [Successful Actions run, attempt 2](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40103624524).
- [Encrypted IPA artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40103624524/artifacts/12563917080).
- Unsigned IPA: **14,239,945 bytes**, containing **11,740 linked lifted game functions**,
  the real engine/runtime, SDL UIKit entry point and MoltenVK framework.
- IPA SHA-256: `1f54f2e8514b3e234f9142567bbc938bb1242665210070689b54bc749bdce5f3`.
- GitHub artifact ZIP SHA-256: `e7aa0e62154ba30ecd9e806c3b12eaa0344b89bf284c199a6d2b83873599a392`.

See [INSTALL.md](INSTALL.md) for download, LiveContainer import and game data.
The download is an encrypted 7z inside GitHub's artifact ZIP. It expires seven
days after upload. The password is the owner's `NGB_IPA_PASSWORD` Actions secret.
No disc, XBE, generated C or unencrypted IPA was published or committed.

## Implemented build and platform path

The original CMake game target links actual generated game code, handwritten
engine, Direct3D Vulkan renderer and Xbox runtime. The iOS-only target settings
add SDL's UIKit main, `ios/app.m`, an iPhone Info.plist and bundled MoltenVK 1.4.2.
The public runtime/renderer can also be compiled without private inputs using
`NGB_RENDERER_CHECK_ONLY=ON`; that option does not create a game executable.

Existing Apple ARM64 fibers, signal scheduling and MMIO decoding were reused.
The iPhoneOS SDK rejects `mach_vm.h`, so `platform/apple_vm.h` selects public,
pointer-sized `vm_*` APIs for iOS while retaining desktop `mach_vm_*` calls.
The existing Apple D3D path is selected; it already relaxes reference NV2A draw
requirements such as geometry shaders. The legacy desktop OpenGL 3.3 D3D8 backend
is excluded on iOS; accidental legacy D3D8 calls fail to link rather than use stubs.

SDL2, GLib and glslang are built for iPhoneOS via a pinned vcpkg revision and a
Release-only static device triplet. CoreFoundation is linked for static libintl.
MoltenVK is loaded from the bundled dynamic framework and seeds volk before the
renderer starts. Official MoltenVK archive bytes are checked against a SHA-256.

The app imports the owner's ISO or extracted folder into `Documents/game` and
validates the supported USA executable. Saves, configuration and shader caches
use separate sandbox directories. Archive subprocesses, desktop setup and exec
fallbacks are disabled in the iOS app; separate arcade executable handoff is not
implemented. The game's existing in-process soft reset remains available.

Hardware controllers use SDL's iOS GameController backend. With no hardware pad,
the UIKit overlay drives a real SDL virtual game controller: both sticks,
triggers, D-pad, face/shoulder/stick buttons, Start and Back. Controls use runtime
bounds/safe-area insets and release held inputs when the app resigns active.
No iPhone 18 screen dimensions were guessed or hard-coded.

## Verified results

| Check | Observed result |
| --- | --- |
| Owner input | Retail USA XBE size 4,571,136 and FNV-1a `0x3602ba6a6079c8cd` accepted |
| Private input SHA-256 | `5fa3997b0d518a9db3867ca6ae2942c99e5349fd138f536ff56772e1a0202200` |
| Generated game code | 48 real C translation units generated and compiled for ARM64 |
| Whole engine | Original game target linked for iPhoneOS; no lifted-function stubs |
| UIKit game app | Bundle linked with real engine, SDL UIKit entry and MoltenVK |
| IPA | iPhoneOS platform and engine symbols checked; ZIP integrity passed |
| Encrypted delivery | 7z password/integrity test and artifact upload passed |
| VM semantics | Allocation collision, protected writes and protection restoration passed on macOS and iPhone simulator |
| Scheduler/thread regressions | Existing macOS and Linux regressions passed |
| Touch controller contract | SDL virtual pad button/stick/trigger mapping passed headless Linux checks; not an iPhone UI test |
| Extraction helper | Cloud PowerShell 7.5.4 synthetic disc tests passed; real Windows execution not observed |

[Simulator VM run](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40097568525)
and [runtime/renderer build](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40099391364).
The macOS runner used Xcode 16.4/iPhoneOS 18.5 SDK with a deployment target of iOS 15.
A deployment target is not validation on iOS 27. Windows and desktop macOS full-game
builds were not run here; their default build paths were retained.

The baseline Linux VEH regression previously failed at test_main.c:87 and remains
unresolved. Its post-protection read passes through its own MMIO handler, so that
failure alone does not establish a production VM bug. Do not report all tests as passing.

## Unverified and remaining work

- No iPhone 18 Pro Max/iOS 27 device or LiveContainer run was available. App launch,
  full game initialization, title screen, gameplay, performance, graphics accuracy,
  real controller/touch behavior and audible output remain unverified.
- The real LiveContainer host must permit the runtime's roughly 4 GiB guest virtual
  reservation. Small-region VM behavior in a simulator does not establish that.
- Vulkan 1.3/shared-device features, Metal presentation and GPU-specific behavior
  need physical-device checks. No successful rendered game frame is claimed.
- Robust coordinated guest-clock/GPU suspension and background resume are not
  complete. Current lifecycle handling releases touch input and uses SDL audio
  support; initial tests should keep the game in the foreground.
- 120 Hz presentation and preservation of game behavior at that rate have not
  been measured. The plist permits high refresh, but guest timing was not doubled.
- Separate arcade executables and external archive extraction are not supported
  by this iOS app.

These gaps mean the user's playable-on-device completion criterion remains open,
even though a genuine game-engine IPA is now available.

## Private inputs and source history

See [PRIVATE_BUILD.md](PRIVATE_BUILD.md) for Actions secrets, privacy and packaging,
and [SMALL_INPUT.md](SMALL_INPUT.md) for the optional read-only PowerShell extractor.
Only the executable is needed to generate code; the complete disc data is still
needed on the phone. Keep generated C and game files out of git and public caches.

The original audited head was `fd3b1e569b3c8da5f697f15f69e86f13a7df33b8`.
At that head the branch only added the six bring-up files. The production Apple
implementations were already present in base `5fc57d8` and were reused rather than
rewritten. Subsequent commits in this branch implement the game app described above.
Upstream was not modified. No user desktop was accessed by the agent.
