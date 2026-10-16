# Rendering and native assets — specification

| | |
|---|---|
| Area code | `GFX` |
| Status | Done: overlay framebuffer in P11, canvas primitives (fill, blend, 8x8 text, frame conversion). Planned: PGF text, RCO/RLZ/GIM images, BMP month colours, animation, glyph/icon caches |
| Milestones | M3 phase 1 (done), M4 |
| Code | [`ui/fb.c`](../../plugin/src/ui/fb.c), [`ui/draw.c`](../../plugin/src/ui/draw.c), [`ui/font8x8.c`](../../plugin/src/ui/font8x8.c) (MIT, PSPFatSave) |
| Research | [m4-native-look](../research/m4-native-look.md), [m4-native-icons](../research/m4-native-icons.md), [m3-menu-e3](../research/m3-menu-e3.md) |

## A. Overview

- **Purpose:** put pixels on screen over the frozen game without touching the game's framebuffer,
  and turn the PSP's own font and image files into drawable glyphs and icons.
- **Current state (VERIFIED, 2026-10-16):** three 8888 pages (stride 512) in P11 — background,
  two draw pages — shown with `sceDisplaySetFrameBufferInternal(0, ...)` and released with a NULL
  address; the game's frame (565/5551/4444/8888) is read through the uncached alias and dimmed into
  the background once; each frame starts as a copy of the background.
- **Target state:** text in the system fonts, the XMB's icons and button pictures, simple
  animation (fades, slides), all with fallbacks.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `GFX-REQ-001` | The overlay uses its own pages and never writes the game's framebuffer or VRAM. | P0 | Done | code, device test |
| `GFX-REQ-002` | The overlay is released (display back to the game) before any suspend and redrawn after. | P0 | Done | device test 2026-10-16 |
| `GFX-REQ-003` | The game's frame is captured in any of the four display formats, dimmed, as the background. | P0 | Done | host test `draw_from_frame_converts_formats` |
| `GFX-REQ-004` | A PGF decoder (revision 2 and 3 headers, metric tables, char map, RLE rows/columns, overlay glyphs) renders a UTF-8 string with 4-bit alpha into the canvas; every read is bounds-checked against the file size. | P0 | Planned | host tests with synthetic PGF files |
| `GFX-REQ-005` | Glyphs are cached per font and size (including XMB-scale downscaled variants ≈ 14.5 px and ≈ 11.4 px). | P1 | Planned | host test, timing on device |
| `GFX-REQ-006` | An RCO reader finds an image by label or index; an RLZ decoder and a GIM decoder (idx8 + 8888 palette + unswizzle first; 4444/5551/565/idx4 later) turn it into an 8888 bitmap with alpha. | P1 | Planned | host tests (RLZ round trip against RCOMage's compressor, synthetic GIMs) |
| `GFX-REQ-007` | zlib-compressed RCO images (themes) decode through the firmware's `sceKernelDeflateDecompress` (UtilsForKernel 0xE8DB3CE6). | P2 | Planned | device test with a theme |
| `GFX-REQ-008` | The month background colour comes from `flash0:/vsh/resource/01-12*.bmp`. | P2 | Planned | device review |
| `GFX-REQ-009` | Every asset read happens before the game is frozen (or at plugin start), never in a suspend path; flash0 only (the Memory Stick rule is untouched). | P0 | Planned | code review |
| `GFX-REQ-010` | Any missing or malformed asset falls back (8x8 font scaled, drawn icons) and logs the reason once. | P0 | Partial (8x8 only) | debug build with the paths broken |
| `GFX-REQ-011` | Alpha-blended drawing of glyphs and images, a soft text shadow, rounded/translucent panels, and linear fades/slides timed by the system clock. | P1 | Planned | golden images |
| `GFX-NFR-001` | Full-screen redraw (background copy + UI) ≤ 8 ms at 333 MHz; open-to-first-frame ≤ 300 ms including asset loading from the cache. | P1 | Planned | log timings |
| `GFX-NFR-002` | Asset caches ≤ 512 KB (fonts ≈ 110 KB, tables ≈ 10 KB, icons ≈ 15 KB each in 8888), in P11/P9, freed on close or kept for the boot only if a state is not using P9. | P1 | Planned | log, `make check` |

## C. Architecture and integration

| Module (PROPOSED path) | Portable? | Role |
|---|---|---|
| `ui/fb.c` (today) → `gfx/fb.c` | no | pages, capture, present, hide |
| `ui/draw.c` → `gfx/canvas.c` | yes | fill, blend, blit with alpha, shadow, frame conversion |
| `gfx/pgf.c` | yes | parse PGF from a memory buffer, glyph lookup and RLE decode |
| `gfx/text.c` | yes | UTF-8 → glyphs, layout (width, ellipsis, wrap), cache |
| `gfx/rlz.c`, `gfx/rco.c`, `gfx/gim.c`, `gfx/bmp.c` | yes | asset formats |
| `gfx/assets.c` | no | reads files from flash0 into P11/P9 at the right moment; fallbacks |
| `gfx/anim.c` | yes | time-based tweening |

Portable modules take byte buffers, so host tests run them on synthetic files produced by our own
encoders (a small Python PGF writer and RCOMage's LGPL RLZ compressor in `tests/fixtures/`); no Sony
file enters the repository ([ADR-013](../architecture/decisions.md#adr-013--sony-assets-are-read-at-runtime-never-redistributed)).

Memory: pages 3 x 544 KB in P11 (today); caches next to them. P11 is overwritten during a suspend,
so caches in P11 are invalidated by every save/load and re-read (cheap: ~15 KB of flash0 reads),
or kept in P9 when no state occupies it.

## D. User experience and visual design

See the [UI spec](README.md#d-user-experience-and-visual-design). Rendering-specific: text is white
with a black shadow (~50 % alpha, offset +2,+2 at XMB scale), icons drawn with their own alpha, the
selected item may get a generated glow (the XMB has no glow texture to read, per research).

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| PPSSPP PGF | [PGF.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/Font/PGF.cpp) | decoder structure | port with attribution | GPL-2.0-or-later |
| JPCSP SceFontInfo | [SceFontInfo.java](https://github.com/jpcsp/jpcsp/blob/master/src/jpcsp/HLE/kernel/types/SceFontInfo.java) | shadow and overlay glyphs | port the logic | GPL-3.0-or-later |
| RCOMage | [RCOMage](https://github.com/kakaroto/RCOMage) | RCO structs, RLZ compressor | port structs; mirror the compressor for a decoder | LGPL-2.1 (GPL-compatible via §3) |
| Puyo Tools GIM | [GimTextureDecoder.cs](https://github.com/nickworonekin/puyotools/blob/master/src/PuyoTools.Core/Textures/Gim/GimTextureDecoder.cs) | GIM formats, swizzle | logic | MIT |
| VitaXMB | [VitaXMB](https://github.com/dekiller82/VitaXMB) | RCO reader, XMB metrics, glow generation | ideas, measured constants | GPL-3.0 (assets excluded) |
| libLZR, intraFont | — | — | **avoid** (CC BY-SA) | CC BY-SA |

## F. Implementation plan

1. `gfx/pgf.c` + `gfx/text.c` with a synthetic-PGF test generator; on the device log the header of
   ltn0/ltn8/jpn0 once.
2. `gfx/rlz.c` (decoder mirroring `rlzpack.c`), `gfx/rco.c`, `gfx/gim.c`; debug dump of
   `topmenu_icon.rco` labels as BMPs to identify the icons (never while a state is held).
3. Canvas: alpha blit, shadow, panel; `anim.c`.
4. `assets.c`: load before the freeze, cache policy, fallbacks.

## G. Testing and acceptance

Host: PGF glyph decode against expected bitmaps from the synthetic font; RLZ round trip on random
data; GIM unswizzle on synthetic images; golden-image renders of each widget. Device: header logs,
icon dump, timing logs, side-by-side captures.

## H. Risks and decisions

[RISK-010](../risk-register.md#risk-010--parsing-sony-assets-on-the-device), [RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth); [ADR-011](../architecture/decisions.md#adr-011--own-overlay-framebuffer-never-the-games), [ADR-013](../architecture/decisions.md#adr-013--sony-assets-are-read-at-runtime-never-redistributed). Open: 6.61 RCO compression
flags (expected RLZ, 0x0C = 0x20); `system_plugin_fg.rco` compression (zlib per VitaXMB, to check);
whether ltn0 contains ○×△□.
