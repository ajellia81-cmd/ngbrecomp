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
