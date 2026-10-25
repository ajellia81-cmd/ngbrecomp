# Download and first-device test

This is an experimental real-engine build. The owner reports successful startup,
New Game selection and working gameplay in LiveContainer after a restart following
the first load. Background graphics corruption and continuing audio were also reported.
A successful compile/package is not a gameplay result.

## Download the actual IPA

1. Open the [encrypted game artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40200373376/artifacts/12601999037)
   while signed into GitHub. It is from [Actions run 40200373376](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40200373376).
2. Extract GitHub's outer ZIP. It contains `NinjaGaidenBlack-ios-arm64-unsigned.ipa.7z`.
3. Open the inner archive with a 7z-capable tool such as 7-Zip and enter the same
   password stored in `NGB_IPA_PASSWORD`. Do not upload the password to an issue or chat.
4. Extract `NinjaGaidenBlack-ios-arm64-unsigned.ipa`. Import this file, not the ZIP/7z.

The artifact is retained for seven days. The unsigned IPA is 47,258,912 bytes and
has SHA-256 `6f69f0c84dee1f9d412f532ad1ad39d1ebbaaf708117bc1af610c18f9f183371`.
App version: **0.2.0 (2.55)**.
Build source commit: `e4f37dc34106ad08ebd708b94a71cac4e906d562`.
It contains the real engine, 11,740 original lifted game functions and 55 Korean
variants. It is not NGBBringup. Korean glyph rendering/gameplay still require a device test.
The Korean-enabled build contains the pinned translation capsule; the original
game data must still be imported. No developer certificate/provisioning profile is included.

## LiveContainer

If the host is not installed/configured, follow the
[official LiveContainer installation guide](https://livecontainer.github.io/docs/installation).
It describes the supported host setup and JIT-less certificate/diagnose workflow.
This project did not test or modify that setup and does not establish iOS 27 support.

In a configured LiveContainer, tap **+**, select the extracted IPA, then select
NGB Recomp and launch it. This is the standard import flow described in the
[official LiveContainer README](https://github.com/LiveContainer/LiveContainer).
Use a normal foreground launch for the initial test.

## Game files and paths

The supported build uses the retail USA `default.xbe`, size **4,571,136 bytes**,
FNV-1a **0x3602ba6a6079c8cd**. The uploaded USA En/Ja executable matched this.
A different disc revision has not been established as compatible.

On first launch the app offers:

- **Import ISO:** select the owner's complete `.iso`/`.xiso` through Files. The
  existing in-process extractor copies the game data into the app's sandbox.
- **Import extracted game folder:** select the folder whose immediate contents
  include `default.xbe` and the rest of the extracted disc tree. The entire folder
  is copied; selecting only the small build XBE does not supply playable data.

The resulting layout is relative to the guest app's data container:

```text
Documents/
  game/
    default.xbe
    ...all other extracted game files and directories, preserving the disc tree...
  saves/
  config/
  shaders/
```

Environment mapping: `NGB_GAME_DIR=Documents/game`, `NGB_SAVE_DIR=Documents/saves`,
`NGB_CONFIG_DIR=Documents/config`, `NV2A_SHADER_CACHE=Documents/shaders` (all resolved
as absolute paths at runtime). The host LiveContainer's own Documents folder is
not automatically the guest's folder.

Keep the original complete ISO/data until game import and device startup have
been verified. The 4.57 MB Drive XBE is only a compilation input; retain it for
future builds. The large Drive ISO is no longer required by the build once the
small input is working, but retain at least one complete game-data copy elsewhere.

## Controls and first-run evidence

For a full native screen, select **iPhone Native** at the end of **Aspect Ratio**,
then **iPhone Native** after **10x** in **Render Resolution**, in the main or pause
recomp settings. The full-size rendering targets then use the actual drawable's
content pixel dimensions; `[D3D] iPhone native render target` logs the result.
Other aspect ratios keep their appropriate black bars. This mode is device-test
pending. It does not change game timing. iOS hides window-size, window-mode and
V-Sync selection; sync is fixed FIFO. Windows/macOS choices are retained.

For experimental 120 Hz scene output, set **Display & Audio → Smooth Motion: On**,
**Graphics → MSAA: Off**, and **Smoother Glow: Off**. Start at 1x to assess headroom.
The renderer re-rasterizes an intermediate scene; game updates remain 60 Hz.
Actual sustained 120 fps and image correctness require a device test. See
[120FPS.md](120FPS.md) for unsupported cases and diagnostic log fields.

The separately requested [Korean adaptation](KOREAN_PATCH.md) remains available:
Options → General → Text and Subtitles → Korean, then Quit and reopen. No archive
upload is required; keep the original imported game data. Recomp menus remain English.

Connect a supported controller for SDL's native iOS GameController backend.
Without one, the native touch overlay supplies both sticks, D-pad, face buttons,
White/Black, triggers, stick clicks, Back and Start. Physical-controller detection
hides and detaches the touch controller. The iPhone UI and hotplug behavior have
not been tested on a real phone.

First verify whether the import UI appears, then whether the complete data is
accepted, the engine initializes and a title/game frame is rendered. If launch
fails, retain LiveContainer's error/crash details and the app log if present under
`Documents/saves/log.txt`. Report the device, iOS and LiveContainer versions and
which stage was reached; these distinguish loader, data, VM and renderer failures.
Do not include personal saves or game content in a public issue.

If files cannot be selected, enable the app-specific **Fix File Picker** option
in LiveContainer, then relaunch. The first device report confirmed this resolves
the inactive file selection even after broadening the app's file type filter.

The first startup failed at `CreateFileMapping failed (error 8)`. The sandbox
temporary-file fix subsequently mapped all 28 RAM mirrors on the owner's phone.
The Apple scheduler's separate-image text-range fix subsequently allowed the owner
to start and play the game. Do not delete and reimport data to test an updated IPA.
The New Game loading log shows successful data reads and preparation of files in
the emulated hard disk. The owner reported entering gameplay after restarting;
the exact first-load duration has not been measured.

The latest lifecycle change pauses guest execution/time, audio and GPU submissions
on resign-active, and resumes on activation. It needs a physical-device retest:
after installing the update, fully close any old process, start the game, go Home
for several seconds, and return. Sound and game progression should stop while
inactive, and graphics should remain intact on return. Repeat with screen lock.
If it fails, retain `Documents/saves/log.txt`; `[iOS] inactive` and `[iOS] active`
identify the transitions, and `VK_ERROR_DEVICE_LOST` identifies GPU loss.
GPU recovery after an already lost device, extended suspension semantics and
120 Hz behavior are not established. Keep a copy of saves before longer testing.


## Latest device-report fixes

The owner reported ignored Korean selection, half-speed gameplay with Smooth
Motion, an FPS strip hidden at the screen edge, and desktop-specific exit labels.
The new source fixes these routes; installation on the owner's phone is pending:

- Text selection uses the original Options menu and the actual saved game text
  selector; the separate Recomp selector is removed. Voice selection is retained.
- Midpoint rendering uses isolated targets. A bounded latest-pair presenter runs
  display waits on its own thread, not inside guest Swap. Active GPU source images
  are leased until their copy fences complete; superseded pending pairs are dropped.
  Consecutive slow guest Swap intervals temporarily skip replay to protect game speed.
- FPS uses the intersection of content bounds and UIKit safe-area insets, with padding.
- iOS app-exit labels and confirmations say Quit. They use a process-exit path,
  not just window closure. LiveContainer may close with its guest; the official
  source reviewed did not interpose `_exit`. Actual host-version behavior needs
  a device test. No claim is made that it returns to LiveContainer's app list.
- Long **recomp-added** descriptions are shortened in main/pause settings. Original
  disc-provided Options descriptions are unchanged.


### Cropped title screen report

The owner reported a small/cropped title picture after installing the asynchronous
motion build, preventing access to its settings. The follow-up source serializes
normal-frame and pair-worker publication of the image **and** its dimensions with
the presentation lock. A concurrent production-function test rejects the old
unlocked publication. Replay also preserves the clear state of newly created
depth targets and explicitly ends rendering before the final composite.

Boot movies and 2D/title menus now bypass midpoint replay, even if Smooth Motion
is enabled in the existing settings. No in-game setting change, data reimport or
save deletion is required to install this correction. The owner confirmed that
intro skipping was the persisted Skip Intros=On choice, not a regression. The
proposed boot-input change was withdrawn; intro behavior is unchanged. These
rendering changes need a device retest; compilation alone cannot establish that
the reported image is restored. The separate request to alter the description's first glyph was
withdrawn, and no such glyph/content edit is included.


### Identify the running build and current log

The follow-up phone report still showed the cropped title. Its attached session
log contained the old synchronous `midpoint scenes submitted` signature and no
new asynchronous presenter counters. The owner confirmed copying it immediately
after running. This establishes a log/binary mismatch to investigate, not that the
owner selected an old file: the app previously left saves/log.txt untouched when
LiveContainer already redirected stdout to a regular file. Korean boots can also
use saves-korean/log.txt instead of saves/log.txt.

New builds use short version **0.2.0** and a distinct **2.<Actions run number>**
build version. The executable prints its compiled source revision after log setup.
Packaging verifies that the revision and version in Info.plist occur in the
linked executable. iOS always refreshes its session log, and Documents/build-info.txt
records the executing version, source commit and actual current log path.

Close the running guest and LiveContainer completely, import the newly downloaded
IPA, and select replacement of the existing app to preserve its data. Check that
the selected entry is version 0.2.0 before launching. If multiple entries have the
same name, verify the version rather than launching a previous copy. Do not delete
a data container or reimport the ISO to diagnose this display failure.

The additional renderer correction reserves each draw within the actual **32 MiB
frame slot**, rather than the full 64 MiB two-slot allocation. Otherwise Upload()
can submit/reset between uniforms and geometry, reusing their offsets. Normal and
replay draws now share the same slot capacity, with oversized replay rejected.
Boundary tests reject a 31 MiB position plus a 2 MiB draw; the old 64 MiB capacity
negative control fails. This is a verified resource-lifetime defect, but its role
in the phone's cropped picture remains unconfirmed until the new session is read.
Bounded `[iOS View]` and `[iOS Fit]` records expose guest/target/viewport/scissor and
source/drawable/destination dimensions without a full picture capture.


### Confirmed portrait drawable during a landscape Korean boot

The correct saves-korean session identified build 2.37 and exposed the missing
runtime evidence: Vulkan created a **1320 x 2868** portrait swapchain, while the
configured landscape aspect was 2868/1320. The native target became **1320 x 608**;
`[iOS Fit]` placed it at **(0,1130)** inside that portrait drawable. The owner's
landscape screenshot consequently contained a small/cropped picture. No midpoint
pairs were queued during this title screen, so interpolation was not its cause.

The correction keeps both import and Korean preparation controllers landscape,
sets SDL's orientation hint before those windows appear, and requests the scene's
landscape geometry using public UIKit APIs. SDL's main-thread pump synchronizes
CAMetalLayer.drawableSize with its actual view bounds and scale. It observes the
actual drawable pixel size every pump and invalidates the swapchain on change,
even without SDL_WINDOWEVENT_SIZE_CHANGED or VK_ERROR_OUT_OF_DATE. Resize requests
are exchanged atomically so a concurrent rotation cannot be lost. No numeric
width/height swap is used to pretend that a portrait Metal layer is landscape.
Touch controls are reattached if SDL replaces its root view with the Metal view;
old virtual-controller inputs are released and disconnected first. UIKit pump
objects have a per-call autorelease pool.

The production-function regression simulates 1320x2868 -> 2868x1320 without a
resize event, verifies swapchain invalidation, and checks that unchanged/zero sizes
do not cause repeated rebuilds. Removing the invalidation makes its negative
control fail. The exact UIKit orientation helpers and full application compile
against the iPhoneOS SDK. Physical-device verification remains necessary: the new
session should report a landscape swapchain and full landscape native target;
`[iOS Orientation]`, `[iOS Drawable]` and `[iOS Fit]` identify any remaining mismatch.
The original game data, saves, intro setting and simulation timing are unchanged.


### Missing menu art/text and New Game loading after the movie

The build 2.39 phone session confirmed landscape output at 2868x1320, but reported
black main-menu art, mostly absent Options/Recomp Settings contents, a corrupted
New Game movie and a loading screen that did not advance. Its file reads exposed
an archive-layout mismatch: the patched executable requested rebuilt resource
offsets (for example a 32 KiB shift after the grown demo resource), while the old
on-device patcher appended replacement members and retained the original data at
those positions. The AFS directory itself was valid, so the old directory-based
synthetic checks did not catch the game's extra offset dependency.

The patcher now streams every member in canonical order/alignment, preserving the
header prefix, unchanged member contents, filename metadata and tail padding. It
checks the source member layout and replacement preimage hashes. A state revision
forces regeneration of old Korean overlays and removal of only the disposable
saves-korean/Cache installation; shared UserData/TitleData saves and original game
files are preserved. No manual game-data deletion or ISO reimport is needed.
A native preparation screen runs once on the next Korean boot. Synthetic Foundation
tests compare the complete resulting file against expected canonical bytes and
verify both metadata preservation and save-safe cache migration. Restoration of
phone menus, movie rendering and New Game progression still requires device testing.

Touch controls reuse the existing virtual gamepad and landscape/safe-area layout.
The transparent overlay attaches to SDL's current Metal root view, refreshes on
controller notifications and also polls presence every 250 ms. Connecting a game
controller hides it and releases/disconnects the virtual pad; disconnecting restores
both sticks, D-pad, A/B/X/Y, White/Black, LT/RT, Back/Start and stick-click buttons.
`[iOS Touch]` records attachment, visibility and virtual-input readiness. Actual
multitouch and physical hotplug still need a phone test.

All visible app-exit contexts were audited. The uppercase main-menu list uses
**QUIT**; pause Exit pages, the auxiliary settings screen and native restart alert
use **Quit** alongside their title/sentence-case neighbors. Confirmation boxes use
**Quit?** with Yes/No. Desktop labels and menu-back Exit items retain their original
roles. The application exit path itself is unchanged.

## Build 2.41 follow-up

Replace the existing LiveContainer game app with this IPA; retain its data and
saves. No ISO reimport is required. Dynamic Korean text/subtitle corruption in
2.40 exposed two font-bank continuation jumps that were lifted as host returns.
The corrected merger resumes all caller paths and keeps their stack/register
state. Korean mode also labels the extra pause tab 설정.

The FPS overlay can show output and guest rates separately when motion pairs
are submitted. This does not establish sustained 120 FPS. If text, long New Game
loading or 60-FPS-only output persists, copy the current saves-korean/log.txt
after the problem and include Documents/build-info.txt. For motion diagnosis,
move in actual gameplay with Smooth Motion on for at least ten seconds before
copying. Menus and movies deliberately bypass intermediate-scene replay.

## Build 2.42 follow-up

Replace the existing app and keep its imported files/saves. This build corrects
direct C calls to patched font/resource entries, removes two presenter queue/wait
bottlenecks, and reuses the patch's own 설정 glyphs for the added tab. Japanese
and English retain their original function bodies and Settings label. The code
and whole-engine build checks passed; phone rendering and throughput are not yet
verified. If motion stays limited, the current log now includes separate pair,
drawable-acquisition and final-copy wait timings. The owner reported immediate
gameplay entry on a later 2.41 session; no special loading-speed fix is claimed.

## Build 2.43 startup retest

The owner confirmed the 2.42 startup stall. This build retains original callers
when their Xbox instruction bytes are unchanged, narrowing the Korean selection
to 25 entry gates and 55 variants. Replace the app and retain imported data/saves;
no ISO reimport is required. Regression tests and the whole-engine device build
passed, but phone startup is unverified. If it still freezes, keep the app in the
foreground for thirty seconds, then copy the current log.txt and build-info.txt.
Before a first guest flip, the bounded sampler records the boot-thread stack;
after flips begin it stops. Korean text and sustained 120 FPS remain retest items.

## Build 2.46 DirectSound startup correction

The 2.43 samples identified a DSP command-completion wait at 0x0037DC15. Korean
memory reservations moved the observed command word to 0x86C28810, but the old
fixed probe still addressed 0x86BF8810. This build registers a title spin service
before guest threads start and uses the verified current operand in auto mode.
Only aligned, mapped command-3 words at that loop are acknowledged; unrelated
memory and unknown commands are retained. This keeps the existing DSP passthrough
policy and does not implement DSP effects.

Fully quit both the game and LiveContainer, reopen LiveContainer, and replace the
existing app with the new IPA. Retain its imported data and saves; no ISO reimport
is required. This discards any legacy DSP environment retained in an old host
process. The current log should show `[iOS DSP] mailbox policy: auto` and, when
the effects image is posted, its observed completion address. Native tests and
the complete game build passed; phone startup and rendering remain unverified.
If it still stalls, leave it foreground for thirty seconds and copy its new log
and build-info.txt. The existing bounded startup sampler remains enabled.


## Build 2.47 follow-up DSP completion

The owner's 2.46 build-info and new log confirm source 400eb5b, auto policy and
successful effects-image command-3 completion at 0x86C28810. The three startup
samples instead stop at the parameter-upload wait, 0x0037DC78, which reads
[EAX+0x10]. This follow-up services command 2 at that verified operand as well.
It retains the existing address/alignment checks and explicit-off/manual policy,
and never clears a different command value. This preserves passthrough audio;
it is not DSP effects emulation. No data or save reset is needed.

Native tests cover both command paths, a moved mailbox, unrelated memory,
wrong commands, bad addresses and opt-out. Full iPhoneOS build and startup are
separate checks: a successful package does not establish phone startup recovery,
correct Korean text or sustained 120 FPS. If startup still stalls, leave the
app foreground for thirty seconds and copy its new log and build-info.txt.


## Build 2.48 presentation and Korean tab lettering

The owner confirms that 2.47 enters gameplay and displays Korean dynamic text
correctly. The next device log shows a 120 Hz display but only sparse midpoint
pairs: 27–30 ms per pair, followed by the existing 120-guest-frame overload
cooldown. Thus the 60 FPS report is not established as an overlay-only problem.

The iOS host now selects asynchronous MoltenVK queue encoding before Vulkan is
initialized, keeping Metal encoding out of the application's shared queue lock.
The iOS swapchain requests three images, subject to the driver's maximum, rather
than two. Existing fences still protect midpoint/endpoint source leases; guest
simulation stays at 60 Hz and the overload guard remains enabled. Phase timings
separate presenter-lock, queue-submit and queue-present time from drawable
acquisition and final-copy waits. These are performance changes to validate on
device, not evidence that sustained 120 FPS has already been achieved.

The Korean 설정 tab uses CoreText vector lettering, medium Korean sans weight,
matching slant, antialiasing and the plate's full text-height budget. It replaces
the small Options-sprite crop; the plate's existing light edge and selection glow
remain. The exact original baked-art typeface is not known, so visual matching
still needs an owner-device check. Original game menu artwork is preserved.

Fully quit LiveContainer before replacing the app. Keep imported data and saves;
no ISO reimport or translation reset is needed. For a useful Smooth Motion test,
play an active 3D scene with it enabled, then copy the new log. Static menus and
movies intentionally do not generate geometry-interpolated frames. A new log
should show `[iOS GPU] synchronous queue submissions: 0` and three swap images
when the device supports them. Sustained 120 distinct displayed pictures remains
unverified until a device run establishes it.


## Tab orientation and near-camera geometry (included in 2.51)

The owner reports that 2.48's Korean tab is inverted and mismatched in weight
and size. The unnecessary bitmap-row reversal is removed, the CoreText font is
Regular, and the Korean word's plate-height budget is 32 pixels rather than 38.
The platform job runs the actual production mask function on macOS and emits a
preview without any game assets. Its upright orientation was manually reviewed.
This checks native raster output; the final iPhone plate still needs visual
verification and the original baked-art typeface is not known.

The owner also reports 60 FPS and wall flicker during camera movement. The motion
vertex shader used a per-vertex normalized-screen-distance cutoff. This could
leave some vertices of a triangle at an endpoint while interpolating others,
especially near the camera or viewport edge. The cutoff is removed: finite clip
positions now interpolate in homogeneous space before GPU clipping. Replay still
uses isolated targets and the original 60 Hz guest simulation. Cuts require
whole-frame history reset; this change does not establish complete cut detection.
Actual fixed/program shader compilation and source ownership tests are separate
from physical-device rendering verification.

The font and vertex changes do not establish a performance fix. Sustained 120
FPS remains unresolved. Send the current log after active 3D gameplay, and check
whether the same wall flickers with Smooth Motion Off. Retain game data and saves;
no ISO reimport is required. Fully quit LiveContainer before replacing the IPA.


## Build 2.51 output-counter correction

The owner's fresh 2.48 log confirms three swap images and approximately 300
midpoint/endpoint pairs per five seconds in multiple gameplay windows. Mean
pair service improved to roughly 12–17 ms, with occasional superseded pairs and
an overload cooldown. The owner also confirms that wall flicker stops with
Smooth Motion Off. The vertex-coherence correction and upright, thinner Korean
tab from the preceding changes are included in this IPA.

The remaining 60 FPS overlay was compiled using an application-only define that
is absent in the Xbox runtime library. The production sampler now uses Apple's
iPhoneOS platform macros. A native synthetic-clock test compiles that exact
sampler without the app define, verifies 120 submitted pictures/game 60, checks
menu fallback to 60, retains desktop behavior, and rejects the old branch.

A completed pair count means its presenter copies were submitted and fenced;
it is not a hardware presentation timestamp or proof that every screen refresh
was distinct and visually correct. Physical-device frame pacing, wall flicker
and the final font appearance remain retest items. Keep the original 60 Hz game
simulation and existing overload protection. No ISO reimport or save reset is
needed. Fully quit LiveContainer, replace the IPA, and test active 3D gameplay.


## Build 2.54: image label, haptics recovery and Korean spaces

The owner confirms 120 FPS output/counter operation but reports camera-dependent
polygon-face flicker across the scene. The previous NDC-cutoff removal was not
sufficient. The new interpolation helper uses precise subtract/multiply/add for
multipass EQUAL-depth consistency. CI inspects its actual SPIR-V NoContraction
decorations and rejects the unprotected control. This verifies compiler precision,
not on-device visual correctness. EQUAL-depth draw counts are logged for retesting.

설정 is now a new transparent image generated at the owner's request, fitted
through the original plate/shadow/glow code. No original game pixels are committed.
The actual native mask loader's plate-sized preview was reviewed on macOS. Its
font/style match still needs an owner-device check.

Backbone Pro's first-rumble stall and later loss of vibration prompted a native
controller haptics worker. The guest input path only publishes an atomic latest
command; it never initializes or calls a haptic engine under the input lock.
GameController player indexes retain port routing. Controller-supported localities
use separate handle motors or a default motor. Engines and reusable players are
prepared at zero intensity, with no warmup pulse. Stops, resets and parameter
errors invalidate the engine and retry after one second; disconnected or
backgrounded controllers stop, and expired commands are not replayed. Native
CoreHaptics source compiles for iPhoneOS; synthetic tests cover the production
bridge, latest-command coalescing, expiry, foreground silence and timer wrap.
No real controller was driven by those tests. Actual latency and recovery remain
owner-device retest items. Desktop SDL/XInput paths remain unchanged.

Dynamic Korean text used a full-cell ASCII-space advance, unlike the baked menu
art. The Korean-bank space is now half a cell in width calculation and both draw
continuations. Latin modes and non-space glyphs retain their metrics. A native
fixture rejects the old behavior and verifies matching measure/draw results.
The reported bottom clipping is still unresolved. The log now records bounded
font-cell metrics; original font assets are needed to compare ink bounds.

### Extract only the font assets on Windows

The session cannot execute commands on the owner's desktop. The supplied script
can be run manually without installation, administrator rights or ISO modification.
It reads only the three known paths and directory metadata, rejects existing
output folders and creates Downloads/NGB-font-input/NGB-font-input.zip. It has
no upload or network code. The three paths are Media/font.ttf, Media/font.xpr,
and Media/Fonts/kfont.xpr. Missing files are recorded in its manifest.

Run in ordinary Windows PowerShell:

```powershell
$script = Join-Path $env:TEMP 'Extract-NGBFontInput.ps1'
Invoke-WebRequest -UseBasicParsing 'https://raw.githubusercontent.com/ajellia81-cmd/ngbrecomp/ios-port/ios/Extract-NGBFontInput.ps1' -OutFile $script
powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script
```

The default ISO path is the previously supplied Downloads/Ninja Gaiden Black
(USA) (En,Ja).iso. Use -IsoPath to override it. Upload only the resulting font ZIP
privately in chat, never to the public repository. Keep the original ISO backup.
After upload, the PC need not remain running. The existing Drive ISO is 7.83 GB,
above the connector's 256 MB raw-download limit; its sharing settings were not
changed. Windows PowerShell is checked using synthetic XDVDFS fixtures, not the
owner's ISO or desktop.


## Build 2.55: preserve Korean glyph bottom strokes

The owner provided the font-only ZIP. Its three file hashes match the extraction
manifest and its TrueType file contains valid Hangul outlines. The displayed
Korean lettering also uses patched P8 pages: fjp_bk02.spr, fjp_st15.spr and
fjp_st16.spr. Private inspection found ink on cell row 15 in 715 of 768 cells;
the first two rows of every cell are transparent. No font file is replaced.

The patched sprite groups 0x56D, 0x587 and 0x588 map to these three pages. Their
helper reduced both upper-U and lower-V texture bounds by one texel. The new
adaptation removes only the lower-V reduction, retaining the original vertical
padding/last ink row and keeping horizontal neighbor-bleed protection. The
fixture uses a thin final-row stroke and fractional destination sampling: the
adapted interval preserves it and the previous trim fails. It also checks the
non-target page and unchanged horizontal trim. These are native arithmetic/codegen
checks, not a physical GPU rendering result. The actual pinned private helper
must match exactly or generation fails. Phone verification is still required.

The log's former `cell 16x2` line was not a two-pixel-high glyph. Its second field
came from the character-byte-count slot. The read-only diagnostic now reads the
actual height from the font table. Existing line spacing, non-space glyph advance,
original Latin/Japanese code and on-disk assets remain unchanged. No data/cache
reset or ISO reimport is needed. Fully quit LiveContainer before replacement.

The previously reported whole-scene polygon flicker and controller haptic
behavior remain device retest items. This log had Smooth Motion disabled; it does
not independently validate the prior interpolation-precision change.


## Build 2.56: motion input interpolation and remaining typography

The previous clip-output interpolation is replaced by retained vertex-input and
matrix interpolation before the original program, following the VF3 scene
reference. The Korean-only 설정 image height is reduced from 32 to 29 plate units
and its source coverage is slightly thinned. No original tab images are changed.
Korean full-width Shift-JIS spaces (0x8140), which bypassed the previous ASCII
space repair, now consume both bytes and take the same half-cell advance in
measurement and both draw continuations. Other font modes retain their parser.

The owner's 2.55 report says glyph-bottom clipping improved but remains slightly
visible. This build retains the 2.55 UV repair; no further unsupported change to
the font-cell geometry is made. The final glyph ink may also be constrained by
the translation's baked bitmap. Its exact remaining cause is unresolved. Phone
checks are still needed for spacing, image weight and camera-motion flicker.
