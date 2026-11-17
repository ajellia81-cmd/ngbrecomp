# iPhone port of upstream 0.9.72

Base: `gamboozino/ngbrecomp` tag `v0.9.72`, commit
`2606383704e2675b687fec89fb3dfa965b068d46`.

Build the macOS ARM64 engine with `-DNGB_MACOS_ON_IOS=ON`, then convert
that app with the pinned macos-on-ios compatibility layer. Do not build
the engine with the iPhoneOS SDK. The workflow builds a compact Release
IPA and delivers only an encrypted archive.

## Carried over

- Korean code variants, font clipping, spacing, punctuation, paragraph and
  sprite alignment, approved Settings art and movie subtitle handling.
- Native iPhone resolution/aspect, safe-area FPS placement, touch controls,
  controllers, haptic preparation/recovery and foreground recovery.
- ISO import, save/config paths, and Korean asset/save link repair after
  the iOS container moves.
- Ninja Gaiden Black display name and approved icon.

## Kept from the new upstream

All upstream features, including official **Frame Interpolation**, graphics,
shader compilation/cache/prewarm, loading, audio and menus. Interpolation's
Off/Full Rate/Half Rate/2x choices and its default are the upstream choices.
The iOS display bridge requests the panel's maximum refresh rate; interpolation
and picture pacing remain the upstream renderer/presenter's responsibility.

Only `quick_menu.inc` extends screen-space text/FPS layout. The spark/glow
aspect tables add the iPhone native aspect without changing existing modes.
The vblank adapter discards deadlines accumulated while iOS suspends the app;
it does not accelerate loading or change normal game cadence.

## Excluded

The former iOS VF3 Smooth Motion adapter, water/interpolation experiments,
custom universal shaders, patched MoltenVK, cache/loader acceleration and
user-captured shader recipes are not imported. Added port diagnostic writers,
samplers, overlays and dump features are excluded. Original upstream logging
and F7 remain available.

## Settings

Upstream `text_language` retains Auto/English/Japanese. The iOS Korean selector
uses `ios_text_language` (0 Japanese, 1 English, 2 Korean), with the original
Options selector taking ownership on change. Legacy numeric language settings
migrate to that field. Upstream Audio Output is still setting 25, official
Frame Interpolation 45; the internal Korean selector uses 46.

`migration.json` records the carry-over list and exact base. The baseline test
checks that original rendering, official interpolation and compilation/loading
sources remain upstream-identical outside the explicit display adapters.
Private game files, generated C, device logs and credentials must never enter
source control or plain artifacts.

The small `ui-adapters.patch` records only the menu changes. CI applies it to
the existing public upstream files before checking and building; complete
manual menu implementations are not re-uploaded by this port.
