# Ninja Gaiden Black iOS port

An experimental unsigned **real-engine IPA** has been built. The owner reports
successful game startup, New Game selection and working gameplay in LiveContainer
after restarting following the first load. Long-term reliability, performance and
background resume still require testing. The older NGBBringup diagnostic app is
a separate target.

- App version: **0.2.0 (2.59)**; compiled revision and active log path are recorded in `Documents/build-info.txt`.
- Build source: `bd290afebabfaffdcc3d283e784e2d4c043e9a0e`, branch `ios-port`.
- [Successful Actions run](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40250377734).
- [Encrypted IPA artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40250377734/artifacts/12620751067).
- Unsigned IPA: **47,263,460 bytes**, containing **11,740 linked lifted game functions**,
  55 additional Korean variants, the real engine/runtime, SDL UIKit entry point,
  MoltenVK framework and private translation capsule.
- IPA SHA-256: `542703ce630dda67776a732c9df4206bb06a559ffef2e5cf503386eec2310af5`.
- GitHub artifact ZIP SHA-256: `965be6b85b322f668a5dda054371e85a9544aa3ef6a796bf605d4cf57037b510`.

The owner confirms gameplay, Korean text and 120 FPS output/counter operation.
Build 2.59 retains 2.57's frame-wide repeated-mesh pairing and adds a
whole-draw input-continuity check. Singleton candidate groups no longer bypass
identity plausibility: the old source blended a previous lamp at 10 with a
different visible lamp at 20 into a false middle glow at 15. A native negative
control reproduces this; the new guard retains the current endpoint for the
large discontinuity and still interpolates normal small motion. The guard uses
a conservative 25% relative component limit, not VF3's world-space camera/model
metric. It can also skip legitimate rapid motion and requires phone validation.

Only constants referenced by the active vertex program influence pose distance
and continuity. Relative addressing conservatively marks the full bank; fixed
function uses its matrices. Native direct/relative/unused-constant and normal-
motion tests pass, and matching telemetry now accumulates over five seconds.
The owner confirms 2.57 ran on the phone and that flicker decreased but persists.
Physical rendering and performance of this new source remain a retest item.

The owner has completed the Korean glyph-bottom task after pixel comparison
against the original translation bitmap, and accepted the spacing and 설정 tab
image. These font/UI sources are unchanged. Controller haptics recovery still
needs device feedback. No original fonts, bitmaps or game code are committed.
Imported data and shared saves are preserved.

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

## Public validation results

The linked Actions run passed the platform checks and packaged the real-engine
IPA. Native motion-history tests check reordered instances, singleton replacement,
constant-use masks and normal-motion acceptance. These checks do not establish
that the remaining iPhone rendering artifact is resolved. See [120FPS.md](120FPS.md)
for the current interpolation limits and [INSTALL.md](INSTALL.md) for installation.

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

## Game data

Use your own game data. Game executables, font files, generated game sources and
input fingerprints are not provided in this guide. Preserve your imported data
and saves when replacing the application.
