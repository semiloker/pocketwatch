# M4 — the native look: PGF fonts, system UI metrics, buttons, language

Question: what must Pocketwatch implement to draw text and layouts that match the PSP's own UI,
using only what is on the user's PSP?
Status: Answered by source research (2026-10-16); XMB metrics and glyph coverage **unverified on
the device**.

Labels: **VERIFIED** = read in source code (link) or checked by decoding a test file, **INFERRED**
= taken from a reimplementation that measured the real UI, **GUESS** = unconfirmed.

## 1. PGF font format (VERIFIED)

Sources: PPSSPP [PGF.h L131-L198](https://github.com/hrydgard/ppsspp/blob/master/Core/Font/PGF.h#L131-L198),
[PGF.cpp L466-L583](https://github.com/hrydgard/ppsspp/blob/master/Core/Font/PGF.cpp#L466-L583) (glyph reader),
[L685-L708](https://github.com/hrydgard/ppsspp/blob/master/Core/Font/PGF.cpp#L685-L708) (RLE) —
"GPL 2.0 or later", parts from JPCSP; JPCSP
[SceFontInfo.java](https://github.com/jpcsp/jpcsp/blob/master/src/jpcsp/HLE/kernel/types/SceFontInfo.java#L232)
(GPL-3.0-or-later, more complete: shadow and overlay glyphs). Both may be ported with attribution.
The decoder below was checked by decoding 'A' and 'g' from ARK-4's ttf2pgf-generated
`extras/menus/arkMenu/FONT.PGF` (revision 2). No Sony PGF was available on the PC: the header
values of the real ltn/jpn files are still to be logged on the device.

Header (little-endian, 392 bytes in revision 2; "26.6" = fixed point / 64):

| Offset | Field |
|---|---|
| 0x00 | u16 headerOffset (0), u16 headerSize (392) |
| 0x04 | "PGF0" |
| 0x08 | s32 revision (2 or 3), 0x0C s32 version (6) |
| 0x10 | s32 charMapLen, 0x14 s32 charPtrLen (= glyph count), 0x18 s32 charMapBpe, 0x1C s32 charPtrBpe |
| 0x22 | u8 bpp (4) |
| 0x24 | s32 hSize, vSize, hRes, vRes (26.6: points and dpi) |
| 0x35 / 0x75 | char fontName[64] / fontType[64] |
| 0xB6 | u16 firstGlyph, u16 lastGlyph |
| 0xD4 | s32 maxAscender, maxDescender, maxLeftX, maxBaseY, minCenterX, maxTopY (26.6) |
| 0xEC / 0xF4 | s32 maxAdvance[2] / maxSize[2] |
| 0xFC | u16 maxGlyphW, maxGlyphH (pixels) |
| 0x102 | u8 dimLen, xAdjLen, yAdjLen, advLen |
| 0x16C / 0x170 / 0x178 | s32 shadowMapLen / shadowMapBpe / shadowScale[2] |

Revision 3 adds 20 bytes at 0x188: s32 compBpe1, u16 compLen1, 2 pad, s32 compBpe2, u16 compLen2,
6 pad. After the header: tables dim, xAdj, yAdj, adv (entries `{s32 H, s32 V}`); shadowMap (bit
stream); rev 3 only compTable1/2 (`{u16 start, u16 count}`); charMap; charPtr; fontData (rest of the
file). Each bit stream holds `len*bpe` bits padded to 4 bytes (`((len*bpe+31)&~31)/8` bytes).

```
bits(n): LSB first; bit k = (byte[k>>3] >> (k&7)) & 1; value assembled low bit first
lookup(c): missing if c < firstGlyph or c > lastGlyph
   idx = charMap[c - firstGlyph]; missing if idx >= charPtrLen
   (rev 3: JPCSP maps c through the compTable1 ranges first; PPSSPP ignores it - log the revision)
p = charPtr[idx] * 32                      ; bit offset into fontData
size = bits(14); w = bits(7); h = bits(7)
left = sext7(bits(7)); top = sext7(bits(7)) ; sext7(v) = v >= 64 ? v - 128 : v
flags = bits(6); skip 2 + 2 + 3; shadowId = bits(9)
for (bit, table) in (0x04 dim), (0x08 xAdj), (0x10 yAdj), (0x20 adv):
   flags & bit ? i = bits(8) -> table[i].{H,V} : H = bits(32), V = bits(32)
bitmap follows (byte-aligned)
flags & 3: 1 = rows (i = y*w + x), 2 = columns (i = x*h + y),
           3 = overlay: up to 3 u16 char codes; draw each at (sub.left - left, top - sub.top)
RLE of 4-bit values until w*h pixels:
   n = bits(4); if n < 8: v = bits(4), emit v (n+1) times; else emit bits(4), (16-n) times
```

Shadow glyph at `p + size*8`: header `14 size, 7 w, 7 h, 7 left, 7 top, 6 flags`, RLE from +48 bits
(PPSSPP omits the 6-bit flags; JPCSP and the test decode agree on including it). `shadowMap[shadowId]`
is the character code that owns the shadow. Drawn scaled by `64/shadowScale` (2x for 32).

Drawing: top-left = `(pen >> 6) + left`, `baseline - top`; `pen += advH` (26.6). Alpha `a * 17`.
Line height from maxAscender/maxDescender. No kerning table.

## 2. Font files on 6.61

From PPSSPP's table ([sceFont.cpp L98-L121](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceFont.cpp#L98-L121)), VERIFIED as
a list: `jpn0.pgf` ("FTT-NewRodin Pro DB", 1.58 MB), `ltn0`..`ltn15.pgf`, `kr0.pgf`
("AsiaNHH(512Johab)"), `gb3s1518.bwfon` (Chinese, other format), `arib.pgf`, `imagefont.bin`.

| Files | Style order (per group of 8) | Size |
|---|---|---|
| ltn0-7 | sans, serif, sans italic, serif italic, sans bold, serif bold, sans bold italic, serif bold italic (sans "FTT-NewRodin Pro Latin", serif "FTT-Matisse Pro Latin") | 10.125 pt at 128 dpi = **18 px em**; ltn0 69 KB |
| ltn8-15 | same order | 7 pt = **12.44 px em**; ltn8 41 KB |

The XMB draws Latin text with ltn0 (INFERRED: replacing ltn0.pgf changes the XMB font,
[techenclave guide](https://techenclave.com/t/guide-to-customize-psp-ui/86287); VitaXMB treats it as
the UI font); Japanese with jpn0. `imagefont.bin`'s PSP format is not publicly reversed
([libvita2d#55](https://github.com/xerpi/libvita2d/issues/55)): do not depend on it.

Button glyphs: jpn0 very likely has ○ U+25CB, △ U+25B3, □ U+25A1, × U+00D7 (GUESS); ltn0 may stop
at U+2265 and lack them (GUESS). The system's own button pictures are `tex_cross`/`tex_circle`/...
in `system_plugin_fg.rco` ([m4-native-icons](m4-native-icons.md)).

## 3. System UI metrics

**(a) In-game HOME screen:** no published metrics or screenshots found. Nearest model, PPSSPP's
message dialog ([PSPDialog.cpp L435-L560](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/PSPDialog.cpp#L435-L560)):
full-screen dim `0x20000000` with a 200 ms fade; 1 px white lines x 60-420 above and below the text
centred on y 135; Yes at x 204, No at x 273, the selected one in a 30x16 box `0x6DCFCFCF`; white
text with a black shadow (alpha 0x80) at (+1, +2). PPGe colours are 0xAABBGGRR; its atlas font is
24 px, so scales 0.5/0.55/0.6 = 12/13.2/14.4 px. **Best evidence: capture the real HOME screen on
the device** (debug framebuffer dump, never committed).

**(b) XMB** (INFERRED from VitaXMB, GPL-3.0, which measured PSP captures and 6.60/6.61 paf/vshmain
constants: [layout.h L9-L16](https://github.com/dekiller82/VitaXMB/blob/HEAD/src/ui/layout.h#L9-L16),
`src/theme/psptheme.h` L40-L70, L505-L560):

| Element | Value |
|---|---|
| Category bar | 80 px pitch, +5 px each side of the open category; open icon centre ≈ (109, 70) |
| Bar x offset | −130 from the screen centre at home, −190 with a list open, −240 one level deeper |
| Timing | category change 200 ms; dialogs slide in 100 ms |
| Item list | selected row centre y ≈ 135, item icon centre x ≈ 104, text left x ≈ 150, row pitch ≈ 65 px; stock icons 64x64 drawn scaled |
| Text | item title ≈ 14 px, sub-text ≈ 11 px (ltn0 scaled ≈ 0.8 and 0.63); shadow offset (2, 2.5) |
| Status | battery centred (463, 12), 44x16; clock anchor ≈ (443, 13); mute icon 5 px left of it |

**(c) Saved Data Utility** (PPSSPP [PSPSaveDialog.cpp L380-L663](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/PSPSaveDialog.cpp#L380-L663)):
top banner y 0-23 in `0x65636358`, title at (30, 11) 14.4 px; selected icon 144x80 at (27, 97),
others 81x45 at x 58.5, y above `97 − 13 − 45k`, below `97 + 48 + 45k`; selected border 1.2 px
`0xD0FFFFFF`, icons over black; details: 1 px white rule from (180, 136) to the right edge, title
bottom-aligned at (180, 136) in #C0C0C0, date and size at (180, 137) 12 px, save title at
(175, 159), wrapped detail at (175, 181) 305 px wide; button hints
([PSPDialog.cpp L373-L405](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/PSPDialog.cpp#L373-L405)):
11.5 px icons at x 183.5 (×) and 261.5 (○), y 256, labels at x + 14.5, y 252.

## 4. Buttons (Enter = ○ or ×)

Region-dependent, stored in the registry `/CONFIG/SYSTEM/XMB/button_assign` (0 = ○ confirms,
Japan/Asia; 1 = × confirms); ARK's VSH menu "swap" toggles it
([registry.c L73-L80](https://github.com/PSP-Archive/ARK-4/blob/main/extras/menus/vshmenu/registry.c#L73-L80)).
Kernel read: `sceImposeGetLanguageMode(int *lang, int *button)` (sceImpose_driver 0x24FD7BCF →
0x842309DD, in our NID table), same 0/1 meaning. PPSSPP always draws × left and ○ right and swaps
only the labels.

## 5. Language

`sceImposeGetLanguageMode` (no file I/O); alternatives `sceImposeGetParam(0x80)` or the registry
`/CONFIG/SYSTEM/XMB/language`. Codes: 0 ja, 1 en, 2 fr, 3 es, 4 de, 5 it, 6 nl, 7 pt, 8 ru, 9 ko,
10 zh-Hant, 11 zh-Hans. Log impose and registry values once on the device to confirm they agree.

## 6. Recommended approach (PROPOSED)

- Fonts: ltn0 for titles and items, ltn8 for hints and secondary text; for an exact XMB match
  downscale ltn0 glyphs once (area averaging) into a cache at ≈ 14.5 px and ≈ 11.4 px. ○×△□ from
  the RCO button pictures (or jpn0 glyphs read once). kr0 for Korean; Chinese later.
- Look: white text, soft black shadow ~50 % alpha at (+2, +2); dim overlay, 200 ms fade, 100 ms
  slide-in; translucent light selection box and thin white separators; button hints at the bottom
  in the impose button order.
- Minimum decoder: rev-2 header, the four metric tables, charMap, charPtr, glyph header, RLE,
  row/column order, overlay glyphs (accents). Integer pen. Later: shadow glyphs, rev 3, sub-pixel.
- Load fonts at plugin start or just before the freeze, never in suspend paths; ~110 KB.

## 7. To verify on the device

Header values of ltn0/ltn8/jpn0 (revision, sizes, glyph ranges); whether ltn0 has ○×△□; the impose
language and button values vs the registry; a framebuffer capture of the real HOME screen, the XMB
and the Saved Data Utility for side-by-side comparison (kept off the repository).
