# Download and first-device test

This is an experimental real-engine build. The owner reports successful startup,
New Game selection and working gameplay in LiveContainer after a restart following
the first load. Background graphics corruption and continuing audio were also reported.
A successful compile/package is not a gameplay result.

## Download the actual IPA

1. Open the [encrypted game artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40154920349/artifacts/12584258274)
   while signed into GitHub. It is from [Actions run 40154920349](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40154920349).
2. Extract GitHub's outer ZIP. It contains `NinjaGaidenBlack-ios-arm64-unsigned.ipa.7z`.
3. Open the inner archive with a 7z-capable tool such as 7-Zip and enter the same
   password stored in `NGB_IPA_PASSWORD`. Do not upload the password to an issue or chat.
4. Extract `NinjaGaidenBlack-ios-arm64-unsigned.ipa`. Import this file, not the ZIP/7z.

The artifact is retained for seven days. The unsigned IPA is 47,269,743 bytes and
has SHA-256 `379520f7c9b037db652cc9ae963cecfb74368eb9ee7a2b2ddf3ac465a430e542`.
App version: **0.2.0 (2.37)**.
Build source commit: `b0464871bc0392755a5212be8061906cf25cd5ce`.
It contains the real engine, 11,740 original lifted game functions and 137 Korean
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
