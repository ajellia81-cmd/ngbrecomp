# Download and first-device test

This is an experimental real-engine build. The owner reports successful startup,
New Game selection and working gameplay in LiveContainer after a restart following
the first load. Background graphics corruption and continuing audio were also reported.
A successful compile/package is not a gameplay result.

## Download the actual IPA

1. Open the [encrypted game artifact](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40111477099/artifacts/12566663991)
   while signed into GitHub. It is from [Actions run 40111477099](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40111477099).
2. Extract GitHub's outer ZIP. It contains `NinjaGaidenBlack-ios-arm64-unsigned.ipa.7z`.
3. Open the inner archive with a 7z-capable tool such as 7-Zip and enter the same
   password stored in `NGB_IPA_PASSWORD`. Do not upload the password to an issue or chat.
4. Extract `NinjaGaidenBlack-ios-arm64-unsigned.ipa`. Import this file, not the ZIP/7z.

The artifact is retained for seven days. The unsigned IPA is 14,239,616 bytes and
has SHA-256 `7c2b3859c05bcb25e507a6a42b9a13b484727b31fb628c4340250c1703861a9e`.
Build source commit: `fd4617d1ef582f4ee255e76fbeb7adb0c186d856`.
It contains the real engine and 11,740 lifted game functions, not NGBBringup.
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

The display-link-only 120 Hz request has been removed. This build still uses
the original 60 Hz guest clock and does not implement true 120 fps gameplay. Menus remain English: Korean progress
reports were requested, not interface translation. The separately requested game
translation uses an [isolated USA adaptation](KOREAN_PATCH.md). Select Korean in
Startup → Text Language, then fully close and reopen the app. No archive upload
is required; keep the original imported game data.

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
