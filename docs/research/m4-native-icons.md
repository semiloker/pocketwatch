# M4 — the XMB's own icons from flash0 (RCO / GIM)

Question: can the hub show the XMB's icons, read at runtime from the user's PSP, and what must
Pocketwatch implement for it?
Status: Answered by source research (2026-10-16); **device verification pending**.

Labels: **VERIFIED** = confirmed in source code or by decoding stock 6.60 files during the
research (the files were not kept and are never added to the repository), **GUESS** = unconfirmed.

## 1. The RCO container (VERIFIED)

Sources: RCOMage [rcofile.h L27-L178](https://github.com/kakaroto/RCOMage/blob/master/src/rcofile.h#L27-L178),
[rcoreader.c L198-L249, L785-L880](https://github.com/kakaroto/RCOMage/blob/master/src/rcoreader.c#L198-L249)
(**LGPL-2.1**; §3 allows applying GPL-2.0-or-later, so porting with attribution is compatible with
GPL-3.0-or-later). Compact reader: VitaXMB
[rco.h L120-L217](https://github.com/dekiller82/VitaXMB/blob/main/src/theme/rco.h#L120-L217)
(GPL-3.0; rejects RLZ files).

- Header 0xA4 bytes, little-endian, magic `\0PRF`, version 0x100. Word at 0x0C: upper nibble =
  table compression (0 none, 1 zlib, 2 RLZ). Offsets: 0x10 main table, 0x24 image table, 0x30
  object tree, 0x34 animation tree, 0x40/0x44 label data, 0x80/0x84 image data.
- Packed table area: at 0xA4 `{packedLen, unpackedLen, longestText}`, stream at 0xB0. The unpacked
  bytes stand in at offset 0xA4 (table offsets refer to header + unpacked layout); `pImgData` is a
  real file offset (`ALIGN4(0xB0 + packedLen)`).
- Entry (0x28 bytes): `u16 typeId` (0x0401 = image), `u16 0`, `u32 labelOff`, `u32 0x28`, `u32 0`,
  `u32 numSub`, `u32 next`, `u32 prev`, `u32 parentOff`, 2 x 0.
- Image table: head entry's `numSub` = image count; each child adds
  `{u16 format (5 = GIM), u16 compr, u32 packed, u32 off (from pImgData), u32 unpacked}`, the last
  only when compressed; walk children by `next`.
- Labels: in `topmenu_*` the 6.6x firmware uses sequential two-letter codes (AJ..GF);
  `system_plugin_fg.rco` keeps names (`tex_cross`, `tex_circle`, `tex_triangle`, `tex_square` +
  `_shadow`, `tex_battery`, `tex_busy`).

## 2. Compression: RLZ, not zlib (VERIFIED on 6.60 files)

`topmenu_icon.rco` (227,684 B) and `topmenu_plugin.rco` (38,716 B) — the same sizes as 6.61
([consolespec 661.mtree](https://github.com/SnowflakePowered/consolespec/blob/master/definitions/partitionspec/SONY_PSP/flash0/661.mtree))
— compress the tables **and every image** with RLZ (compression 2); none is zlib. 6.61's kernel
no longer exports an RLZ decoder (`sceKernelLzrcDecode` 0x7DD07271 is missing from the 6.60
sysmem exports; PRXDecrypter loads an old sysmem to get it,
[main.c L79-L119](https://github.com/smiky/psptools/blob/master/PRXDecrypter/prxdecrypter_01g/main.c#L79-L119)).
**Pocketwatch needs its own RLZ decoder.**

| Candidate code | License | Usable? |
|---|---|---|
| libLZR ([libLZR.c](https://github.com/PSP-Archive/libLZR/blob/main/libLZR.c#L1-L9)) | CC BY-SA 3.0 | **No** |
| benderthemes `psp_lzr.py` (port of libLZR) | none / derived | **No** |
| PPSSPP `tlzrc.cpp` | GPL-2.0-or-later | different model, failed on these streams in the research test |
| RCOMage `rlzpack.c` (compressor reverse-engineered from Sony's tool) | LGPL-2.1 | **Yes**: write the decoder as its mirror, host-test by round-tripping its output on random data |

Firmware deflate exists for zlib images (custom themes): `sceKernelDeflateDecompress`,
`UtilsForKernel` NID 0xE8DB3CE6 (not randomized on 6.60 per uOFW
[exports.exp](https://github.com/uofw/uofw/blob/master/src/kd/sysmem/exports.exp#L302-L376)),
`int (u8 *dst, u32 dstSize, const void *src, void **next)`, raw deflate
([utils.c L481-L503](https://github.com/uofw/uofw/blob/master/src/kd/sysmem/utils.c#L481-L503));
Adrenaline's inferno calls it on 6.61. Gzip variant 0x78934841. Needs an allowlist entry.

## 3. The GIM image format (VERIFIED)

Sources: Puyo Tools [GimTextureDecoder.cs](https://github.com/nickworonekin/puyotools/blob/master/src/PuyoTools.Core/Textures/Gim/GimTextureDecoder.cs#L148-L192) (MIT),
VitaXMB [psptheme.h L142-L199](https://github.com/dekiller82/VitaXMB/blob/main/src/theme/psptheme.h#L142-L199).

- `MIG.00.1PSP\0` + 4 bytes, then chunks `{u16 id, u16 0, u32 size incl. children, u32 next,
  u32 dataOff = 0x10}`: 02 root, 03 picture (containers), 04 image, 05 palette, FF info. The palette
  can come before the image: walk, do not assume order.
- Data header (from chunk + 0x10): +4 format (0 565, 1 5551, 2 4444, 3 8888, 4 idx4, 5 idx8, 6/7
  idx16/32, 8-10 DXT), +6 order (1 = swizzled), +8 width, +0xA height, +0xC bpp, +0xE pitch align
  (16), +0x10 height align (8), +0x1C pixel start (0x40), +0x20 pixel end.
- Swizzle: 16-byte x 8-row blocks in block-row order; pitch `ALIGN16(w*bpp/8)`, rows `ALIGN8(h)`.
- Colours are the PSP's native ABGR (8888 bytes R,G,B,A = our framebuffer layout).
- **All stock icons are idx8, swizzled, 256-entry 8888 palette**, trimmed to content (category
  icons 51x41 .. 61x45, items ~68x52, largest 90x74).

## 4. Which files hold what

| File (flash0:/vsh/resource/) | Size | Content |
|---|---|---|
| `topmenu_icon.rco` | 227,684 B | all 153 XMB icons. Categories (VERIFIED, two sources agree): AJ Settings, AK Extras, AL Photo, AM Music, AN Video, AO Game, AP Network, AQ PSN. Items (GUESS, sources disagree): BH UMD, BI Memory Stick, BJ saved data, BX system settings, CA power save or exit, CF display or "PSP" |
| `topmenu_plugin.rco` | 38,716 B | xmenu layout (bar position, icon scale, 80 px pitch), `tex_parental` + shadow/focus variants |
| `system_plugin_fg.rco` | 34,336 B | button glyphs ○ × △ □ (+ shadows), battery, busy |
| `system_plugin_bg.rco` | 8,380 B | wave model(s), colour animation ([VitaXMB bgscene.h](https://github.com/dekiller82/VitaXMB/blob/main/src/theme/bgscene.h)) |
| `01-12.bmp` (74,112 B), `01-12_03g.bmp`, `13-27.bmp` | | month sky colours (twelve 60x34 images); `_03g` probably the PSP-3000 version, `13-27` the Colour setting (GUESS) |
| `impose_plugin.rco` | 69,540 B | XMB-side overlays; in a game the kernel impose draws from `flash0:/kd/resource/impose.rsc` (113,676 B, undocumented) (GUESS) |

No per-icon glow texture (GUESS); VitaXMB generates its own. VitaXMB's approximation of the XMB
animation (not Sony documentation): a selected category icon grows from 49 to 60 px, alpha 165 →
255, 80 px pitch ([main.c ~L886](https://github.com/dekiller82/VitaXMB/blob/main/src/main.c#L886-L897)).

## 5. Cost and safety

Reading only what is needed: header 0xB0 B + ~1.9 KB tables (→ 9.9 KB unpacked), then 0.5-3 KB per
icon (→ 2-9 KB GIM). Six icons ≈ 15 KB of reads, ~10 KB tables + 9 KB scratch + ~5-15 KB per icon,
RLZ state ~2.8 KB; a few tens of ms (GUESS). **Do the flash0 I/O before freezing the game** (at
plugin start or just before the freeze when the hub opens): a frozen game thread may hold the
flash0/fatms locks. Never in a suspend path. Reading flash0 does not violate the Memory Stick rule.

## 6. Recommended approach (PROPOSED)

1. Portable RCO reader (subset of RCOMage's structs, attributed).
2. Own RLZ decoder, mirror of `rlzpack.c`; host-tested by round trip on random data (no Sony file
   in the repository).
3. GIM decoder: idx8 + 8888 palette + unswizzle first; 4444/5551/565/idx4 for themes.
4. Icons from `topmenu_icon.rco`, button glyphs from `system_plugin_fg.rco`, month colour from
   `01-12*.bmp`; glow generated procedurally.
5. Optional: zlib images via 0xE8DB3CE6 (allowlist entry citing uOFW and Adrenaline).
6. Fallback: Pocketwatch's own drawn icons when anything fails.

## 7. To verify on the device

- 6.61's header word at 0x0C is 0x20 and image compression is 2.
- Which label is which icon: a one-time debug dump of all 153 icons as BMPs to the card (never
  while a state is held).
- Theme redirection (CXMB) of `topmenu_icon.rco`: themed files may be flat or zlib.
- The import self-check resolves 0xE8DB3CE6.
- Time for read + RLZ + decode; whether `01-12_03g.bmp` is the PSP-3000's.
