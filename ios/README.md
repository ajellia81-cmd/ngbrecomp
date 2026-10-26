# Ninja Gaiden Black iOS port

An experimental unsigned **real-engine IPA** has been built. The owner reports
successful game startup, New Game selection and working gameplay in LiveContainer
after restarting following the first load. Long-term reliability, performance and
background resume still require testing. The older NGBBringup diagnostic app is
a separate target.

- App version: **0.2.0 (2.61)**; compiled revision and active log path are recorded in `Documents/build-info.txt`.
- Build source: `56b3a88af07d3f0e9b66b1dfb083403dff9e64cb`, branch `ios-port`.
- [Successful Actions run](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40262850366).
- [Encrypted IPA artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40262850366/artifacts/12625448486).
- Unsigned IPA: **47,310,901 bytes**, containing **11,740 linked lifted game functions**,
  55 additional Korean variants, the real engine/runtime, SDL UIKit entry point,
  MoltenVK framework and private translation capsule.
- IPA SHA-256: `2b23f43a6c899535babb2cbe0b84b4a9491d2896dc98478aae87302bb2295486`.
- GitHub artifact ZIP SHA-256: `af19a55cef04dd1eb15445304f626c7d138f790af6d9ee54e5e65e9fa9191490`.

The owner confirmed gameplay, Korean text and 120 FPS output in earlier builds.
Build 2.61 replaces the previous constant-bank matcher with the unchanged VF3
scene interpolation implementation pinned at `a8473a99091e6bb207c85d6d247eece8c4866b39`.
NGB now supplies owned geometry, model-to-view matrices and viewports; the VF3
result drives a replay-only vertex buffer shared across material/depth passes.
Rigid transforms are proved algebraically and unchanged meshes are retained.
The original evaluator handles nonlinear/skinned position programs. The full
NGB renderer and all of VF3's material animation behavior are not identical.

The pinned-code identity check, native scene/ownership tests, SPIR-V shader
compilation, iPhoneOS full engine link and encrypted IPA packaging pass. Physical
rendering, sustained performance and the reported flicker still require a phone
retest. See [120FPS.md](120FPS.md) for the exact shared code and adapter limits.

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
IPA. Native tests exercise scene extraction, repeated instances, shared depth
passes, relative bone addressing, rigid/nonlinear separation and history reset.
The separate runtime compile is also green after supplying its standalone UIKit
build metadata. These checks do not establish that the reported iPhone rendering
artifact is resolved. See [120FPS.md](120FPS.md) and [INSTALL.md](INSTALL.md).

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
