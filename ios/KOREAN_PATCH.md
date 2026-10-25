# Korean v1.2 integration

Source: [slimshadykor-cmd/korean-patches, ngb-v1.2](https://github.com/slimshadykor-cmd/korean-patches/releases/tag/ngb-v1.2).
The Windows installer is not executed. CI pins the ZIP SHA-256 to
`c7df8ebc34b75077e75647f89606ea31eef5860d21bf3416941b0c3e5bba19d2`.

## Compatibility and isolation

The release installer supports Japan/Korea and rejects USA. This port implements
a separate, restricted adaptation for the exact owner-provided USA executable.
Applying the release plan to a private copy produces all 17 expected executable
section hashes. This proves section compatibility for that input, not that the
release installer supports USA or that gameplay/fonts are correct on iOS.

CI retains the original generated functions and adds reachable Korean variants,
including the added font helpers. Dispatch selects variants only during a Korean
boot. Japanese and English retain the original executable and data. Missing new
continuations and newly untranslated instructions stop generation; they are not
replaced by empty stubs. SSE2 packed moves preserve all 128 XMM bits. Two pinned
flags-save wrappers reserve their unobserved guest-stack flags word while caller
conditions remain in caller-local snapshots; this is not a general architectural
EFLAGS implementation. Manual font
hooks retain the host instrumentation around their translated bodies.

The IPA contains a private patch capsule. It contains game-derived data, so the
IPA is delivered as a password-encrypted artifact rather than a public release.
No modified executable, game archive or generated game C is committed.

## Use

Keep the existing imported USA game data. No large archive upload is required.
Open the original **Options → General → Text and Subtitles** selector. It offers
Japanese, English and Korean. The duplicate Recomp Settings Startup selector has
been removed. Speech Language remains the separate original voice setting.
A native restart notice offers Quit or Later; completely reopen after changing
text language. Existing imported game files and saves are retained.

The previous integration only supplied an EEPROM default. The title then loaded
its saved Options text value, which could select English/Japanese instead. The
new private codegen boundary routes reads of that actual game text selector and
its reversed text-table column to the selected boot. It never writes a third
index into the two-language Xbox tables. After the original Options selector is
used, its host value persists across overlay restarts. Older ordinary Japanese/
English Options values are retained until the selector is explicitly changed.

On a Korean boot, the app checks every replaced archive member against the patch
preimage hash, builds an isolated `Documents/korean-game` overlay, and verifies
replacement hashes. Archives are rebuilt in member order with the release's
2048-byte alignment, canonical offsets, complete filename metadata and tail
padding. An append-only AFS directory is insufficient because the patched game
also uses rebuilt resource offsets. Member order, sizes and data prefix are checked
against the pinned layout before output. Original files remain unchanged. Unchanged files are linked
from the original import. A transactional pending directory avoids accepting a
partial patch. A mismatch reports an error and offers continuation in English.

Korean uses `Documents/saves-korean` for a separate installed-game cache so the
English cache cannot hide translated resources. UserData and TitleData link to the
original save directories. A `canonical-afs-v2` state marker upgrades previous
append-based overlays once and clears only `saves-korean/Cache`, whose installed
archives contain stale offsets. UserData, TitleData and the original import are
preserved. The first launch after this upgrade prepares Korean archives again.
Back up saves before testing this experimental build.
The translated game text does not translate Recomp Settings or the import UI.

## Validation limits

All 17 patched executable section hashes match the pinned release plan. Native
Foundation tests exercise AFS member replacement, alignment/reindexing, preservation
of original data, preimage rejection and executable patch validation with synthetic
fixtures, including byte-for-byte canonical output after a grown member shifts
later entries, non-size metadata preservation, mismatched layout rejection and
cache removal with shared-save links retained. CI also checks persistence of all three text-language choices.
Actual USA archive compatibility, Korean glyph rendering, menu layout, gameplay,
original-language regression and save compatibility require owner-device testing.

## Font continuation correction

The pinned patch uses two `PUSH continuation; RET` jumps in its font-bank helpers.
Treating these as ordinary host returns skipped the original glyph-bank setup
and selection continuations, including their stack cleanup. The private merger
now rejoins all three original caller branches, preserves the selected EBP value,
and rejects any unexpected patch shape. No new game-function body is published.
Synthetic CI tests cover the branch adaptation. An isolated local execution of
the actual private helpers checked stack/register preservation, seven font-bank
ranges and the three dedicated Korean banks; removing register publication failed
the negative control. These checks do not establish correct on-device glyph output.

On iOS Korean boots only, the extra pause-menu Settings tab uses the system
Hangul font to display 설정. Japanese and English retain Settings. The rest of
the recomp settings are not translated.
