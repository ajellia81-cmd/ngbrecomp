# Private disc input on a GitHub-hosted build runner

The `iOS production engine preflight` workflow builds the real game target and
packages an IPA only after its engine/entry-point checks pass. Packaging is not
proof of device startup or gameplay.
It uses a standard `macos-15` GitHub-hosted runner. It does not use the owner's PC.
Standard hosted runners are free for this public repository; do not select a paid
larger runner or change account billing for this task.

## One-time input configuration

Open this repository's **Settings → Secrets and variables → Actions → New
repository secret**. Add `NGB_DISC_URL` with the owner's existing Google Drive
ISO or extracted `default.xbe` link as its value. Do not add it to source code, repository variables,
workflow_dispatch inputs, an issue or a commit message. Optionally add
`NGB_DISC_SHA256` if the owner has independently computed a SHA-256 of the ISO.
The workflow does not alter Drive sharing or account permissions.

The input must be downloadable by link. Google can impose download quotas or reject
automated downloads; successful metadata access does not establish download success.
For the minimal 4,571,136-byte input, see [SMALL_INPUT.md](SMALL_INPUT.md).

After saving the secret, re-run the failed private-recompilation job. The initial
push runs automatically on `ios-port`. A missing secret intentionally fails the
job rather than reporting engine preparation as successful. Manual dispatch is
also declared, but GitHub normally requires a workflow on the default branch
before displaying its Run workflow control; do not modify master just for this.

## What the runner does

1. Compile the actual VM compatibility and CPU scheduler sources for iPhoneOS in
   an independent job with no disc input.
2. Download the owner's ISO or raw XBE to runner temporary storage using gdown. The chat
   Drive connector's 256 MiB limit is not involved in this build job.
3. Compute the input SHA-256 and compare it if an independent expected hash exists.
   Extract only `default.xbe`, validating the size and FNV-1a fingerprint from
   `src/install.c`. The full disc is then removed to free space.
4. Reuse `tools/pipeline.sh` to generate the real private game sources. Compile
   `src/fibers.c` with their real headers. The original CMake game target builds
   all generated game C, handwritten engine and renderer sources for iPhoneOS.
5. Link through SDL UIKit with the bundled MoltenVK framework. Check the bundle
   has an iPhoneOS executable, SDL entry and at least 1,000 lifted game functions.
   Remove signatures for LiveContainer import, include license notices and ZIP
   the Payload directory. This is not a diagnostic-app fallback.
6. If `NGB_IPA_PASSWORD` is configured, encrypt the IPA as a 7z archive with
   encrypted filenames and upload only that archive. Use at least 16 characters
   and keep the password out of chat/source/logs. Without the secret, no IPA is
   uploaded. Extract the downloaded artifact ZIP, then open the inner 7z using
   the same password to obtain the unsigned IPA.
7. Delete private inputs, game objects, generated C, raw logs and local IPA copies.
   No raw game data/source or unencrypted IPA is published. Encrypted delivery
   artifacts expire after 7 days. Public logs contain sanitized status and hashes.

This is an ordinary source-generation and compilation job on a separate hosted
build machine, not a tunnel or a transfer proxy into the chat workspace. This
chat environment's domain policy is unchanged. The job does not send the ISO or
generated code back to the assistant environment.

## Remaining gates

Actual Drive download, source generation and iPhoneOS compilation must be checked
in the run results; workflow installation alone is not success. Full application
compilation/linking, renderer/input/lifecycle verification, private IPA delivery
and LiveContainer device execution remain separate requirements in `README.md`.

The separate `iOS full runtime build` workflow builds open-source SDL2, GLib and
glslang for iPhoneOS via a pinned vcpkg revision, then compiles the existing Xbox
runtime, including its NV2A Vulkan path. Its isolated dependency cache contains
only open-source libraries, never game code. It does not package MoltenVK or an
IPA and does not consume the owner's disc.

The complete extracted disc directory is still needed at runtime. Extracting
only the XBE here does not supply runtime data. Each runner is disposable, so do
not delete the only copy of the disc or revoke its link while future build runs
still need it. A successful preflight does not create a permanent backup.

## Confirmed execution

[Run 40096238764](https://github.com/ajellia81-cmd/ngbrecomp/actions/runs/40096238764)
downloaded all 7,825,162,240 bytes, accepted the retail executable fingerprint,
generated 48 game C translation units, and compiled the real production fibers
for iPhoneOS ARM64. Its independent job also linked the production scheduler
against device and simulator libraries and passed the existing macOS scheduler
and thread-suspension regressions. This establishes the input/build route; it
does not establish whole-engine compilation or gameplay. The later workflow
adds compilation of all 48 generated units, whose result must be checked
separately.

The computed ISO SHA-256 was
`4fd09d95ebf86dde18fca6ea038a76a376a15cdf3980c9cffe6f2571ead827ff`.
No independent expected hash was supplied for that run. This value is an
observed digest, not a claim of matching an external preservation database.
