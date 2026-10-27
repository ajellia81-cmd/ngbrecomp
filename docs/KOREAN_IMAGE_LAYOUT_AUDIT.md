# Korean image placement audit

The pinned Korean patch contains 119 sprite packages with 958 sprite definitions.
This includes 100 map images, three font banks and 16 multi-sprite packages used
by menus, loading/chapter screens, the HUD, result screens and other UI. All 119
packages were decoded and their sprite crops inspected; the audit is not limited
to screens visited in a single device session.

The other patched members (267 story scripts, 30 real-time movie scripts, four
message tables and a font mapping table) are structured data, not sprite packages.
Their text is drawn by the font path; changing their whitespace or recentering
entire paragraphs is not an image placement correction.

## Findings and corrections

| Area | Cause | Correction |
| --- | --- | --- |
| Options, six left category buttons | Lettering centered in a 160-pixel atlas cell but the original drawer places that cell left of the button center | Move the existing label pixels 22 console pixels right inside each cell |
| Save Game / Load Game headings | Original cropped sampling window retained although Korean lettering is centered in the full texture | Move each sampling window without changing its extent |
| Mission heading in battle selection | Same cropped-window mismatch as the Main Menu Missions heading | Move the sampling window |
| Mission Selection heading | Korean ink center 256.5 in a 512-pixel texture; original window spans only 410 pixels | Move the sampling window |
| Arcade heading | Korean ink centered in a 512-pixel texture; original window spans 480 pixels | Move the sampling window |
| Both Game Over packages | Sharp Korean lettering center is 145.5 in a 256-pixel quad; the separate blurred effect is already centered | Move only the sharp lettering 18 pixels left |
| Main Menu / Missions headings | Previously corrected cropped-window mismatch | Keep the existing correction |

The Options measurements came from the original, successful sprite submissions
on device: the label quad starts at x=58.5 and is 160 pixels wide. The button's
inner quad spans x=56.5..263.5, center x=160. Thus an ink center of 80 within the
label appears at x=138.5, 21.5 pixels left of the button center. Integer texel
translation preserves the source anti-aliasing and places all six rows within
half a console pixel of the button center. The correction also follows the
original slide-in animation because it changes the image content, not an
absolute screen coordinate.

Map labels belong to locations on their maps. Loading/chapter headings, the
translated result/rank images, item/status labels, icon-and-word composites,
in-game tab plates and decorative artwork were also inspected. Intentionally
left-aligned labels, numbers using their original glyph advances, logos and
composite art are not automatically centered according to their bounding boxes.
The chapter-story paragraphs are font-rendered left-aligned text, not heading
images. The approved Settings tab asset and its source mask remain byte-identical.

## Validation and limits

`tools/audit_korean_images.py` inventories every sprite, including non-menu
packages. The reviewed center targets cover 113 heading/text quads and six
Options button labels. Before corrections the original patch has 14 offsets
larger than two console pixels (including the two already fixed headings).
After the complete preparation there are zero such outliers. Small optical
bounds differences in blurred effects are retained.

The real private patch was prepared through `ios/prepare_korean.py`: every
executable section still matches its pinned fingerprint. Pixel histograms verify
that every nontransparent pixel, including alpha-1 shadows, survives the Options
and Game Over moves. A move that would clip even one such pixel is rejected.
Unrelated members, palettes, sprite sizes, other-language images and the approved
Settings lettering are unchanged. Sample-window moves preserve the original
width and height, so they do not stretch or resize the text.

Synthetic regressions in `ios/korean_sprite_layout_test.py` check original
window guards, exact pixel preservation, clipping rejection, and replay the
measured Options coordinates at 4:3, 16:9, 16:10 and 21:9 viewport scales. These
are asset and coordinate checks, not a claim that every game stage was played on
a device. The updated IPA still needs visual confirmation on device.

To repeat the asset audit with a privately prepared capsule (numpy required;
Pillow only for optional contact sheets):

```sh
python tools/audit_korean_images.py \
  --members /private/KoreanPatch/members \
  --report /tmp/korean-image-layout.json
python ios/korean_sprite_layout_test.py
```

Use `--previews /tmp/korean-image-previews` to inspect every crop in readable
pages. Keep private assets, reports, screenshots and previews out of Git and
out of the IPA. The audit tool is a build-time investigation tool; it adds no
runtime recording or scanning to the game.

## Follow-up: font-rendered paragraphs and punctuation

The image audit above did not reposition font-rendered story paragraphs. The
loading story uses the five rotating messages beginning at ID 0x70000; its drawer
submits the whole title/body block at a fixed left margin. Reducing spaces made
its visible block narrower while keeping that margin.

`ios/korean_text_layout.c` now operates after the existing drawer finishes
measuring all lines. It calculates one bounding span across the line origins and
widths, then shifts every line by the same delta to center the complete block in
the original 720-pixel UI plane. It preserves explicit line breaks, indentation,
blank lines, all Y coordinates, and the original state's anchor. The loading
story caller is recognized explicitly, including its pinned Korean trampoline
frame. The shared path also covers other wide, multi-line Korean prose at the
page's left margin. It excludes single-line items, already centered/right-aligned
text, right-hand description columns, small HUD text and footer hints. The
specific scope guards are in the helper; this is not a change to every text box.

The separate public text measurer previously retained the old space width even
though the draw function's measuring and rendering passes used quarter-cell
spaces. Both ASCII and full-width spaces now use that same advance there too.
This also keeps layouts that ask for measurements before drawing consistent.

The font-1 preprocessing path converts Latin punctuation into full-width JIS
cells. The character's picture can therefore have large side bearings even
when the resource string contains no space before it. For period, comma,
colon, semicolon, question mark and exclamation mark, the helper reads the
current original glyph window and its texture's actual ink bounds. It moves
the original quad's pen to remove excessive left padding, and uses a compact
advance with space for the complete ink and a small right margin. No glyph
pixels, UVs, font weight, size or texture data are edited. The helper supports
the original A8, palette, ARGB and ARGB4444 glyph textures; unsupported formats
retain original behavior. Bounds are cached by texture and selected glyph
window. Explicit extra spaces immediately before punctuation use zero advance;
normal word spaces retain their approved quarter-cell width.

All three relevant measuring/drawing paths use matching punctuation advances:
the public measurer, the draw function's measuring pass and both draw
continuations. Korean/Japanese full-width punctuation pairs and their ASCII
forms are recognized. Other language boots use the original functions, and
other font modes retain their original metrics.

Native synthetic regressions in `ios/korean_text_layout_test.py` replay GPU glyph
bounds in four texture formats at several scales, check that every ink pixel
fits the new advance, and prove the texture and UV data are unchanged. They also
check common paragraph translation, blank lines, scrolling loading prose and
the exclusions above. Strict code-generation guards require every measuring
and draw continuation to be present. The adapted private translation and host
helper are compiled locally before the production IPA build. Device visual
confirmation is still needed; these checks do not substitute for playing every
screen in the game.


### Preserve original auto-fit proportions after 2.73

The loading story caller uses the separate public measurer at return address
0x002053DF, then computes horizontal compression as `min(1, limit / width)`
at 0x00205529. This is independent of the later paragraph-origin translation.
Changing the public measured spaces and punctuation in 2.73 reduced `width`,
so the original caller compressed the glyphs less. The glyph pictures were not
resampled, but their drawn proportions became wider. That was an unintended
regression; the earlier claim that no size changed missed this caller effect.

Restore the separate public measurer byte-for-byte to its pre-2.73 metrics for
all callers, including other auto-fit screens. Keep compact punctuation and
approved spaces in the drawer's own measuring pass and rendering continuations;
these place quads after the caller has selected its original compression.
Paragraph centering still translates origins only. The code-generation test
now requires the public measurer to remain unmodified, rather than requiring
it to return compact widths. This supersedes the public-measurer alignment
claims above. Native checks do not establish first-run loading duration.


### First-run loading investigation

The iOS bootstrap explicitly retains the original title's AFS cache under
`saves-korean/Cache`, with shared `UserData` and `TitleData`. Successful Korean
archive rebuilding invalidates only that disposable cache; an unchanged capsule
returns before invalidation. A fresh install or an asset-changing upgrade can
therefore cause original cache preparation again. The shader warmup in the
available older device log finished in 0.2 seconds; that is not evidence for
attributing the reported 60-second wait to shaders.

The story drawer belongs to the original loading state machine. Its stage 3
also checks controller input before the fade-out, so a visible story screen is
not by itself proof that disk work is still pending. Neither observation proves
which condition dominates the user's current first-run delay.

For the next explicitly diagnostic build, the existing optional recorder adds
`ui-load-state` (stage at 0x8DB160, local tick counter at 0xBCDFDC, global frame
at 0xD21734, gate byte at 0xD52F2D) and `ui-load-font` (horizontal/vertical scale
float bit patterns, packed anchor, line count) to `ui-layout.log`. It samples
once per second or at a stage transition, with limits of 180 seconds and 240
samples. The observation never changes loading state, calls file I/O from the
original title, or edits saves. It is absent when movie diagnostics are disabled.
A fresh first-run trace is needed before changing loading behavior or claiming
a duration fix. Do not delete user saves to obtain that trace.
