# Ninja Gaiden Black iOS port status

This branch is experimental. **There is no verified playable iOS IPA.**
The existing NGBBringup app is a UIKit diagnostic harness, not the game.
Do not commit game data or generated recompilation source files.

## Source audit (2026-10-24)

Audited base: `fd3b1e569b3c8da5f697f15f69e86f13a7df33b8`.
Compared with `5fc57d84d617aa7f360cf0a5ab707dc9334f4b95`, the branch
adds only the six iOS bring-up files. It does not yet change the production
runtime, game executable target, or renderer for iOS.

Reuse the existing Apple implementation before adding another one:

- `src/fibers.c` already saves/restores Apple ARM64 host contexts, including
  x19-x30, d8-d15, FPCR and FPSR. The separate assembly in
  `ios/bringup/fiber_smoke.c` checks compilation only; it does not exercise
  production fiber creation, guest state, switching or resumption.
- `xboxrecomp/src/platform/win32_compat.c` already implements fixed Darwin
  allocation with `mach_vm_map`, and Mach-backed memory helpers also exist
  in `src/port.h`.
- `xboxrecomp/src/platform/vcpu.c` has an Apple signal-preemption path.
  ARM64 MMIO decoding and Apple signal contexts exist in the runtime.

These are existing desktop Apple paths, not evidence of iOS execution.

## Remaining implementation and validation gates

1. Add an iOS application target that actually links the game object code,
   runtime and renderer. The current IPA workflow links only `main.m` and
   `fiber_smoke.c`.
2. Validate production fiber switching and signal preemption on iPhoneOS.
   Preserve the existing guest scheduler and timing when adding application
   suspension/resumption. A compile-only assembly check cannot establish this.
3. Validate VM allocation in the actual LiveContainer process. The runtime
   attempts a contiguous `0x100010000`-byte virtual reservation in
   `xbox_memory_layout.c`. This is virtual address space, not that amount of
   resident RAM. iOS address-space limits, page protections and signal/MMIO
   recovery require device tests; do not assume macOS behavior or entitlements.
4. Build and package SDL2, MoltenVK and glslang for the device SDK; route
   presentation through an iOS Metal-backed surface. Existing Vulkan
   portability-enumeration code is useful but insufficient. The D3D backend
   requests Vulkan 1.3, dynamic rendering, synchronization2 and push descriptors.
   The NV2A renderer additionally requests geometry shaders in
   `xemu/pgraph/vk/instance.c`. Query capabilities and implement a supported
   path before claiming MoltenVK compatibility; linking MoltenVK alone is
   not a renderer port.
5. Replace desktop installer dialogs and subprocess/re-execution paths
   (`fork`, `execvp`, `popen`, `execv` in `src/install.c`) with sandboxed
   import and in-process startup. Wire application lifecycle and audio-session
   interruptions to safe engine pause/resume.
6. Feed GameController state into the actual Xbox input backend and add
   multitouch controls when no controller is connected. Merely counting
   connected controllers in the harness is not game input support.
7. Lay out controls using runtime safe-area insets and view bounds, without
   hard-coded device dimensions. Negotiate display refresh independently of
   guest simulation; 120 Hz presentation must not double game ticks.
8. Perform a complete device link, package and inspect the executable, then
   verify engine initialization, title screen and gameplay under LiveContainer.
   No iPhone 18 Pro Max / iOS 27 behavior is verified by this audit.

## Private build inputs

The game source build requires `src/recomp/gen/*.c` and its generated headers.
These are produced by `tools/pipeline.sh` from the owner's disc and must remain
private. The production CMake configuration fails when this tree is empty.
Do not substitute stub functions to make an IPA link.

The installer currently identifies the retail USA NTSC `default.xbe` by
size 4,571,136 bytes and FNV-1a value `0x3602ba6a6079c8cd`
(`src/install.c`). Preserve the complete extracted disc directory tree,
not only the executable. Other disc revisions are not established as supported.

A safe build arrangement, to implement once a working iOS target exists:

- Use an owner-controlled private cloud macOS runner with Xcode and the
  iPhoneOS SDK. Supply the owner's disc through a private mounted directory
  outside the repository. Never put a disc URL, credentials or game content
  in a public workflow input, log, cache or artifact.
- Use a disposable checkout of the exact `ios-port` commit. Run the existing
  pipeline against the private input; keep `game_files/`, generated C and
  analysis output in the private runner workspace.
- Build the real engine target with the device toolchain. Publish no diagnostic
  fallback when generation, compile, link or packaging fails.
- Keep generated code out of git. Treat an IPA containing compiled lifted game
  code as private too: distribute it through an access-controlled destination
  unless the owner has separately established permission for public release.
- Delete temporary inputs/output on completion; avoid a persistent shared cache
  containing game-derived material.

Proposed runtime import layout (not implemented): copy the extracted disc tree
to `Documents/game/` inside the guest app's LiveContainer data container and
set `NGB_GAME_DIR` to that absolute path before engine startup. Set
`NGB_SAVE_DIR` to a separate writable sandbox directory. LiveContainer's
guest container must be resolved at runtime; the enclosing host app's Documents
directory is not automatically the game's directory. There is currently no
implemented iOS importer or verified game-data placement procedure.

## Verification in the available cloud environment

The provided checkout was Linux x86-64 at `5fc57d8`. The GitHub connector
confirmed that all production runtime/test sources are unchanged at the audited
iOS head. A fresh GCC 14.2 build and CTest run gave:

| Check | Result |
| --- | --- |
| POSIX virtual CPU regression | Passed, 1/1 (10.00 s) |
| POSIX thread suspension regression | Passed, 1/1 (0.25 s) |
| POSIX VEH regression | Compiled; failed at test_main.c:87 |
| iPhoneOS compile/link | Not run: no Xcode or iPhoneOS SDK |
| Actual game build | Not run: no game generated source or owner disc data |
| LiveContainer / device execution | Not run |

The VEH failure checks `s_steps == 1 && s_page[0x108] == 0xAB` after
re-protecting the page. The subsequent read is itself subject to the test's MMIO
handler, so this result does not by itself establish a production runtime bug.
It remains unresolved; do not describe the regression suite as passing.

Shell GitHub access also failed (proxy connection failure; CLI reported invalid
authentication). The GitHub connector could read the branch and write this
documentation, but exposes no workflow-dispatch operation. Its commit workflow
lookup filters to PR-triggered runs: an empty response does not establish that
push/dispatch runs do not exist.

No user desktop was accessed. No game data was uploaded. No IPA was created.
The bring-up workflows and their existing artifacts are not completion evidence
for a playable port.
