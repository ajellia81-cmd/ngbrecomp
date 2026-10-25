# Ninja Gaiden Black iOS port

An experimental unsigned **real-engine IPA** has been built. The owner reports
successful game startup, New Game selection and working gameplay in LiveContainer
after restarting following the first load. Long-term reliability, performance and
background resume still require testing. The older NGBBringup diagnostic app is
a separate target.

- App version: **0.2.0 (2.51)**; compiled revision and active log path are recorded in `Documents/build-info.txt`.
- Build source: `7471451017e3f1011a8be0e34703206467ada1e0`, branch `ios-port`.
- [Successful Actions run](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40190646660).
- [Encrypted IPA artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40190646660/artifacts/12598261083).
- Unsigned IPA: **47,125,800 bytes**, containing **11,740 linked lifted game functions**,
  55 additional Korean variants, the real engine/runtime, SDL UIKit entry point,
  MoltenVK framework and private translation capsule.
- IPA SHA-256: `ae7c46810d9e7506c0a601698af2e6d45d50d0ba84bc710888ab2ed203c7f3a1`.
- GitHub artifact ZIP SHA-256: `46b128b2608d223ff722bba9f1ba3aaae4363ee12a09d6e674d45446df847b2f`.

The owner confirms that build 2.47 enters gameplay and displays Korean dynamic
text correctly. The 2.48 device log now shows approximately 300 midpoint/end
pairs per five seconds in several active gameplay windows, with occasional
backpressure and one overload cooldown. The overlay still reported 60 because
its iOS output-count path used an application-only define absent from the runtime
library. Build 2.51 selects that path using the actual iPhoneOS platform macros;
its regression test reports 120 submitted pictures/game 60 and returns to 60 in
menus. This is submission evidence, not physical display-timestamp validation.
The owner confirms camera-dependent wall flicker stops when Smooth Motion is Off.
The vertex-level NDC cutoff is removed to avoid mixing midpoint and endpoint
vertices in a triangle. Korean tab row reversal is removed, its weight is Regular
and its plate height is 32 pixels. The actual CoreText function's upright macOS
preview was reviewed. Final iPhone tab appearance and wall correctness still need
retesting. Imported data and saves are preserved; no ISO reimport is required.

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

The upstream `packaging/appimage/ngb.png` supplies the iPhone AppIcon asset catalog.
iOS adds **iPhone Native** after the existing aspect ratios and after the 1x-10x
render-scale choices, in both main-menu and pause-menu recomp settings. The native
aspect uses the device's real landscape pixel ratio. Native rendering fits the
actual SDL drawable at the selected aspect and uses its exact pixel dimensions
for full-size color/depth targets; intermediate effects and shadows retain their
separate scale paths. Native aspect plus native rendering fills the drawable.
Desktop window-size/display-mode rows are hidden on iOS, and FIFO sync is fixed,
so its V-Sync choice is hidden too. Desktop builds keep their existing choices.

iOS now implements experimental **Smooth Motion**, with retained draw recipes
and separately rasterized intermediate geometry while the guest stays at 60 Hz.
It requires MSAA Off, Smoother Glow Off and a reported display cadence above
100 Hz; 1x is recommended for the initial device test. It does not guarantee
sustained 120 fps. See [120FPS.md](120FPS.md) for implementation, fallback and
unverified limits.

The v1.2 translation has a restricted USA adaptation with isolated Korean code
and data. The original Options → General → Text and Subtitles selector offers
Japanese, English and Korean; quit and reopen to apply. See [KOREAN_PATCH.md](KOREAN_PATCH.md) for validation limits.

On resign-active the iOS bridge now requests guest scheduler pause, freezes the
guest monotonic clock, pauses APU/SDL audio, drains the Vulkan device and blocks
new queue work and swapchain acquisition. The UI event thread keeps its run loop
available for foreground notifications. On activation the gates reopen, virtual
time resumes without the background interval, and audio reprimes. Menu audio
pause state is preserved. This lifecycle path needs an owner-device retest.

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
| Korean build | Pinned patch, all 17 section hashes, 137 variants, iPhoneOS ARM64 compilation/link and encrypted IPA packaging passed |
| Smooth Motion build | Actual fixed/program HLSL compiled to SPIR-V; retained-pose tests and iPhoneOS ARM64 engine link/package passed; device scenes/performance unverified |
| Korean archive/settings fixtures | Actual Foundation AFS/delta tests and Japanese/English/Korean settings persistence passed; not a real-disc/device test |
| VM semantics | Allocation collision, protected writes, protection restoration and 64 MiB backing-file shared aliases passed on macOS and iPhone simulator |
| Scheduler/thread regressions | Existing macOS and Linux regressions passed |
| Lifecycle CPU/time | Loaded-image scheduler stops a noncooperative guest spin loop, freezes its clock and resumes; passed Linux and macOS |
| Lifecycle audio | Actual SDL dummy device rejects queued data while paused and reprimes on resume; passed macOS |
| Touch controller contract | SDL virtual pad button/stick/trigger mapping passed headless Linux checks; not an iPhone UI test |
| Extraction helper | Cloud PowerShell 7.5.4 synthetic disc tests passed; real Windows execution not observed |
| Owner device report | Import and startup work after shared-RAM and loaded-image scheduler fixes; owner reports New Game and working gameplay; background execution/audio and graphics corruption on return were also reported |

[Simulator VM run](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40109319653)
and [runtime/renderer build](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40099391364).
The macOS runner used Xcode 16.4/iPhoneOS 18.5 SDK with a deployment target of iOS 15.
A deployment target is not validation on iOS 27. Windows and desktop macOS full-game
builds were not run here; their default build paths were retained.

The baseline Linux VEH regression previously failed at test_main.c:87 and remains
unresolved. Its post-protection read passes through its own MMIO handler, so that
failure alone does not establish a production VM bug. Do not report all tests as passing.

## Unverified and remaining work

- The owner confirmed the shared-memory fix gets past startup. A subsequent log
  opens movie/font resources and `bgm.afs`, then the game displays its disc-error
  screen. The Apple scheduler used image zero's text range, which belongs to the
  host in LiveContainer. It now identifies its own image with `dladdr`; the
  separate-image spin-loop regression tests both the fix and the old failure.
  The owner subsequently confirmed game startup and gameplay. Extended play,
  measured performance, graphics accuracy and controller hotplug remain unverified.
- The owner's LiveContainer startup log maps 28/28 RAM mirrors and verifies the
  tiled aperture alias. This is stronger evidence than the small-region simulator
  check, but does not establish every allocation or subsequent game state.
- Rendering and audible output have owner-device evidence, but the log reports
  `VK_ERROR_DEVICE_LOST` when submitting from background. The new lifecycle gates
  prevent new GPU submissions and drain before losing foreground permission;
  device pause/resume and screen-lock tests are still required. This does not
  recreate a device that was already lost before the app became inactive.
- Host condition-variable waits still use real-time deadlines. Not every guest
  wait/timer behavior across a long suspension has been established, even though
  guest execution, its monotonic counters and the NV2A virtual clock are paused.
- Smooth Motion replay and sustained 120 fps need physical-device checks.
  Guest timing is not doubled; the renderer adds an intermediate scene.
- Separate arcade executables and external archive extraction are not supported
  by this iOS app.

The real-engine build and owner-reported basic gameplay milestones are reached;
the reliability, lifecycle and performance work above remains open.

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
