# Regional background flicker on converted iOS

Baseline: 3.1.17, 5bccc8b7ff460667ff12e6290b63902952ff87e7.
The owner reproduced cave-wall flicker with Smooth Motion both On and Off.
The Off session has no midpoint replay and the guest rate stays around 60 Hz.
This rules out needing intermediate frames for the symptom; it does not yet
identify the original-frame rendering defect.

Observed: the device selects D32 float depth because D24 is not supported.
The cache warmed 490 shaders and 2021 pipelines in 0.2 seconds. There is no
per-draw missing-pipeline report in that session, so warm-up alone cannot prove
that every visible draw was ready. The original renderer already documents
float-depth multipass flicker on other drivers; that is a hypothesis here,
not a confirmed cause. No device draw capture is available.

This diagnostic build enables only the existing NGB_D3D_HITCHLOG and
NGB_D3D_STATS reports, inside converted iOS startup. HITCH names deferred
modules/pipelines and counts draws omitted during compilation. The per-second
D3D summary counts submitted, unassembled, targetless and pipeline-less draws.
Logs go to the already existing macos-on-ios-startup.log. No shader or image
dumps are enabled and no private game code or device logs are published.

Shader generation, numerical precision, shader preparation/cache policy,
render target formats, VF3 interpolation and presentation are unchanged.
There is deliberately no speculative depth bias, forced synchronous compile,
loading acceleration or interpolation workaround in this diagnostic build.

Reproduction: keep Smooth Motion Off, visit the affected area and rotate the
camera for 20-30 seconds. Export the startup log before reopening the app
(which replaces that log). Correlate HITCH/deferred events and D3D nopipe
counts with the flicker. If those counts stay zero, inspect the actual depth
passes/position invariance rather than altering shader-loading policy.

## Targeted upstream correction found during diagnosis

Upstream commit: https://github.com/gamboozino/ngbrecomp/commit/7caa75a8199300c85a0117c90885c32230663540
Issue #106: a shadow receiver redraws a wall through a different vertex path.
At high rendering scales its depth can disagree with the base surface, causing
visible breakup/flicker. iPhone Native on the reported device is about 4.48x.
This matches a known upstream condition, but identity with the device symptom
still needs the owner's visual test.

The backport keeps the upstream receiver predicate: triangle draws sampling a
shadow render target, testing depth, and not writing depth. It adds -2 depth
steps and -0.5 times at least 1x rendering scale to the game's existing bias.
Only converted iOS enables this predicate. Normal colour, depth-writing and
non-shadow passes retain their original depth state. Shader programs and the
VF3 common interpolation code are unchanged. The existing has_ds field carries
the dynamic-bias bit without changing pipeline key layout. Until a new biased
pipeline is ready, the original cached pipeline draws the surface; there is no
additional synchronous compile, prewarm, gate or loading-policy change.

NGB's replay adapter records and reapplies the same dynamic bias to midpoint
draws. The correction therefore works with Smooth Motion both Off and On.
The log reports corrected receiver draws and temporary original-pipeline
fallback counts. Existing HITCH/frame reports remain enabled for comparison.
No other upstream rendering/threading/stale-target changes are imported.
